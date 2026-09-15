 
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "protocol.h"
#include "deviceIdentify.h"
#include "blue.h"
#include "stdarg.h"
extern void usb_printf(char *fmt, ...);
static UART_HandleTypeDef g_uart1_handle;  /* UART??? */
static UART_HandleTypeDef g_uart2_handle;  /* UART??? */
static UART_HandleTypeDef g_uart3_handle;  /* UART??? */
static UART_HandleTypeDef g_uart4_handle;  /* UART??? */
static UART_HandleTypeDef g_uart5_handle;  /* UART??? */


static DMA_HandleTypeDef  UART1RxDMA_Handler;
static DMA_HandleTypeDef  UART2RxDMA_Handler;
static DMA_HandleTypeDef  UART3RxDMA_Handler;
static DMA_HandleTypeDef  UART4RxDMA_Handler;

static uint8_t usart1dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart2dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart3dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart4dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart5dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint16_t usart5dmaRxlength;
static uint16_t usart5_expected_length;
static bool usart5_frame_in_progress;
static uint32_t usart5_last_rx_tick;

static uint8_t blueTxBufer[1024];

#define BLE_FRAME_QUEUE_SIZE          8U
#define BLE_TX_PACKET_SIZE            1024U
#define BLE_TX_CONTROL_PACKET_SIZE    64U
#define BLE_TX_CONTROL_QUEUE_SIZE    6U
#define BLE_RX_GUARD_MS               8U
#define BLE_FRAME_TIMEOUT_MS         100U
#define BLE_TX_GUARD_MS               2U

/* 协议帧只在中断中入队，所有业务处理都在主循环完成。 */
static _AGREEMENT ble_frame_queue[BLE_FRAME_QUEUE_SIZE];
static volatile uint8_t ble_frame_queue_head;
static volatile uint8_t ble_frame_queue_tail;

/* UART5 单线时序由一个发送器统一管理：控制/OTA应答优先于监控。 */
static uint8_t ble_tx_active_buffer[BLE_TX_PACKET_SIZE];
static uint8_t ble_tx_control_queue[BLE_TX_CONTROL_QUEUE_SIZE][BLE_TX_CONTROL_PACKET_SIZE];
static uint16_t ble_tx_control_length[BLE_TX_CONTROL_QUEUE_SIZE];
static volatile uint8_t ble_tx_control_head;
static volatile uint8_t ble_tx_control_tail;
static volatile uint8_t ble_tx_control_count;
static uint8_t ble_tx_monitor_buffer[BLE_TX_PACKET_SIZE];
static uint16_t ble_tx_monitor_length;
static volatile bool ble_tx_monitor_pending;
static volatile bool ble_tx_busy;

/* 收包后留出保护时间，控制/OTA发送仍由独立优先级队列保证。 */
static volatile uint32_t ble_rx_quiet_until;
static volatile uint32_t ble_tx_quiet_until;

static bool ble_time_before(uint32_t now, uint32_t deadline)
 {
    return (int32_t)(now - deadline) < 0;
}

static void ble_rx_reset(void);

static void ble_rx_check_timeout(void)
{
    if (usart5_frame_in_progress &&
        (uint32_t)(HAL_GetTick() - usart5_last_rx_tick) >= BLE_FRAME_TIMEOUT_MS)
    {
        ble_rx_reset();
    }
}

static bool ble_tx_can_start(bool is_monitor)
{
    uint32_t now = HAL_GetTick();

    ble_rx_check_timeout();

    if (usart5_frame_in_progress || ble_time_before(now, ble_rx_quiet_until) ||
        ble_time_before(now, ble_tx_quiet_until))
    {
        return false;
    }

    (void)is_monitor;
    return true;
}

static void ble_queue_frame_from_isr(const _AGREEMENT *frame)
{
    uint8_t next_head;

    next_head = (uint8_t)((ble_frame_queue_head + 1U) % BLE_FRAME_QUEUE_SIZE);
    if (next_head == ble_frame_queue_tail)
    {
        /* 保留旧帧，丢弃新帧；发送端不会收到 ACK 后会重试该帧。 */
        return;
    }

    memcpy(&ble_frame_queue[ble_frame_queue_head], frame, sizeof(_AGREEMENT));
    __DMB();
    ble_frame_queue_head = next_head;
}

