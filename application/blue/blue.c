#include "blue.h"
#include "matrix_port.h"

#include "event_manager.h"
#include "string.h"
#include "malloc.h"
#include "protocol.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "exfuns.h"

static volatile uint8_t blue_remote_latest[BLUE_REMOTE_DATA_SIZE];

/* 遥控活跃判定：
 * 1. 主动进入遥控模式后，直到主动退出前一直暂停蓝牙监控；
 * 2. 未主动进入时，收到 0xC1 帧也认为对端是遥控器（PC 上位机从不发 0xC1），
 *    30s 内暂停蓝牙监控，把带宽让给遥控。 */
#define BLUE_REMOTE_ACTIVE_TIMEOUT_MS 30000U
static volatile bool blue_remote_session_active;
static volatile bool blue_remote_seen;
static volatile uint32_t blue_remote_last_tick;

static void blue_remote_clear_latest(void)
{
    memset((void *)blue_remote_latest, 0, sizeof(blue_remote_latest));
}

bool blue_remote_active(void)
{
    if(blue_remote_session_active)
    {
        return true;
    }
    if(!blue_remote_seen)
    {
        return false;
    }
    return (uint32_t)(HAL_GetTick() - blue_remote_last_tick) < BLUE_REMOTE_ACTIVE_TIMEOUT_MS;
}

void blue_remote_session_start(void)
{
    blue_remote_session_active = true;
    blue_remote_seen = false;
    blue_remote_last_tick = 0;
    blue_remote_clear_latest();
}

void blue_remote_session_end(void)
{
    blue_remote_session_active = false;
    blue_remote_seen = false;
    blue_remote_last_tick = 0;
    blue_remote_clear_latest();
}

void blue_update_remote(const uint8_t *data,uint16_t length)
{
    uint16_t copy_length;

    if(data == NULL || length == 0U)
    {
        return;
    }

    copy_length = length;
    if(copy_length > sizeof(blue_remote_latest))
    {
        copy_length = sizeof(blue_remote_latest);
    }

    blue_remote_seen = true;
    blue_remote_last_tick = HAL_GetTick();

    /* 关中断整块替换：清零/拷贝过程不会被脚本读到，
     * 否则脚本可能读到"全 0=全部松开"的中间态，边沿检测就会多触发。 */
    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        memset((void *)blue_remote_latest, 0, sizeof(blue_remote_latest));
        memcpy((void *)blue_remote_latest, data, copy_length);
        __set_PRIMASK(primask);
    }
}

void blue_read_remote(uint8_t *data)
{
    uint32_t primask;

    if(data == NULL)
    {
        return;
    }

    /* 关中断整块快照，保证脚本拿到的一定是某一帧的完整状态，不会撕裂。 */
    primask = __get_PRIMASK();
    __disable_irq();
    memcpy(data, (const void *)blue_remote_latest, sizeof(blue_remote_latest));
    __set_PRIMASK(primask);
}

