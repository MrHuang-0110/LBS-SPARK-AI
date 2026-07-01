#include "main.h"
#include "motor.h"
#include "touch.h"
#include "ultrasion.h"
#include "color.h"
#include "blue.h"
#include "exfuns.h"
#include "monitor.h"
#include "bat_manager.h"
#include "matrix_port.h"
 

volatile bool is_iwdg = false;
volatile uint32_t spark_version;
volatile float bat = 0.0f;
uint32_t fatfs_total = 0;
uint32_t fatfs_free = 0;

/* 监控发送开关：true=允许发送 */
volatile bool usb_monitor_enabled = true;
volatile bool blue_monitor_enabled = true;

static EVENT_MANAGER event_t[] = {
 {"led3_event",500,led3_event_callback,NULL},
 {"iwdg_feedevent",10,iwdg_feed,NULL},
 {"usb_connect",10,usb_event_connect_callback,NULL},
 {"scan_adc",100,sample_adc_data_callback,NULL},
 {"usb_receive",0, usb_event_receive_callback,(USB_MESSAGE_BOX*)&usb_message},
 {"beep",1, beep_update,NULL},
 {"matrix_event",10, matrix_callback,NULL},
 {"key_middle_event",1, key_middle_callback,NULL},
 {"monitor_event",10,monitor_call_back,NULL},
 /* 脚本运行(主循环阻塞)时由中断接管监控发送，避免 USB/蓝牙断流。
    仅在 start_py||start_pauto 时实际发送(见 monitor.c)，空闲期仍走主循环。 */
 {"usb_monitor_send",1, monitor_send_usb,NULL},
 {"blue_monitor_send",50, monitor_send_blue,NULL}
};