static void ble_note_valid_frame(const _AGREEMENT *frame)
{
    uint32_t now = HAL_GetTick();

    ble_rx_quiet_until = now + BLE_RX_GUARD_MS;

    if (frame->index == 0xC1)
    {
        /* 遥控脚本可能正在阻塞主循环，只做固定长度的无分配快照。 */
        /* C1 is a fixed ten-byte remote record; unused bytes are zero-filled
           by dataAgreeAnalys(), preserving the legacy protocol behavior. */
        blue_update_remote(frame->data, BLUE_REMOTE_DATA_SIZE);
        return;
    }

    ble_queue_frame_from_isr(frame);
}

static void ble_tx_try_start(void)
{
    uint16_t length = 0;
    bool from_control = false;

    /* HAL 已经完成发送但回调未推进队列时，允许下一次轮询自恢复。 */
    if (ble_tx_busy && HAL_UART_GetState(&g_uart5_handle) == HAL_UART_STATE_READY)
    {
        ble_tx_busy = false;
    }

    if (ble_tx_busy || HAL_UART_GetState(&g_uart5_handle) != HAL_UART_STATE_READY)
    {
        return;
    }

    if (ble_tx_control_count > 0U && ble_tx_can_start(false))
    {
        memcpy(ble_tx_active_buffer,
               ble_tx_control_queue[ble_tx_control_tail],
               ble_tx_control_length[ble_tx_control_tail]);
        length = ble_tx_control_length[ble_tx_control_tail];
        ble_tx_control_tail = (uint8_t)((ble_tx_control_tail + 1U) % BLE_TX_CONTROL_QUEUE_SIZE);
        ble_tx_control_count--;
        from_control = true;
    }
    else if (ble_tx_monitor_pending && ble_tx_can_start(true))
    {
        memcpy(ble_tx_active_buffer, ble_tx_monitor_buffer, ble_tx_monitor_length);
        length = ble_tx_monitor_length;
        ble_tx_monitor_pending = false;
    }

    if (length == 0U)
    {
        return;
    }

    if (HAL_UART_Transmit_IT(&g_uart5_handle, ble_tx_active_buffer, length) != HAL_OK)
    {
        /* 句柄状态被其他路径占用时，把包放回待发送位置。 */
        if (from_control)
        {
            ble_tx_control_tail = (uint8_t)((ble_tx_control_tail + BLE_TX_CONTROL_QUEUE_SIZE - 1U) %
                                             BLE_TX_CONTROL_QUEUE_SIZE);
            memcpy(ble_tx_control_queue[ble_tx_control_tail], ble_tx_active_buffer, length);
            ble_tx_control_length[ble_tx_control_tail] = length;
            ble_tx_control_count++;
        }
        else
        {
            memcpy(ble_tx_monitor_buffer, ble_tx_active_buffer, length);
            ble_tx_monitor_length = length;
            ble_tx_monitor_pending = true;
        }
        return;
    }

    ble_tx_busy = true;
}

void blue_tx_poll(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    ble_tx_try_start();
    __set_PRIMASK(primask);
}

void blue_send_control(const uint8_t *data, uint16_t len)
{
    uint32_t primask;
    uint8_t head;

    if (data == NULL || len == 0U)
    {
        return;
    }
    if (len > BLE_TX_CONTROL_PACKET_SIZE)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (ble_tx_control_count >= BLE_TX_CONTROL_QUEUE_SIZE)
    {
        /* 不丢旧 ACK，调用方会因没有响应而重试控制/OTA帧。 */
        __set_PRIMASK(primask);
        return;
    }

    head = ble_tx_control_head;
    memcpy(ble_tx_control_queue[head], data, len);
    ble_tx_control_length[head] = len;
    ble_tx_control_head = (uint8_t)((head + 1U) % BLE_TX_CONTROL_QUEUE_SIZE);
    ble_tx_control_count++;
    ble_tx_try_start();

    __set_PRIMASK(primask);
}

