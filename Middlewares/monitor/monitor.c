#include "json-maker.h"
#include "monitor.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "malloc.h"
#include "rtc.h"
#include "touch.h"
#include "ultrasion.h"
#include "ir_remote.h"
#include "color.h"
#include "bat_manager.h"
#include "btim.h"
#include "blue.h"
#include "deviceIdentify.h"
#include "./SYSTEM/usart/usart.h"
#include "usbd_cdc_interface.h"
#include "usbd_cdc.h"
static char json_buffer[2*1024];
extern bool returnDownLoadState(void);
extern uint32_t fatfs_total;
extern uint32_t fatfs_free;
extern volatile uint32_t spark_version;
extern void usb_printf(char *fmt, ...);
extern void blue_printf(const char *format, ...);
const char* weekdays[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saterday"};

char* monitor_get_json(void)
{
    return json_buffer;
}

void monitor_call_back(void*arg) { 
	 
    size_t remLen = 2*1024;
    char *p = json_buffer;
    char temp_str[64];
    memset(p,0,remLen);
	
 
	  DEV_COLOR *color;
	  DEV_TOUCH *touch;
	  DEV_ULTRASION *ultrasion;
	  DEV_IR_REMOTE *ir_remote;
 
	  HubBase_Scan_TimeOut();
    if(returnDownLoadState()) return;
	
    p = json_objOpen(p, NULL, &remLen);
    p = json_arrOpen(p, "deviceList", &remLen);
    #if 1
    for(uint8_t i = 0; i < 4; i++) {
        p = json_objOpen(p, NULL, &remLen);
        p = json_int(p, "port", i, &remLen);
        
        if(hub_port[i].sensors != NULL) {			
            /* 按内部类型分派：0xA3 复用线上 IR_REMOTE 与超声波 ObjectID 相同 */
            switch(hub_port[i].sensors->type) {
                case DEVICE_ULTRASION_ID: {
                    ultrasion = read_ultrasion((SensorBase *)hub_port[i].sensors);
                    p = json_objOpen(p, "ultrasion", &remLen);
                    snprintf(temp_str, sizeof(temp_str), "%d", ultrasion->cm);
                    p = json_str(p, "cm", temp_str, &remLen);
									 // snprintf(temp_str, sizeof(temp_str), "%d", ultrasion->dt);
									//	p = json_str(p, "dt", temp_str, &remLen);
                    p = json_objClose(p, &remLen);
                    break;
                }
                case SENSOR_TYPE_IR_REMOTE: {
                    ir_remote = read_ir_remote((SensorBase *)hub_port[i].sensors);
                    p = json_objOpen(p, "ir_remote", &remLen);
                    /* 只上报设备回传的命令态；bat 恒为未知，按协议要求不出现在 JSON 中 */
                    p = json_int(p, "state", ir_remote->state, &remLen);
                    p = json_objClose(p, &remLen);
                    break;
                }
                case DEVICE_TOUCH_ID:{
                    touch = read_touch((SensorBase *)hub_port[i].sensors);
                    p = json_objOpen(p, "touch", &remLen);
                    p = json_int(p, "state", touch->touchState, &remLen);
										 
                    p = json_objClose(p, &remLen);
                    break;
                }
                case DEVICE_COLOR_ID: {
                    color = read_color((SensorBase *)hub_port[i].sensors);
                    p = json_objOpen(p, "color", &remLen);
                    snprintf(temp_str, sizeof(temp_str), "%d",color->lux);
                    p = json_str(p, "lux", temp_str, &remLen);		
							
                    snprintf(temp_str, sizeof(temp_str), "%d",color->lux_state);
                    p = json_str(p, "state", temp_str, &remLen);		

                    snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.cal_min);
                    p = json_str(p, "min", temp_str, &remLen);	

                    snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.cal_max);
                    p = json_str(p, "max", temp_str, &remLen);	

                    snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.thresholdValue);
                    p = json_str(p, "threadValue", temp_str, &remLen);	
									
                   // snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.cal_max);
                   // p = json_str(p, "cal_max", temp_str, &remLen);										

                   // snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.cal_min);
                   // p = json_str(p, "cal_min", temp_str, &remLen);	

                   // snprintf(temp_str, sizeof(temp_str), "%d",color->color_calibation.calibrated);
                   // p = json_str(p, "cal_brated", temp_str, &remLen);	
									
                    p = json_objClose(p, &remLen);
                    break;
                }
            }				
        }     
        p = json_objClose(p, &remLen);  
    }
    #endif
    p = json_arrClose(p, &remLen); 
    
 
    p = json_objOpen(p, "flash", &remLen);
    
    snprintf(temp_str, sizeof(temp_str), "%d kb", fatfs_total);
    p = json_str(p, "total", temp_str, &remLen);
    
    snprintf(temp_str, sizeof(temp_str), "%d kb", fatfs_free);
    p = json_str(p, "free", temp_str, &remLen);
	  p = json_objClose(p, &remLen);
     
	  p = json_objOpen(p,"adc",&remLen);
		snprintf(temp_str,sizeof(temp_str),"%d%%",calculate_battery_percentage(get_bat_filtered_volatge(),VOLTAGE_CRITICAL_LOW,VOLTAGE_HIGH_MAX));
		p = json_str(p, "bat", temp_str, &remLen);
		
    p = json_objClose(p, &remLen);
    
    p = json_int(p, "version", spark_version, &remLen);

    snprintf(temp_str, sizeof(temp_str), "%d", my_mem_perused(SRAMIN));
    p = json_str(p, "heap", temp_str, &remLen);
  //  p = json_objClose(p, &remLen);
		 
		snprintf(temp_str,sizeof(temp_str),"%s","WillAiState");
		extern volatile bool start_py;
		if(start_py)
			p = json_str(p, temp_str,"run", &remLen);
		else
			p = json_str(p, temp_str,"stop", &remLen);
    
	//	DateTime_t sys_time;
	//	Time_Get(&sys_time);
	//	snprintf(temp_str,sizeof(temp_str),"%d-%d-%d %d:%d:%d",sys_time.year,sys_time.month,sys_time.day,sys_time.hour,sys_time.minute,sys_time.second);
	//	p = json_str(p, "Flash-Time", temp_str, &remLen);
   // p = json_objClose(p, &remLen);
  //  extern DateTime_t start_time; 
  //  extern DateTime_t onff_time;   
	 
 
		 
		//snprintf(temp_str,sizeof(temp_str),"%d-%d-%d %d:%d:%d",onff_time.year,onff_time.month,onff_time.day,onff_time.hour,onff_time.minute,onff_time.second);
		//p = json_str(p, "OFF-Time", temp_str, &remLen);
    p = json_objClose(p, &remLen);
    
    p = json_end(p, &remLen);
    
 
    if (p == NULL || remLen == 0) {
        return;
    }
}

