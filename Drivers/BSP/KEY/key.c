
#include "./BSP/KEY/key.h"
#include "./SYSTEM/delay/delay.h"
#include "pikaVM.h"
#include "event_manager.h"
#include "beep.h"
#include "exfuns.h"
volatile bool start_py = false,start_pauto = false;
static REMOTE_CFG remote_cfg;

 
static Key_Scan_Handle_t key_handle = {
    .port = KEY1_GPIO_PORT,
    .pin = KEY1_GPIO_PIN,
    .active_level = GPIO_PIN_RESET,  
    
    .state = KEY_STATE_IDLE,
    .short_press_flag = 0,
    .long_press_flag = 0,
    .long_hold_flag = 0,
    
    .press_start_time = 0,
    .press_duration = 0,
    .last_long_hold_time = 0,
    
    .debounce_time = 10,          
    .long_press_time = 1500,      
    .long_hold_interval = 500,     
    
    .current_raw_state = 1,
    .last_raw_state = 1
};

void set_entery_short(uint8_t key)
{ 
		key_handle.short_press_flag = key;
}

 
void Key_Scan_Handler(uint32_t scan_interval_ms)
{
	  extern volatile bool start_py;
	  extern volatile bool start_pauto;
 
    key_handle.current_raw_state = (HAL_GPIO_ReadPin(key_handle.port, key_handle.pin) 
                                    == key_handle.active_level) ? 1 : 0;
    
 
    uint8_t falling_edge = (key_handle.current_raw_state == 1 && key_handle.last_raw_state == 0);
    uint8_t rising_edge = (key_handle.current_raw_state == 0 && key_handle.last_raw_state == 1);
    
 
    key_handle.last_raw_state = key_handle.current_raw_state;
    
 
    switch (key_handle.state)
    {
        case KEY_STATE_IDLE:
            if (key_handle.current_raw_state == 1)
            {
 
                key_handle.state = KEY_STATE_PRESS_DB;
                key_handle.press_start_time = HAL_GetTick();
                key_handle.press_duration = 0;
            }
            break;
            
        case KEY_STATE_PRESS_DB:
            if (key_handle.current_raw_state == 0)
            {
 
                key_handle.state = KEY_STATE_IDLE;
            }
            else
            {
 
                if (HAL_GetTick() - key_handle.press_start_time >= key_handle.debounce_time)
                {
 
                    key_handle.state = KEY_STATE_PRESSED;
  
                   // beep_play_key_press();
                }
            }
            break;
            
        case KEY_STATE_PRESSED:
            if (key_handle.current_raw_state == 0)
            {
               
                key_handle.state = KEY_STATE_RELEASE_DB;
            }
            else
            {
							  if(start_py)break;
                
                key_handle.press_duration = HAL_GetTick() - key_handle.press_start_time;
						    uint32_t duration = Key_Get_Press_Duration();
								if (duration > 0 && (duration % 500) == 0)
								{
					 
									beep_play_key_press();
								}               
                if (key_handle.press_duration >= key_handle.long_press_time)
                {
                    
                    key_handle.long_press_flag = 1;
                    key_handle.state = KEY_STATE_LONG_PRESS;
                }
            }
            break;
            
        case KEY_STATE_LONG_PRESS:
					   
            if (key_handle.current_raw_state == 0)
            {
                
                key_handle.state = KEY_STATE_RELEASE_DB;
            }
            else
            {
                
                key_handle.press_duration = HAL_GetTick() - key_handle.press_start_time;
                
                
                if (key_handle.press_duration - key_handle.last_long_hold_time >= key_handle.long_hold_interval)
                {
       
                    key_handle.long_hold_flag = 1;
                    key_handle.last_long_hold_time = key_handle.press_duration;
              
                   // beep_play_key_press();
                }
            }
            break;
            
        case KEY_STATE_RELEASE_DB:
            if (key_handle.current_raw_state == 1)
            {
               
                key_handle.state = KEY_STATE_PRESSED;
            }
            else
            {
                
                if (HAL_GetTick() - (key_handle.press_start_time + key_handle.press_duration) >= key_handle.debounce_time)
                {
                    
                    if (key_handle.press_duration < key_handle.long_press_time)
                    {
											  extern void exit_python(void);
											  if(start_py)
												{exit_python();start_pauto = false;}
												else
												{start_py = true; key_handle.short_press_flag = 1;}  
											  
                    }
                    
                     
                    key_handle.state = KEY_STATE_IDLE;
                    key_handle.long_hold_flag = 0;
                    key_handle.last_long_hold_time = 0;
                }
            }
            break;
            
        default:
            key_handle.state = KEY_STATE_IDLE;
            break;
    }
}
 