/*Pauto Demo*/
#include "_motor.h"
#include "_key.h"
#include "_matrix.h"
#include "_touch.h"
#include "_color.h"
#include "_ultrasion.h"
#include "_os.h"
static void stop_motors_for_group(int group) {
    if (group == 0) {
        _motor_pair(NULL, 4, 5, 0);
        _motor_mov_set_stop_module(NULL, 1);
        _motor_mov_stop(NULL);
    } else if (group == 1) {
        _motor_pair(NULL, 6, 7, 0);
        _motor_mov_set_stop_module(NULL, 1);
        _motor_mov_stop(NULL);
    }
}
void pauto_play(void)
{ 
    extern volatile bool start_pauto;
    int mode = 1;
    int last_left = 0;
    int last_right = 0;
    int last_mode = 0;

    int port_connected[4] = {0};          // 0:无, 0xA2:颜色, 0xA3:超声波, 0xA4:触摸
    int port_allowed_touch[4] = {0};      // 允许使用的触摸端口
    int port_allowed_color[4] = {0};      // 允许使用的颜色端口
    int ultra_active_port = -1;           // 当前允许使用的超声波端口
    int ultra_active_group = -1;          // 当前超声波所在的组(0或1)

    int last_ultra_port = -1;
    int last_ultra_group = -1;

    while(start_pauto) { 
        int left = _key_key_mast(NULL, "left", 1);
        int right = _key_key_mast(NULL, "right", 1);
        if (left == 1 && last_left == 0) {
            mode = 1;
        }
        if (right == 1 && last_right == 0) {
            mode = 2;
        }
        last_left = left;
        last_right = right;

        if (mode != last_mode) {
            if (mode == 1) {
                _matrix_set_pixel_brightness(NULL, 2, 4, 1);
                _matrix_set_pixel_brightness(NULL, 3, 4, 0);
            } else {
                _matrix_set_pixel_brightness(NULL, 2, 4, 1);
                _matrix_set_pixel_brightness(NULL, 3, 4, 1);
            }
            last_mode = mode;
        }

        // 触摸传感器
        for (int port = 0; port < 4; port++) {
            if (port_allowed_touch[port]) {
                if (_touch_state(NULL, port) == 1) {
                    int power = (mode == 1) ? 100 : -100;
                    _motor_run_power(NULL, port + 4, power);
                } else {
                    _motor_stop(NULL, port + 4);
                }
            }
        }

        // 颜色传感器
        for (int port = 0; port < 4; port++) {
            if (port_allowed_color[port]) {
                int lux = _color_lux(NULL, port);   // 非阻塞读取
                if (mode == 1) {
                    if (100 > lux) {
                        _motor_run_power(NULL, port + 4, 100);
                    } else {
                        _motor_stop(NULL, port + 4);
                    }
                } else {
                    if (100 < lux) {
                        _motor_run_power(NULL, port + 4, -100);
                    } else {
                        _motor_stop(NULL, port + 4);
                    }
                }
            }
        }

        // 超声波传感器
        if (ultra_active_port != -1) {
            int dist = _ultrasion_value(NULL, ultra_active_port);
            if (mode == 1) {
                if (dist == 255 || dist <= 20) {
                    _motor_mov_stop(NULL);
                } else {
                    _motor_mov_power(NULL, 80, 80);
                }
            } else { // mode == 2  比例跟随
                #define TARGET_DIST 15
                #define MAX_SPEED 80
                #define MIN_SPEED 40
                #define Kp 7.5f

                if (dist == 255) {
                    _motor_mov_stop(NULL);
                } else {
                    int error = dist - TARGET_DIST;
                    float speed_f = error * Kp;
                    int speed = (int)speed_f;
                    if (speed > MAX_SPEED) speed = MAX_SPEED;
                    else if (speed < -MAX_SPEED) speed = -MAX_SPEED;
                    if (abs(speed) < MIN_SPEED) speed = 0;
                    _motor_mov_power(NULL, speed, speed);
                }
            }
        }

        // 4.1 读取所有端口连接状态
        int new_conn[4];
        for (int port = 0; port < 4; port++) {
            new_conn[port] = _os_get_port_linke(NULL, port);
        }

        // 4.2 确定每组中启用的超声波端口（端口号小的优先）
        int ultra_in_group[2] = {-1, -1};   // 组0:端口0/1, 组1:端口2/3
        for (int port = 0; port < 4; port++) {
            if (new_conn[port] == 0xA3) {
                int group = (port == 0 || port == 1) ? 0 : 1;
                if (ultra_in_group[group] == -1) {
                    ultra_in_group[group] = port;
                }
            }
        }

        // 4.3 更新允许标志
        int new_touch[4] = {0};
        int new_color[4] = {0};
        int new_ultra_port = -1;
        int new_ultra_group = -1;

        for (int port = 0; port < 4; port++) {
            int conn = new_conn[port];
            int group = (port == 0 || port == 1) ? 0 : 1;

            if (conn == 0xA4) {   // 触摸
                if (ultra_in_group[group] == -1 || ultra_in_group[group] == port) {
                    new_touch[port] = 1;
                }
            } else if (conn == 0xA2) { // 颜色
                if (ultra_in_group[group] == -1 || ultra_in_group[group] == port) {
                    new_color[port] = 1;
                }
            } else if (conn == 0xA3) { // 超声波
                if (ultra_in_group[group] == port) {
                    new_ultra_port = port;
                    new_ultra_group = group;
                }
            }
        }

        // 4.4 更新LED显示
        for (int port = 0; port < 4; port++) {
            int conn = new_conn[port];
            if (conn == 0) {
                _matrix_set_pixel_brightness(NULL, port * 2, 0, 0);
            } else {
                int allowed = (conn == 0xA4 && new_touch[port]) ||
                              (conn == 0xA2 && new_color[port]) ||
                              (conn == 0xA3 && new_ultra_port == port);
                if (allowed) {
                    _matrix_set_pixel_brightness(NULL, port * 2, 0, 1);
                } else {
                    _matrix_set_pixel_brightness(NULL, port * 2, 0, 0);
                }
            }
        }

        // 4.5 处理电机停止
        // 超声波组停止
        if (last_ultra_port != -1 && new_ultra_port == -1) {
            stop_motors_for_group(last_ultra_group);
        }

        // 非超声波端口停止（从允许变为不允许）
        for (int port = 0; port < 4; port++) {
            if (port_allowed_touch[port] && !new_touch[port]) {
                _motor_stop_module(NULL, port + 4, 1);
                _motor_stop(NULL, port + 4);
            }
            if (port_allowed_color[port] && !new_color[port]) {
                _motor_stop_module(NULL, port + 4, 1);
                _motor_stop(NULL, port + 4);
            }
        }

        // 4.6 如果超声波端口变化，重新配对
        if (new_ultra_port != last_ultra_port) {
            if (new_ultra_port != -1) {
                if (new_ultra_group == 0) {
                    _motor_pair(NULL, 4, 5, 0);
                } else {
                    _motor_pair(NULL, 6, 7, 0);
                }
                _motor_mov_set_stop_module(NULL, 1);
            }
        }

        // 4.7 更新全局标志
        for (int i = 0; i < 4; i++) {
            port_allowed_touch[i] = new_touch[i];
            port_allowed_color[i] = new_color[i];
        }
        ultra_active_port = new_ultra_port;
        ultra_active_group = new_ultra_group;
        last_ultra_port = new_ultra_port;
        last_ultra_group = new_ultra_group;
    }
}

 