void blue_send_it(const uint8_t *data, uint16_t len)
{
    uint32_t primask;

    if (data == NULL || len == 0U)
    {
        return;
    }
    if (len > BLE_TX_PACKET_SIZE)
    {
        len = BLE_TX_PACKET_SIZE;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    /* 始终保留最新监控包；接收保护窗口只延迟发送，不丢弃状态。 */
    memcpy(ble_tx_monitor_buffer, data, len);
    ble_tx_monitor_length = len;
    ble_tx_monitor_pending = true;
    ble_tx_try_start();

    __set_PRIMASK(primask);
}

bool blue_pop_frame(_AGREEMENT *frame)
{
    uint32_t primask;
    bool has_frame = false;

    if (frame == NULL)
    {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (ble_frame_queue_tail != ble_frame_queue_head)
    {
        memcpy(frame, &ble_frame_queue[ble_frame_queue_tail], sizeof(_AGREEMENT));
        ble_frame_queue_tail = (uint8_t)((ble_frame_queue_tail + 1U) % BLE_FRAME_QUEUE_SIZE);
        has_frame = true;
    }
    __set_PRIMASK(primask);

    return has_frame;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t primask;

    if (huart->Instance != USART5_UX)
    {
        return;
    }

    ble_tx_busy = false;
    ble_tx_quiet_until = HAL_GetTick() + BLE_TX_GUARD_MS;

    primask = __get_PRIMASK();
    __disable_irq();
    ble_tx_try_start();
    __set_PRIMASK(primask);
}

UART_HandleTypeDef *getusartHandle(uint8_t num)
{ 
   switch(num)
	 {
	   case 1:return &g_uart1_handle;
		 case 2:return &g_uart2_handle;
		 case 3:return &g_uart3_handle;
		 case 4:return &g_uart4_handle;
		 case 5:return &g_uart5_handle;
		 default:break;
	 }
	 return NULL;
}
/******************************************************************************************/
/* ???????????, ???printf????, ??????????use MicroLIB */

#if 1

#if (__ARMCC_VERSION >= 6010050)            /* ???AC6??????? */
__asm(".global __use_no_semihosting\n\t");  /* ???????????????? */
__asm(".global __ARM_use_no_argv \n\t");    /* AC6?????????main????????????????????????????????????? */

#else
/* ???AC5???????, ?????????__FILE ?? ???????????? */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* ??????????????????????????_ttywrch\_sys_exit\_sys_command_string????,????????AC6??AC5?? */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/* ????_sys_exit()??????????????? */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}


/* FILE ?? stdio.h???????. */
FILE __stdout;

/* MDK??????????fputc????, printf????????????????fputc?????????????? */
int fputc(int ch, FILE *f)
{
#if 0
	uint8_t bufer[1];
	bufer[0] = (uint8_t)ch;

 
    extern void cdc_vcp_data_tx(void *data, uint16_t Len);
  	cdc_vcp_data_tx(bufer, 1);          /* ???????? */	
	#endif
    return ch;
}
void blue_printf(const char *format, ...)
{
	va_list args;					// va_list?????,???????????
  int length;						// ????????
  uint32_t start_tick;


  va_start(args, format);
	length = vsnprintf((char *)blueTxBufer, sizeof(blueTxBufer), (char *)format, args);
  va_end(args);
	if (length <= 0)
	{
		return;
	}
	if (length >= (int)sizeof(blueTxBufer))
	{
		length = sizeof(blueTxBufer) - 1;
	}

	/* 主循环中的监控发送恢复为可靠的阻塞发送；脚本运行期间由
	 * monitor_send_blue() 使用 IT 队列，不会在中断里进入这里。 */
	start_tick = HAL_GetTick();
	while (HAL_UART_GetState(&g_uart5_handle) != HAL_UART_STATE_READY)
	{
		blue_tx_poll();
		if ((HAL_GetTick() - start_tick) >= 1000U)
		{
			return;
		}
	}
	HAL_UART_Transmit(&g_uart5_handle, blueTxBufer, (uint16_t)length, 1000U);
}
#endif


void uart_transmit_it(UART_HandleTypeDef *huart,uint8_t *data,uint16_t len)
{
		if (HAL_UART_GetState(huart) == HAL_UART_STATE_READY)
		{
				HAL_UART_Transmit_IT(huart, data, len);
		}
}

/******************************************************************************************/

 
 
/**
 * @brief       ????X?????????
 * @param       baudrate: ??????, ?????????????????????
 * @note        ???: ?????????????????, ??????????????????.
 *              ?????USART????????sys_stm32_clock_init()?????????????????.
 * @retval      ??
 */
void usart1_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart1_handle.Instance = USART1_UX;                                       /* USART_UX */
    g_uart1_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8???????? */
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;                           /* ??????? */
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;                            /* ??????????? */
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart1_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart2_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart2_handle.Instance = USART2_UX;                                       /* USART_UX */
    g_uart2_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8???????? */
    g_uart2_handle.Init.StopBits = UART_STOPBITS_1;                           /* ??????? */
    g_uart2_handle.Init.Parity = UART_PARITY_NONE;                            /* ??????????? */
    g_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart2_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart2_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart3_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart3_handle.Instance = USART3_UX;                                       /* USART_UX */
    g_uart3_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart3_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8???????? */
    g_uart3_handle.Init.StopBits = UART_STOPBITS_1;                           /* ??????? */
    g_uart3_handle.Init.Parity = UART_PARITY_NONE;                            /* ??????????? */
    g_uart3_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart3_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart3_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart4_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart4_handle.Instance = USART4_UX;                                       /* USART_UX */
    g_uart4_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart4_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8???????? */
    g_uart4_handle.Init.StopBits = UART_STOPBITS_1;                           /* ??????? */
    g_uart4_handle.Init.Parity = UART_PARITY_NONE;                            /* ??????????? */
    g_uart4_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart4_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart4_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart5_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart5_handle.Instance = USART5_UX;                                       /* USART_UX */
    g_uart5_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart5_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8???????? */
    g_uart5_handle.Init.StopBits = UART_STOPBITS_1;                           /* ??????? */
    g_uart5_handle.Init.Parity = UART_PARITY_NONE;                            /* ??????????? */
    g_uart5_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart5_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart5_handle);                                           /* HAL_UART_Init()?????UART1 */   
}
void uart_blue_idle_start(void)
{ 	
	__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_IDLE); 
	__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_RXNE);  
}
void uart_dma_idle_start(void)
{ 
	__HAL_UART_ENABLE_IT(&g_uart1_handle,UART_IT_IDLE);/*????????????*/	 
  HAL_UART_Receive_DMA(&g_uart1_handle,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart2_handle,UART_IT_IDLE);/*????????????*/	 
  HAL_UART_Receive_DMA(&g_uart2_handle,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart3_handle,UART_IT_IDLE);/*????????????*/	 
  HAL_UART_Receive_DMA(&g_uart3_handle,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart4_handle,UART_IT_IDLE);/*????????????*/	 
  HAL_UART_Receive_DMA(&g_uart4_handle,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	//__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_IDLE); 
	//__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_RXNE); 
	
}
/**
 * @brief       UART???????????
 * @param       huart: UART??????????
 * @note        ???????HAL_UART_Init()????
 *              ???????????????????????????
 * @retval      ??
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
     GPIO_InitTypeDef gpio_init_struct;
    __HAL_RCC_AFIO_CLK_ENABLE();
	   		 
	  
    if (huart->Instance == USART1_UX)                        
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART1_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART1_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART1_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART1_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART1_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART1_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART1_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????????????? */
        
        __HAL_LINKDMA(&g_uart1_handle,hdmarx,UART1RxDMA_Handler);  
        UART1RxDMA_Handler.Instance=DMA1_Channel5;                           //??????
        UART1RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //???????????
        UART1RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART1RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //???????????
        UART1RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8??
        UART1RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //????????????:8??
        UART1RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART1RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?????????
        HAL_DMA_DeInit(&UART1RxDMA_Handler);
        HAL_DMA_Init(&UART1RxDMA_Handler);
			     		 
		    HAL_NVIC_EnableIRQ(USART1_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART1_UX_IRQn,1,0);
    }
		
		
    if (huart->Instance == USART2_UX)                           
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART2_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART2_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART2_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART2_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART2_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART2_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART2_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????????????? */
        
				__HAL_LINKDMA(&g_uart2_handle,hdmarx,UART2RxDMA_Handler);  
        UART2RxDMA_Handler.Instance=DMA1_Channel6;                           //??????
        UART2RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //???????????
        UART2RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART2RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //???????????
        UART2RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8??
        UART2RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //????????????:8??
        UART2RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART2RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?????????
        HAL_DMA_DeInit(&UART2RxDMA_Handler);
        HAL_DMA_Init(&UART2RxDMA_Handler);
			     		 
		    HAL_NVIC_EnableIRQ(USART2_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART2_UX_IRQn,1,0);
        HAL_UART_Receive_DMA(&g_uart2_handle,usart2dmaRxBufer,DMA_RX_BUFER_SIZE); 
    }
    if (huart->Instance == USART3_UX)                           
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART3_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART3_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART3_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART3_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART3_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART3_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART3_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????????????? */
				
				__HAL_LINKDMA(&g_uart3_handle,hdmarx,UART3RxDMA_Handler);
        UART3RxDMA_Handler.Instance=DMA1_Channel3;                           //??????
        UART3RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //???????????
        UART3RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART3RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //???????????
        UART3RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8??
        UART3RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //????????????:8??
        UART3RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART3RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?????????
        HAL_DMA_DeInit(&UART3RxDMA_Handler);
        HAL_DMA_Init(&UART3RxDMA_Handler);
			       		 
		    HAL_NVIC_EnableIRQ(USART3_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART3_UX_IRQn,1,0);
        HAL_UART_Receive_DMA(&g_uart3_handle,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);		 
    }
    if (huart->Instance == USART4_UX)                           
    {
			  __HAL_RCC_DMA2_CLK_ENABLE();	
        USART4_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART4_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART4_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART4_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART4_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART4_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART4_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????????????? */

				__HAL_LINKDMA(&g_uart4_handle,hdmarx,UART4RxDMA_Handler);
        UART4RxDMA_Handler.Instance=DMA2_Channel3;                           //??????
        UART4RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //???????????
        UART4RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART4RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //???????????
        UART4RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8??
        UART4RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //????????????:8??
        UART4RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART4RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?????????
        HAL_DMA_DeInit(&UART4RxDMA_Handler);
        HAL_DMA_Init(&UART4RxDMA_Handler);
			       		 
		    HAL_NVIC_EnableIRQ(USART4_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART4_UX_IRQn,1,0);	 
    }
    if (huart->Instance == USART5_UX)                           
    {
        USART5_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART5_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART5_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART5_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART5_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART5_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART5_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????????????? */
         
		    HAL_NVIC_EnableIRQ(USART5_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART5_UX_IRQn,0,4);
    }
}
 
