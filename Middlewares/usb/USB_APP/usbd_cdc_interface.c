/**
 ****************************************************************************************************
 * @file        usbd_cdc_interface.c
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-06
 * @brief       USB CDC ��������
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� MiniSTM32 V4������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20200406
 * ��һ�η���
 *
 ****************************************************************************************************
 */

#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "usbd_cdc_interface.h"
#include "event_manager.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
 
volatile bool usb_link = false;
/* USB���⴮��������ò��� */
USBD_CDC_LineCodingTypeDef LineCoding =
{
    115200,     /* ������ */
    0x00,       /* ֹͣλ,Ĭ��1λ */
    0x00,       /* У��λ,Ĭ���� */
    0x08        /* ����λ,Ĭ��8λ */
};


/* usb_printf���ͻ�����, ����vsprintf */
uint8_t g_usb_usart_printf_buffer[USB_USART_REC_LEN];
/* USB���յ����ݻ�����,���USART_REC_LEN���ֽ�,����USBD_CDC_SetRxBuffer���� */
uint8_t g_usb_rx_buffer[USB_USART_REC_LEN];

USB_MESSAGE_BOX usb_message;

volatile uint32_t usb_idle_tick;

/* ����״̬
 * bit15   , ������ɱ�־
 * bit14   , ���յ�0x0d
 * bit13~0 , ���յ�����Ч�ֽ���Ŀ
 */
uint16_t g_usb_usart_rx_sta=0;  /* ����״̬��� */


extern USBD_HandleTypeDef USBD_Device;
static int8_t CDC_Itf_Init(void);
static int8_t CDC_Itf_DeInit(void);
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(uint8_t *pbuf, uint32_t *Len);


/* ���⴮�����ú���(��USB�ں˵���) */
USBD_CDC_ItfTypeDef USBD_CDC_fops =
{
    CDC_Itf_Init,
    CDC_Itf_DeInit,
    CDC_Itf_Control,
    CDC_Itf_Receive
};

/**
 * @brief       ��ʼ�� CDC
 * @param       ��
 * @retval      USB״̬
 *   @arg       USBD_OK(0)   , ����;
 *   @arg       USBD_BUSY(1) , æ;
 *   @arg       USBD_FAIL(2) , ʧ��;
 */
static int8_t CDC_Itf_Init(void)
{
    USBD_CDC_SetRxBuffer(&USBD_Device, g_usb_rx_buffer);
    return USBD_OK;
}

/**
 * @brief       ��λ CDC
 * @param       ��
 * @retval      USB״̬
 *   @arg       USBD_OK(0)   , ����;
 *   @arg       USBD_BUSY(1) , æ;
 *   @arg       USBD_FAIL(2) , ʧ��;
 */
static int8_t CDC_Itf_DeInit(void)
{
    return USBD_OK;
}

/**
 * @brief       ���� CDC ������
 * @param       cmd     : ��������
 * @param       buf     : �������ݻ�����/�������滺����
 * @param       length  : ���ݳ���
 * @retval      USB״̬
 *   @arg       USBD_OK(0)   , ����;
 *   @arg       USBD_BUSY(1) , æ;
 *   @arg       USBD_FAIL(2) , ʧ��;
 */
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    switch (cmd)
    {
        case CDC_SEND_ENCAPSULATED_COMMAND:
            break;

        case CDC_GET_ENCAPSULATED_RESPONSE:
            break;

        case CDC_SET_COMM_FEATURE:
            break;

        case CDC_GET_COMM_FEATURE:
            break;

        case CDC_CLEAR_COMM_FEATURE:
            break;

        case CDC_SET_LINE_CODING:
            LineCoding.bitrate = (uint32_t) (pbuf[0] | (pbuf[1] << 8) |
                                             (pbuf[2] << 16) | (pbuf[3] << 24));
            LineCoding.format = pbuf[4];
            LineCoding.paritytype = pbuf[5];
            LineCoding.datatype = pbuf[6];
				    #if 0
            /* ��ӡ���ò��� */
            printf("linecoding.format:%d\r\n", LineCoding.format);
            printf("linecoding.paritytype:%d\r\n", LineCoding.paritytype);
            printf("linecoding.datatype:%d\r\n", LineCoding.datatype);
            printf("linecoding.bitrate:%d\r\n", LineCoding.bitrate);
				    #endif
            break;

        case CDC_GET_LINE_CODING:
            pbuf[0] = (uint8_t) (LineCoding.bitrate);
            pbuf[1] = (uint8_t) (LineCoding.bitrate >> 8);
            pbuf[2] = (uint8_t) (LineCoding.bitrate >> 16);
            pbuf[3] = (uint8_t) (LineCoding.bitrate >> 24);
            pbuf[4] = LineCoding.format;
            pbuf[5] = LineCoding.paritytype;
            pbuf[6] = LineCoding.datatype;
            break;

        case CDC_SET_CONTROL_LINE_STATE:
            break;

        case CDC_SEND_BREAK:
            break;

        default:
            break;
    }

    return USBD_OK;
}