OTA_PY_FILE usbOTAhandle;

/* 蓝牙OTA下载缓冲区：中断中缓存数据，主循环中处理 */
volatile bool ble_ota_pending = false;
_AGREEMENT ble_ota_frame;
void (*ble_ota_callback)(void *data, uint16_t length) = NULL;

static uint8_t check_swd_config(void)
{
    uint32_t mapr_value = AFIO->MAPR;
    uint8_t swj_cfg = (mapr_value >> 24) & 0x7;  
    return swj_cfg;
}

static void disable_jtag_enable_swd(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG_Msk;  
    AFIO->MAPR |= (0x1 << 24);

    if(check_swd_config() != 0x2)
    {
			  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
        while(1);
    }
}
 
static void Main_Loop_Process(void)
{
	extern volatile bool start_py;
     if (Key_Check_Short_Press())
    {
        beep_play_ui_transition();
        uint32_t speed_ms = UI_TRANSITION_DURATION_MS / MATRIX_ROWS;
        if (speed_ms < 10) speed_ms = 10; // 最小速度
        const UI_Item *item = ui_manager_get_current_item();
				if(item!=NULL)
				{ 
					if(strcmp(item->name,"blue") == 0)
				 {
				    DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(PORT_BLUE));
					  if(blue!=NULL)
						{
						  blue->is_off_on=!blue->is_off_on;							 
							if(blue->is_off_on){
								blue_set_on();
							  blue->cfg.on_off = 1;
							}							 
							else
							{
							  blue_set_off();
								blue->cfg.on_off = 0;
							}
								 							
							/*记录蓝牙状态*/
							if(fatfs_create_file("blue_cfg.cfg",(BLUE_CFG*)&blue->cfg,sizeof(BLUE_CFG))!=FR_OK)
							{
							  usb_printf("->cfg write error\r\n");
							}
						}
						start_py = false;
				 }
				 else
				 { 
           animation_vertical_scroll(DIRECTION_UP, speed_ms);
           uint32_t animation_start_time = HAL_GetTick();
           while (HAL_GetTick() - animation_start_time < UI_TRANSITION_DURATION_MS) {
            animation_update();
            HAL_Delay(10); // 短暂延时
           }
				    run_python(item->name);
						start_py = false;
						ui_manager_refresh_current();	  
				 }
				}       		   		   			
    }

    if (Key_Check_Long_Press())
    {
			  Time_SaveToFlash();
			   
			  delay_ms(500);

        display_play_power_off_animation();
        delay_ms(1000);
          
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

        while(1);
    }
		extern volatile bool is_refresh_matrix;
		if(is_refresh_matrix)
		{
		  ui_manager_refresh_current();
      is_refresh_matrix = false;			
		}

}
 

 
void busDataparsing(_AGREEMENT *frame,void (*port_transerf_data)(void *data,uint16_t length))
{ 
	 extern volatile bool start_py;
	 extern volatile bool start_pauto;
   switch(frame->index)
	 { 
     case 0x6F:
		   f_unlink("updata.txt");
			 is_iwdg = true;
		 break;
		 case 0xC3:		  
		//	fm_print_simple_usage();
		 break;
		 case 0xC4:
			 
		 break;
		 case 0xB6:
		 case 0xB9:
			if(start_py)
			{__exitpython();start_pauto = false;}
			else
			{start_py = true;set_entery_short(1);} 
		 break;
		 case 0xEF:	  
     break;
		 case 0xBE:
			   set_event_disable("monitor_event");
			   usb_monitor_enabled = false;
			   blue_monitor_enabled = false;
		 break;
		 case 0xBA:
			   set_event_enable("monitor_event");
			   usb_monitor_enabled = true;
			   blue_monitor_enabled = true;
		 break;
		 default:
		   if(start_py)
		   {
			   extern void usb_printf(char *fmt, ...);
			   usb_printf("[BLE_OTA] drop by start_py: idx=0x%02X\r\n", frame->index);
			   break;
		   }
		 
		   memset((OTA_PY_FILE*)&usbOTAhandle,0,sizeof(OTA_PY_FILE));
		   memcpy((_AGREEMENT*)&usbOTAhandle.frame,frame,sizeof(_AGREEMENT));
		   usbOTAhandle.port_transerf_data = port_transerf_data;
		   usbOTAhandle.is_refresh_data = true;
			//	touchFile(frame->index,frame->data,frame->length,port_transerf_data);
		 break;
	 }
}
 