static void ble_rx_reset(void)
{
    usart5dmaRxlength = 0U;
    usart5_expected_length = 0U;
    usart5_frame_in_progress = false;
    usart5_last_rx_tick = 0U;
}

static void ble_rx_push_byte(uint8_t byte)
{
    uint16_t frame_length;

    ble_rx_check_timeout();
    usart5_last_rx_tick = HAL_GetTick();

    ble_rx_quiet_until = HAL_GetTick() + BLE_RX_GUARD_MS;

    if (!usart5_frame_in_progress)
    {
        if (byte != FRAME_HEADER)
        {
            if (usart5dmaRxlength < sizeof(usart5dmaRxBufer))
            {
                usart5dmaRxBufer[usart5dmaRxlength++] = byte;
            }
            else
            {
                ble_rx_reset();
            }
            return;
        }

        /* 发现帧头时丢弃前面的 AT 文本，按协议帧重新开始。 */
        usart5dmaRxlength = 0U;
        usart5_expected_length = 0U;
        usart5_frame_in_progress = true;
    }

    if (usart5dmaRxlength >= sizeof(usart5dmaRxBufer))
    {
        ble_rx_reset();
        return;
    }

    usart5dmaRxBufer[usart5dmaRxlength++] = byte;

    /* 收到第 4 字节(length)时算一次整帧长度，之后不再重复计算 */
    if (usart5_expected_length == 0U)
    {
        if (usart5dmaRxlength < 4U)
        {
            return;
        }

        frame_length = (uint16_t)usart5dmaRxBufer[3] + 7U;
        if (frame_length < MIN_FRAME_SIZE || frame_length > sizeof(usart5dmaRxBufer))
        {
            ble_rx_reset();
            return;
        }
        usart5_expected_length = frame_length;
    }

    /* 按 length 收完整帧，不依赖帧内字节间的空闲时间。 */
    if (usart5dmaRxlength == usart5_expected_length)
    {
        _AGREEMENT frame;

        if (dataAgreeAnalys(&frame, usart5dmaRxBufer, usart5_expected_length) == AGREE_MEN_OK)
        {
            ble_note_valid_frame(&frame);
        }
        ble_rx_reset();
    }
}