uint8_t Key_Check_Short_Press(void)
{
    if (key_handle.short_press_flag)
    {
        key_handle.short_press_flag = 0;
        return 1;
    }
    return 0;
}

 
uint8_t Key_Check_Long_Press(void)
{
    if (key_handle.long_press_flag)
    {
        key_handle.long_press_flag = 0;
        return 1;
    }
    return 0;
}

 
uint8_t Key_Check_Long_Hold(void)
{
    if (key_handle.long_hold_flag)
    {
        key_handle.long_hold_flag = 0;
        return 1;
    }
    return 0;
}
 
uint32_t Key_Get_Press_Duration(void)
{
    if (key_handle.current_raw_state == 1)
    {
        return HAL_GetTick() - key_handle.press_start_time;
    }
    return 0;
}

 
uint8_t Key_Is_Pressed(void)
{
    return (key_handle.state == KEY_STATE_PRESSED || 
            key_handle.state == KEY_STATE_LONG_PRESS);
}

 
void Key_Reset_State(void)
{
    key_handle.state = KEY_STATE_IDLE;
    key_handle.short_press_flag = 0;
    key_handle.long_press_flag = 0;
    key_handle.long_hold_flag = 0;
    key_handle.last_long_hold_time = 0;
}
 
void Key_Config_Params(uint16_t debounce_ms, uint16_t long_press_ms, uint16_t hold_interval_ms)
{
    key_handle.debounce_time = debounce_ms;
    key_handle.long_press_time = long_press_ms;
    key_handle.long_hold_interval = hold_interval_ms;
}

void key_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    KEY1_GPIO_CLK_ENABLE();                                      
    KEY2_GPIO_CLK_ENABLE();                                     
    KEY3_GPIO_CLK_ENABLE();                                     

    gpio_init_struct.Pin = KEY1_GPIO_PIN;                       
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                    
    gpio_init_struct.Pull = GPIO_NOPULL;                        
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;               
    HAL_GPIO_Init(KEY1_GPIO_PORT, &gpio_init_struct);            

    gpio_init_struct.Pin = KEY2_GPIO_PIN;                       
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                     
    gpio_init_struct.Pull = GPIO_NOPULL;                        
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;             
    HAL_GPIO_Init(KEY2_GPIO_PORT, &gpio_init_struct);           

    gpio_init_struct.Pin = KEY3_GPIO_PIN;                      
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                   
    gpio_init_struct.Pull = GPIO_NOPULL;                     
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;             
    HAL_GPIO_Init(KEY3_GPIO_PORT, &gpio_init_struct);          		
		
    gpio_init_struct.Pin = BLUE_STA_GPIO_PIN;                     
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                  
    gpio_init_struct.Pull = GPIO_NOPULL;                     
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;             
    HAL_GPIO_Init(BLUE_STA_PORT, &gpio_init_struct);          		
}

int read_advance_offset1(void)
{ 
   return remote_cfg.advance_offset1;
}
int read_advance_offset2(void)
{ 
   return remote_cfg.advance_offset2;
}
int read_retreat_offset1(void)
{ 
   return remote_cfg.retreat_offset1;
}
int read_retreat_offset2(void)
{ 
   return remote_cfg.retreat_offset2;
}

void write_advance_remote_cfg(int offset1,int offset2)
{ 
	 remote_cfg.advance_offset1 = offset1;
	 remote_cfg.advance_offset2 = offset2;
	
	 if(fatfs_create_file("remote_cfg.cfg",(REMOTE_CFG*)&remote_cfg,sizeof(REMOTE_CFG))!=FR_OK)
	 { 
		  memset((REMOTE_CFG*)&remote_cfg,0,sizeof(REMOTE_CFG));
		  return;    
	 }
}

void write_retreat_remote_cfg(int offset1,int offset2)
{ 
	 remote_cfg.retreat_offset1 = offset1;
	 remote_cfg.retreat_offset2 = offset2;
	
	 if(fatfs_create_file("remote_cfg.cfg",(REMOTE_CFG*)&remote_cfg,sizeof(REMOTE_CFG))!=FR_OK)
	 { 
		  memset((REMOTE_CFG*)&remote_cfg,0,sizeof(REMOTE_CFG));
		  return;    
	 }
}

void loader_remote_cfg(void)
{ 
	 if(fatfs_read_file("remote_cfg.cfg",(REMOTE_CFG*)&remote_cfg,sizeof(REMOTE_CFG))!=FR_OK)
	 { 
		  memset((REMOTE_CFG*)&remote_cfg,0,sizeof(REMOTE_CFG));
		  return;    
	 }    
}

void key_middle_callback(void *arg)
{ 
   Key_Scan_Handler(10);
}