static void systemInit(void)
{ 
   HAL_Init();                  
   sys_stm32_clock_init(RCC_PLL_MUL15);	
   delay_init(120);   	
	 disable_jtag_enable_swd();
	 my_mem_init(SRAMIN);	
	 btim_timx_int_init(120-1,1000-1);
	 usb_cdc_init();
   uart_port_init();
	 adc_nch_dma_init();
	 battery_manager_init();
	
	 led_init(); 
	 key_init();	  
	 iic_init();  
	 pwm_init();
	 beep_init();
   init_identify_dev();	  
   reset_usb_parser();       
	 __enable_irq();
	 exfuns_fatfs_init();		 
	 blue_init();
	 Time_Init();
	  
	 Key_Config_Params(10, 1500, 500); 
   iwdg_init(IWDG_PRESCALER_64, 625); 
	 for(uint32_t i = 0;i<sizeof(event_t)/sizeof(event_t[0]);i++)
			create_event_manger(&event_t[i]);
	 animation_register_buzzer_callback(beep_play_power_on, beep_play_power_off); 	      
	 display_init();
	 refreshFwlibInfo();
	 exfuns_get_free((uint8_t*)"0:",&fatfs_total,&fatfs_free);
   uart_dma_idle_start();	 
   set_event_disable("monitor_event");
}

int main(void)
{
    sys_nvic_set_vector_table(FLASH_BASE,0x10000);   
    systemInit();	
    while(1){
			ui_manager_update();
      Main_Loop_Process();			
			if(usbOTAhandle.is_refresh_data)
			{ 
				if(usbOTAhandle.port_transerf_data!=NULL)
				{ 
					/* OTA下载开始前关闭监控 */
					if(usbOTAhandle.frame.index == 0xDA)
					{
						usb_monitor_enabled = false;
						blue_monitor_enabled = false;
					}
			    
			   touchOtherFile(usbOTAhandle.frame.index,
									 usbOTAhandle.frame.data,
									 usbOTAhandle.frame.length,
									 usbOTAhandle.port_transerf_data);
				  				   
				}
				usbOTAhandle.is_refresh_data = false;
				/* OTA下载完成帧或错误后恢复监控 */
				if(usbOTAhandle.frame.index == 0xBB || usbOTAhandle.frame.index == 0xBC)
				{
					usb_monitor_enabled = true;
					blue_monitor_enabled = true;
				}
			}
			/* 处理蓝牙OTA下载（通过中断缓存 + 主循环处理） */
			if(ble_ota_pending)
			{ 
				if(ble_ota_callback != NULL)
				{
					/* OTA下载开始前关闭监控 */
					if(ble_ota_frame.index == 0xDA)
					{
						usb_monitor_enabled = false;
						blue_monitor_enabled = false;
					}
					touchOtherFile(ble_ota_frame.index,
								   ble_ota_frame.data,
								   ble_ota_frame.length,
								   ble_ota_callback);
				}
				ble_ota_pending = false;
				/* OTA下载完成帧或错误后恢复监控 */
				if(ble_ota_frame.index == 0xBB || ble_ota_frame.index == 0xBC)
				{
					usb_monitor_enabled = true;
					blue_monitor_enabled = true;
				}
			}
     check_battery_with_debounce();
		 /* USB监控 */
		 if(usb_monitor_enabled)
		 {
			 monitor_call_back(NULL);
			 usb_printf("%s\r\n", monitor_get_json());
		 }
		 /* 蓝牙监控发送：25ms周期，非阻塞，仅蓝牙连接时发送 */
		 if(blue_monitor_enabled)
		 {
			 static uint32_t blue_monitor_tick = 0;
			 DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(PORT_BLUE));
			 if(blue != NULL && blue->is_off_on)
			 {
				 blue_monitor_tick += 5;
				 if(blue_monitor_tick >= 25)
				 {
					 blue_monitor_tick = 0;
					 monitor_call_back(NULL);
					 blue_printf("%s\r\n", monitor_get_json());
				 }
			 }
		 }
		 delay_ms(5);
	 }
}