static void ble_report_at_response(void)
{
    DEV_BLUE *blue;
    uint16_t copy_length;

    if (usart5dmaRxlength == 0U)
    {
        return;
    }

    if (usart5_frame_in_progress)
    {
        /* 兼容"length 字段为整帧长度"的发送端：这类帧按 data[3]+7 永远等不满，
         * 在 IDLE 时用当前缓冲再尝试解析一次；解析成功才消费，否则继续等后续字节。 */
        _AGREEMENT frame;

        if (dataAgreeAnalys(&frame, usart5dmaRxBufer, usart5dmaRxlength) == AGREE_MEN_OK)
        {
            ble_note_valid_frame(&frame);
            ble_rx_reset();
        }
        return;
    }

    blue = (DEV_BLUE *)getHubBase(PORT_BLUE);
    if (blue != NULL)
    {
        copy_length = usart5dmaRxlength;
        if (copy_length >= sizeof(blue->at_cmd_bufer))
        {
            copy_length = sizeof(blue->at_cmd_bufer) - 1U;
        }
        memset(blue->at_cmd_bufer, 0, sizeof(blue->at_cmd_bufer));
        memcpy(blue->at_cmd_bufer, usart5dmaRxBufer, copy_length);
        blue->at_cmd_bufer[copy_length] = '\0';
        blue->is_resh_flag = true;
    }
    ble_rx_reset();
}