/**
 * @brief       CDC ���ݽ��պ���
 * @param       buf     : �������ݻ�����
 * @param       len     : ���յ������ݳ���
 * @retval      USB״̬
 *   @arg       USBD_OK(0)   , ����;
 *   @arg       USBD_BUSY(1) , æ;
 *   @arg       USBD_FAIL(2) , ʧ��;
 */
static int8_t CDC_Itf_Receive(uint8_t *buf, uint32_t *len)
{
    USBD_CDC_ReceivePacket(&USBD_Device);
    cdc_vcp_data_rx(buf, *len);
    return USBD_OK;
}

/**
 * @brief       ������ USB ���⴮�ڽ��յ�������
 * @param       buf     : �������ݻ�����
 * @param       len     : ���յ������ݳ���
 * @retval      ��
 */
void cdc_vcp_data_rx (uint8_t *buf, uint32_t Len)
{
		if (buf == NULL || Len == 0) {
        return;
    }
		
		if(usb_message.g_user_usb_rx_len > USB_USART_REC_LEN)
		{ 
		  reset_usb_parser();
			return;
		}
		
		memcpy(usb_message.g_user_usb_rx_buffer + usb_message.g_user_usb_rx_len,buf,Len);	
		usb_message.g_user_usb_rx_len+=Len;
		usb_idle_tick = 5;
}

/**
 * @brief       ͨ�� USB ��������
 * @param       buf     : Ҫ���͵����ݻ�����
 * @param       len     : ���ݳ���
 * @retval      ��
 */
void cdc_vcp_data_tx(void *data, uint16_t Len)
{

    if(USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)data, Len) == USBD_FAIL)
				return;

    USBD_CDC_TransmitPacket(&USBD_Device);
}

/*
 * CDC 共享发送的临界区封装。
 * g_usb_usart_printf_buffer 与 USBD_Device(CDC) 是单例，被三处共享写入：
 *   1) usb_printf            —— 主循环上下文日志
 *   2) pika_platform_printf  —— 脚本 print()，run_python 期间在主循环/VM 上下文
 *   3) monitor_send_usb      —— btim 定时器中断里的监控发送
 * 三者必须串行，否则会互相破坏缓冲区与 CDC TxState。
 * 用 PRIMASK 临界区：线程侧持锁时屏蔽 btim 中断，ISR 监控发送无法抢占；
 * 临界区仅 vsnprintf(<600B)+一次 CDC 调用，<50us，不影响 10ms 喂狗。
 * 注：vsnprintf 以 USB_USART_REC_LEN 截断，比原 vsprintf 更安全（防溢出）。
 */
void vcdc_send_locked(const char *fmt, va_list ap)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    uint16_t n = (uint16_t)vsnprintf((char *)g_usb_usart_printf_buffer, USB_USART_REC_LEN, fmt, ap);
    cdc_vcp_data_tx(g_usb_usart_printf_buffer, n);
    __set_PRIMASK(primask);
}