static uint8_t blueScanATcmd(char *atcmd)
{
	  uint8_t error_num = 0;
	
	  DEV_BLUE *blue = (DEV_BLUE*)getHubBase(PORT_BLUE);
	  blue->is_resh_flag = false;
    
		while(1)
		{
			blue_send_control((const uint8_t *)atcmd,strlen(atcmd));
		  if(blue->is_resh_flag)
		 {
		   blue->is_resh_flag = false;
		   if(strstr((const char*)blue->at_cmd_bufer, "OK") != NULL)
		   	    break;
		 }
		 error_num++;
		 if(error_num>10)break;
		 delay_ms(200);
		}
	return 0;
}
void loop_blue_at(void)
{ 
   DEV_BLUE *blue = (DEV_BLUE*)getHubBase(PORT_BLUE);
	 if(blue->is_resh_flag)
	 { 
		  extern void usb_printf(char *fmt, ...);
	    usb_printf("%s", blue->at_cmd_bufer);
		  blue->is_resh_flag = false;
	 }
}
void blue_logo_blinke(void)
{ 
 static bool blinke = false;
 uint8_t blue_log[] = {0x0C, 0x15, 0x16, 0x1C, 0x16, 0x15, 0x0C};
 DEV_BLUE *blue = (DEV_BLUE*)getHubBase(PORT_BLUE);
 
 if(blue->is_off_on)
 { 
   matrix_port_display_pattern(blue_log);
 }
 else
 { 
 if(blinke)
	 matrix_port_clear();
 else
		matrix_port_display_pattern(blue_log);
 
 blinke = !blinke;  
 }
}
void blue_set_on(void)
{
//	set_event_disable("monitor_event");
  blueScanATcmd("AT+ROLE=2\r\n");
//	set_event_enable("monitor_event");
}
void blue_set_off(void)
{
//	set_event_disable("monitor_event");
  blueScanATcmd("AT+ROLE=1\r\n");
//	set_event_enable("monitor_event");
}
void blue_send_data(void *data,uint16_t len)
{ 
    blue_send_control((const uint8_t *)data, len);
}
void blue_init(void)
{ 
   identify_and_bind(&hub_port[PORT_BLUE],DEVICE_BLUE_ID);
   DEV_BLUE *blue = (DEV_BLUE*)getHubBase(PORT_BLUE);
	 if(blue!=NULL)
	 { 
	   if(fatfs_read_file("blue_cfg.cfg",(BLUE_CFG*)&blue->cfg,sizeof(BLUE_CFG))!=FR_OK)
		 {
				goto BLUE_INIT;
		 }
		 else
		 { 
			  
		    if(!blue->cfg.blue_init_state)
						goto BLUE_INIT;
				
				blue->is_off_on = blue->cfg.on_off;
				if(blue->is_off_on)
					blue_set_on();
				else
					blue_set_off();
				return;
		 }
	 }
	 
	 BLUE_INIT:
				blueScanATcmd("AT+NAME=Spark_AI\r\n");
 
				blueScanATcmd("AT+ROLE=1\r\n");
 
				blueScanATcmd("AT+MODE=1\r\n");
 
				blueScanATcmd("AT+UART=4\r\n");
 
				blueScanATcmd("AT+POWE=9\r\n");
 
				blueScanATcmd("AT+ADVINT=3\r\n");
 
				blueScanATcmd("AT+RST\r\n");	 
	 
        blue->cfg.blue_init_state = true;
			 
        blue->cfg.on_off = false;
			 
        fatfs_create_file("blue_cfg.cfg",(BLUE_CFG*)&blue->cfg,sizeof(BLUE_CFG));		
}
void refsh_blue(void* self, void* data)
{ 
	 _AGREEMENT *_fd = (_AGREEMENT *)data;

	 (void)self;
   switch(_fd->index)
   {
		 /*remote*/
	   case 0xC1:
			  blue_update_remote(_fd->data,BLUE_REMOTE_DATA_SIZE);
		 break;
	 }
}

DEV_BLUE *read_blue(void *self)
{ 
   DEV_BLUE *mt = (DEV_BLUE*)self;
	 return mt;     
}

DEV_BLUE *create_blue(void)
{ 
    DEV_BLUE *blue = mymalloc(SRAMIN,sizeof(DEV_BLUE));
    if(blue == NULL) return NULL;
    
    *blue = (DEV_BLUE){
        .base = {
            .type = DEVICE_BLUE_ID,
            .name = "blue"
        },
			  .huart = getusartHandle(5)
    };
	 blue->is_resh_flag = false;
	 blue->is_off_on = false;
	 memset(blue->at_cmd_bufer,0,32);
	 memset((void *)blue_remote_latest,0,sizeof(blue_remote_latest));
	 blue_remote_session_active = false;
	 blue_remote_seen = false;
	 blue_remote_last_tick = 0;
    return blue;
}