static void HAL_USART_IDLE_INTERRUPT(UART_HandleTypeDef *huart)
{
	  if(huart->Instance == USART5_UX)
		{
        uint32_t status = huart->Instance->SR;

        if ((status & USART_SR_RXNE) != RESET)
        {
            ble_rx_push_byte((uint8_t)(huart->Instance->DR & 0xFFU));
        }

        if ((status & USART_SR_IDLE) != RESET)
        {
            /* Read SR then DR to clear IDLE, but feed a byte seen during the clear. */
            uint32_t clear_status = huart->Instance->SR;
            uint8_t clear_data = (uint8_t)(huart->Instance->DR & 0xFFU);
            if ((clear_status & USART_SR_RXNE) != RESET)
            {
                ble_rx_push_byte(clear_data);
            }
            while (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET)
            {
                ble_rx_push_byte((uint8_t)(huart->Instance->DR & 0xFFU));
            }
            ble_report_at_response();
        }
        return;
		}
	  if(__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET)
		{ 
			 _AGREEMENT frame;
			 if(huart->Instance == USART1_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);					 
				   if(dataAgreeAnalys(&frame,usart1dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
					  /* 0xA3 复用（超声波/IR_REMOTE）：在识别层按帧内容细分并绑定 */
					  HubBase_Frame_Process(0,&frame);
					 }
					 else
					 { 
					    /*????????????*/;
					 // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart1dmaRxBufer,0,sizeof(usart1dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART2_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart2dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
					  /* 0xA3 复用（超声波/IR_REMOTE）：在识别层按帧内容细分并绑定 */
					  HubBase_Frame_Process(1,&frame);
					 }
					 else
					 { 
					   // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart2dmaRxBufer,0,sizeof(usart2dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART3_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart3dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
					  /* 0xA3 复用（超声波/IR_REMOTE）：在识别层按帧内容细分并绑定 */
					  HubBase_Frame_Process(2,&frame);
					 }
					 else
					 { 
					  // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart3dmaRxBufer,0,sizeof(usart3dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART4_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart4dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
					  /* 0xA3 复用（超声波/IR_REMOTE）：在识别层按帧内容细分并绑定 */
					  HubBase_Frame_Process(3,&frame);
					 }
					 else
					 { 
					  //  usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart4dmaRxBufer,0,sizeof(usart4dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
		}
		
}
static void ClearUARTErrors(UART_HandleTypeDef *huart)
{
 
	
   // ?????????
   //__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF |UART_CLEAR_IDLEF);

   __HAL_UART_CLEAR_OREFLAG(huart);
	 __HAL_UART_CLEAR_IDLEFLAG(huart);
   huart->RxState = HAL_UART_STATE_READY;
   huart->Lock = HAL_UNLOCKED;
}

 void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	__HAL_UART_CLEAR_FEFLAG(huart);
	__HAL_UART_CLEAR_NEFLAG(huart);
	__HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_IDLEFLAG(huart);
	
	huart->RxState = HAL_UART_STATE_READY;
	huart->Lock = HAL_UNLOCKED;

  if(huart->Instance == USART1_UX)
  {     
		ClearUARTErrors(&g_uart1_handle);
    HAL_UART_Receive_DMA(huart,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);
  }
  else if(huart->Instance == USART2_UX)
  {
		ClearUARTErrors(&g_uart2_handle);
     HAL_UART_Receive_DMA(huart,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);
 }
	else if(huart->Instance == USART3_UX)
	{
		 ClearUARTErrors(&g_uart3_handle);
		 HAL_UART_Receive_DMA(huart,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);
	}
	 else if(huart->Instance == USART4_UX)
	{
		 ClearUARTErrors(&g_uart4_handle);
		 HAL_UART_Receive_DMA(huart,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);
	}
	else if(huart->Instance == USART5_UX)
	{
           ClearUARTErrors(&g_uart5_handle);
           /* UART5 使用自定义 RXNE 收包，不要再启动 HAL 的单字节接收状态机。 */
           ble_rx_reset();
           ble_rx_quiet_until = HAL_GetTick() + BLE_RX_GUARD_MS;
		 g_uart5_handle.gState = HAL_UART_STATE_READY;
		 ble_tx_busy = false;
		 __HAL_UART_DISABLE_IT(&g_uart5_handle, UART_IT_TXE);
		 __HAL_UART_DISABLE_IT(&g_uart5_handle, UART_IT_TC);
		 __HAL_UART_ENABLE_IT(&g_uart5_handle, UART_IT_IDLE);
		 __HAL_UART_ENABLE_IT(&g_uart5_handle, UART_IT_RXNE);
	}
}
void USART1_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart1_handle);
    HAL_UART_IRQHandler(&g_uart1_handle);   
}
void USART2_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart2_handle);
    HAL_UART_IRQHandler(&g_uart2_handle); 
}
void USART3_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart3_handle);
    HAL_UART_IRQHandler(&g_uart3_handle);   
}
void USART4_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart4_handle);
    HAL_UART_IRQHandler(&g_uart4_handle);  
}
void USART5_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart5_handle);
    /* UART5 RXNE is consumed by HAL_USART_IDLE_INTERRUPT().  Let HAL handle
       this IRQ only while its TX state machine owns the peripheral. */
    if (g_uart5_handle.gState == HAL_UART_STATE_BUSY_TX ||
        (g_uart5_handle.Instance->SR &
         (USART_SR_PE | USART_SR_FE | USART_SR_ORE | USART_SR_NE)) != 0U)
    {
        HAL_UART_IRQHandler(&g_uart5_handle);
    }
}
void uart_port_init(void)
{ 
 	 usart1_init(115200);
	 usart2_init(115200);
	 usart3_init(115200);
	 usart4_init(115200);
	 usart5_init(115200);  
	 uart_blue_idle_start();
	 //uart_dma_idle_start();
}