void cdc_send_locked(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcdc_send_locked(fmt, ap);
    va_end(ap);
}
extern volatile uint8_t g_device_state;     /* USB���� ��� */
/**
 * @brief       ͨ�� USB ��ʽ���������
 *   @note      ͨ��USB VCPʵ��printf���
 *              ȷ��һ�η������ݳ��Ȳ���USB_USART_REC_LEN�ֽ�
 * @param       ��ʽ�����
 * @retval      ��
 */
void usb_printf(char *fmt, ...)
{
    /* Routed through PRIMASK-locked path: serializes with
     * pika_platform_printf (script print) and the ISR monitor send. */
    va_list ap;
    va_start(ap, fmt);
    vcdc_send_locked(fmt, ap);
    va_end(ap);
}
void reset_usb_parser(void)
{   
   usb_message.g_user_usb_rx_len = 0;
	 memset(usb_message.g_user_usb_rx_buffer,0,sizeof(usb_message.g_user_usb_rx_buffer));
	 frame_parser_init(&usb_message.usb_parser);
}

 
USBD_HandleTypeDef USBD_Device;             /* USB Device�����ṹ�� */
 
void usb_event_connect_callback(void *arg)
{  
	 extern void beep_play_notice(void);
	 extern void beep_play_error(void); 
   static volatile uint8_t usbstatus;
	 if(usbstatus!=g_device_state)
	 { 
	    usbstatus = g_device_state;
		  if(usbstatus == 1)
      {
					/*usb ������*/
				  usb_link = true;
				  beep_play_notice();
			}
			else
			{
				  /*usb �ѶϿ�*/
				  usb_link = false;
				  beep_play_error();
			}
	 }
}

void usb_cdc_init(void)
{ 
    usbd_port_config(0);    /* USB�ȶϿ� */
    delay_ms(1);
    usbd_port_config(1);    /* USB�ٴ����� */
    delay_ms(1);
    extern USBD_DescriptorsTypeDef VCP_Desc;
    USBD_Init(&USBD_Device, &VCP_Desc, 0);
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);
    USBD_Start(&USBD_Device);
 
}

void usb_event_receive_callback(void *arg) {
	  USB_MESSAGE_BOX *message = (USB_MESSAGE_BOX*)arg;
	  if(message == NULL)return;
	  extern void busDataparsing(_AGREEMENT *frame,void (*port_transerf_data)(void *data,uint16_t length));
	  uint8_t *data = message->g_user_usb_rx_buffer;
	  message->g_sys_usb_rx_len = message->g_user_usb_rx_len;
	  while (message->g_user_usb_rx_len--){			 
      if(frame_parser_process_byte(&message->usb_parser, *data++)){
				_AGREEMENT frame;
			 uint16_t frame_length = message->usb_parser.index;
			 if(dataAgreeAnalys(&frame,message->usb_parser.buffer,frame_length) == AGREE_MEN_OK)
			 { 
					busDataparsing(&frame,cdc_vcp_data_tx);
			 }
			 else
			 { 
				 usb_printf("->usb receive other data:%s\r\n",message->g_user_usb_rx_buffer);
				// extern void blue_send_data(char *str,uint16_t len);
				// if(message->g_user_usb_rx_buffer[0] == 'A' && message->g_user_usb_rx_buffer[0] == 'T')
				// { 
				  //  blue_send_data((char*)message->g_user_usb_rx_buffer,message->g_sys_usb_rx_len);
				// }
				 }
			 message->usb_parser.state = STATE_IDLE;
			 message->usb_parser.index = 0;
			 message->usb_parser.expected_length = 0;
			 message->usb_parser.data_bytes_received = 0;
			 message->usb_parser.calc_checksum = 0;
			 message->usb_parser.frame_valid = false;
     }
	 }		 
    reset_usb_parser();	   	 
    set_event_disable("usb_receive");
}
 
 







