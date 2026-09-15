#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include "./SYSTEM/sys/sys.h"
#include "stdbool.h"
#include "deviceidentify.h"
#include "protocol.h"

/* 线端 ObjectID：IR_REMOTE 与超声波共用 0xA3（设备侧协议规定），
   对外（_os.get_port_linke / 监控）仍统一按 0xA3 上报，
   仅在本模块内部用私有类型区分，不暴露给 Python 或上位机。 */
#define DEVICE_IR_REMOTE_ID      0xA3
#define SENSOR_TYPE_IR_REMOTE    0x1A3

/* IR_REMOTE 协议 typeIndex（见 LBS-NEW-AI-SENSORD/Doc/IR_REMOTE_protocol.md） */
#define IR_REMOTE_INDEX_LINK      0x09
#define IR_REMOTE_INDEX_SET_COLOR 0xD1
#define IR_REMOTE_INDEX_UPLOAD    SENSOR_UPLOAD_INDEX

typedef struct
{
	 SensorBase base;
	 uint8_t state;   /* 设备回传的命令态：0=灭 1=红 2=绿 3=蓝（非执行回执） */
}DEV_IR_REMOTE;

DEV_IR_REMOTE *create_ir_remote(void);
DEV_IR_REMOTE *read_ir_remote(void *self);
void refsh_ir_remote(void* self, void* data);

/* 强特征判定：线端 0xA3 复用，只有以下两种帧能确定为 IR_REMOTE
   1) index=0x09 且 payload 逐字节等于 "Play Aplication"（固件故意拼错）
   2) index=0xED 且 payload 为合法 2 字节 {state,bat} */
bool ir_remote_frame_is_ir(const _AGREEMENT *frame);

/* 端口当前绑定的是否为 IR_REMOTE（内部类型校验，避免与超声波互相误用） */
bool ir_remote_port_is_ir(uint8_t port);

/* 下发颜色：state 0=灭 1=红 2=绿 3=蓝。
   端口不是已识别的 IR_REMOTE 或参数非法时静默忽略，返回 false。 */
bool ir_remote_set_color(int port, int state);

/* 100ms 握手扫描：仅对未绑定端口发送 "Please Link"，UART 忙时跳过本轮重试 */
void ir_remote_handshake_scan(void *arg);

#endif /* IR_REMOTE_H */
