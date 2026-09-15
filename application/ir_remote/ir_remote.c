#include "ir_remote.h"
#include "malloc.h"
#include "string.h"
#include "./SYSTEM/usart/usart.h"

/* 设备 → 主机：握手应答。设备固件历史约定故意拼错（Application 少一个 p），
   必须按错误拼写逐字节匹配，不要"纠正"它。 */
static const char ir_link_reply[] = "Play Aplication";

/* 主机 → 设备：握手请求 5A A3 97 0B 09 "Please Link" B0 A5（18 字节，静态只读） */
static uint8_t ir_link_frame[] = {
    0x5A, 0xA3, 0x97, 0x0B, IR_REMOTE_INDEX_LINK,
    'P', 'l', 'e', 'a', 's', 'e', ' ', 'L', 'i', 'n', 'k',
    0xB0, 0xA5
};

/* 主机 → 设备：切换颜色 5A A3 97 01 D1 <state> <crc> A5（8 字节）。
   crc = 0x66 + state，即给定 4 个状态时分别为 0x66/0x67/0x68/0x69。 */
static uint8_t ir_color_frame[] = {
    0x5A, 0xA3, 0x97, 0x01, IR_REMOTE_INDEX_SET_COLOR, 0x00, 0x66, 0xA5
};

static uint8_t ir_frame_checksum(const uint8_t *frame, uint16_t length)
{
    uint32_t checksum = 0;

    for(uint16_t i = 0; i < length; i++)
    {
        checksum += frame[i];
    }
    return (uint8_t)(checksum & 0xFFU);
}

DEV_IR_REMOTE *read_ir_remote(void *self)
{
    return (DEV_IR_REMOTE *)self;
}

DEV_IR_REMOTE *create_ir_remote(void)
{
    DEV_IR_REMOTE *ir_remote = mymalloc(SRAMIN, sizeof(DEV_IR_REMOTE));

    if(ir_remote == NULL)
    {
        return NULL;
    }

    memset(ir_remote, 0, sizeof(DEV_IR_REMOTE));
    ir_remote->base.type = SENSOR_TYPE_IR_REMOTE;
    memset(ir_remote->base.name, 0, sizeof(ir_remote->base.name));
    strcpy(ir_remote->base.name, "ir_remote");
    ir_remote->state = 0;

    return ir_remote;
}

/* 0xED 上报：payload 是 2 字节 packed {state, bat}，没有 version 字段。
   bat 只参与合法性校验（本硬件恒为 0xFF），不保存、不上报；
   state 是主机最后下发的有效命令态，不代表接收端已收到红外信号。 */
void refsh_ir_remote(void* self, void* data)
{
    DEV_IR_REMOTE *ir_remote = (DEV_IR_REMOTE *)self;
    _AGREEMENT *frame = (_AGREEMENT *)data;

    if(ir_remote == NULL || frame == NULL)
    {
        return;
    }

    if(frame->index != IR_REMOTE_INDEX_UPLOAD || frame->length != 2U)
    {
        return;
    }

    if(frame->data[0] > 3U)
    {
        return;
    }

    if(frame->data[1] > 100U && frame->data[1] != 0xFFU)
    {
        return;
    }

    ir_remote->state = frame->data[0];
}

bool ir_remote_frame_is_ir(const _AGREEMENT *frame)
{
    if(frame == NULL || frame->sID != DEVICE_IR_REMOTE_ID)
    {
        return false;
    }

    /* 强特征 1：握手应答 index=0x09、长度 15、payload="Play Aplication" */
    if(frame->index == IR_REMOTE_INDEX_LINK &&
       frame->length == (uint16_t)(sizeof(ir_link_reply) - 1U) &&
       memcmp(frame->data, ir_link_reply, sizeof(ir_link_reply) - 1U) == 0)
    {
        return true;
    }

    /* 强特征 2：上报 index=0xED、2 字节载荷且取值合法。
       超声波同用 0xED，但载荷是 ASCII "<cm>/<dt>"（至少 3 字节），不会混淆。 */
    if(frame->index == IR_REMOTE_INDEX_UPLOAD && frame->length == 2U)
    {
        if(frame->data[0] <= 3U &&
           (frame->data[1] <= 100U || frame->data[1] == 0xFFU))
        {
            return true;
        }
    }

    return false;
}

bool ir_remote_port_is_ir(uint8_t port)
{
    SensorBase *base;

    if(port > 3U)
    {
        return false;
    }

    base = hub_port[port].sensors;
    return (base != NULL && base->type == SENSOR_TYPE_IR_REMOTE);
}

bool ir_remote_set_color(int port, int state)
{
    UART_HandleTypeDef *huart;
    uint32_t start_tick;

    if(port < 0 || port > 3 || state < 0 || state > 3)
    {
        return false;
    }

    /* 端口内部类型必须是 IR_REMOTE，避免把超声波端口当 IR 使用 */
    if(!ir_remote_port_is_ir((uint8_t)port))
    {
        return false;
    }

    huart = getusartHandle((uint8_t)port + 1U);
    if(huart == NULL)
    {
        return false;
    }

    ir_color_frame[5] = (uint8_t)state;
    ir_color_frame[6] = ir_frame_checksum(ir_color_frame, 6U);

    /* 单帧 8 字节，115200 8N1 下 <1ms：用短时阻塞发送，避免异步缓冲生命周期
       与连续命令丢包问题。发送时**不**直接改监控状态，等设备下一帧 0xED 回显。 */
    start_tick = HAL_GetTick();
    while(huart->gState != HAL_UART_STATE_READY)
    {
        if((HAL_GetTick() - start_tick) >= 10U)
        {
            return false;
        }
    }

    return (HAL_UART_Transmit(huart, ir_color_frame,
                              (uint16_t)sizeof(ir_color_frame), 10U) == HAL_OK);
}

void ir_remote_handshake_scan(void *arg)
{
    (void)arg;

    for(uint8_t port = 0; port < 4U; port++)
    {
        UART_HandleTypeDef *huart;

        if(hub_port[port].sensors != NULL)
        {
            continue;   /* 已绑定：停止该端口握手，解绑后自动恢复 */
        }

        huart = getusartHandle(port + 1U);
        if(huart == NULL)
        {
            continue;
        }

        /* 只判断发送状态：端口 UART 的 DMA 接收一直在跑（RxState=BUSY_RX），
           不能直接用 HAL_UART_GetState()。忙则跳过本轮，下个周期重试。 */
        if(huart->gState != HAL_UART_STATE_READY)
        {
            continue;
        }

        HAL_UART_Transmit_IT(huart, ir_link_frame, (uint16_t)sizeof(ir_link_frame));
    }
}