/*
 * =======================================================================
 * 中断驱动的监控发送（由 btim 定时器中断的 event_schedlucer 派发）
 * =======================================================================
 * 背景：run_python()/pauto_play() 同步阻塞主循环，原主循环里的
 *       usb_printf/blue_printf 发送永远轮不到 → 脚本运行时监控断流。
 * 解决：把发送也放进 btim 中断事件，中断不受主循环阻塞。
 * 互斥：仅当 start_py || start_pauto（主循环正被脚本阻塞）时才发，
 *       空闲期仍由主循环发送，两者不重复。
 */

extern volatile bool start_py;        /* main.c：脚本运行标志 */
extern volatile bool start_pauto;     /* main.c：Pauto 演示运行标志 */
extern volatile bool usb_monitor_enabled;
extern volatile bool blue_monitor_enabled;
extern USBD_HandleTypeDef USBD_Device;

/* USB 监控专用发送缓冲区：与 g_usb_usart_printf_buffer 分离，
 * 避免 monitor_send_usb 的 DMA 与脚本 print()/usb_printf 异步读写同一缓冲区而互相踩踏。 */
static char mon_usb_txbuf[USB_USART_REC_LEN + 8];

/* 10ms：USB CDC 监控发送 */
void monitor_send_usb(void *arg)
{
    (void)arg;
    if (!(start_py || start_pauto))      return;   /* 空闲期交给主循环 */
    if (!usb_monitor_enabled)            return;
    if (returnDownLoadState())           return;   /* OTA 进行中 */

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)USBD_Device.pData;
    /* CDC 忙(上一包 DMA 未完成)则丢弃本帧：既省去 1KB vsprintf 的 ISR 开销，
     * 又避免覆盖正在被 DMA 读取的 mon_usb_txbuf。 */
    if (hcdc == NULL || hcdc->TxState != 0U) return;

    /* snprintf 写的是本函数专用的 mon_usb_txbuf，不与 usb_printf/pika_platform_printf
     * 共享，放在临界区外，避免关中断时间超过 UART5 一个字节(115200 下 87us)导致丢字节。
     * 临界区只保留 SetTxBuffer+TransmitPacket，与其它 USB 发送串行。 */
    uint16_t n = (uint16_t)snprintf(mon_usb_txbuf, sizeof(mon_usb_txbuf), "%s\r\n", monitor_get_json());

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    cdc_vcp_data_tx(mon_usb_txbuf, n);
    __set_PRIMASK(primask);
}

/* 200ms：蓝牙(UART5) 监控发送，非阻塞 */
void monitor_send_blue(void *arg)
{
    (void)arg;
    blue_tx_poll();
    if (!(start_py || start_pauto))      return;   /* 空闲期交给主循环 */
    if (!blue_monitor_enabled)           return;
    if (returnDownLoadState())           return;   /* OTA 进行中 */
    if (blue_remote_active())            return;   /* 遥控器活跃：让出蓝牙带宽 */

    DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(PORT_BLUE));
    if (blue == NULL || !blue->is_off_on) return;  /* 蓝牙未连接/未开启 */

    static char blue_mon_txbuf[1024];
    size_t json_length = strlen(monitor_get_json());

    /* 不发送被截断的 JSON，否则上位机只能把整行丢弃。 */
    if (json_length > sizeof(blue_mon_txbuf) - 3U)
    {
        return;
    }

    memcpy(blue_mon_txbuf, monitor_get_json(), json_length);
    blue_mon_txbuf[json_length++] = '\r';
    blue_mon_txbuf[json_length++] = '\n';
    blue_send_it((const uint8_t *)blue_mon_txbuf, (uint16_t)json_length);
}
