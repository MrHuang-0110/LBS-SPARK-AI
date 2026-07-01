
#include "./BSP/KEY/key.h"
#include "./SYSTEM/delay/delay.h"
#include "pikaVM.h"
#include "event_manager.h"
#include "beep.h"
#include "exfuns.h"
volatile bool start_py = false,start_pauto = false;
static REMOTE_CFG remote_cfg;

// 按键句柄（假设按键连接到KEY0）
static Key_Scan_Handle_t key_handle = {
    .port = KEY1_GPIO_PORT,
    .pin = KEY1_GPIO_PIN,
    .active_level = GPIO_PIN_RESET,  // 假设按下为低电平
    
    .state = KEY_STATE_IDLE,
    .short_press_flag = 0,
    .long_press_flag = 0,
    .long_hold_flag = 0,
    
    .press_start_time = 0,
    .press_duration = 0,
    .last_long_hold_time = 0,
    
    .debounce_time = 10,          // 10ms消抖
    .long_press_time = 1500,      // 1.5秒长按
    .long_hold_interval = 500,    // 500ms长按反馈间隔
    
    .current_raw_state = 1,
    .last_raw_state = 1
};

void set_entery_short(uint8_t key)
{ 
		key_handle.short_press_flag = key;
}


/**
 * @brief  按键扫描函数（在定时器中断中调用）
 * @param  scan_interval_ms: 扫描间隔时间(ms)，用于计算持续时间
 * @note   需要在定时器中断中定期调用，建议10ms调用一次
 */
void Key_Scan_Handler(uint32_t scan_interval_ms)
{
	  extern volatile bool start_py;
	  extern volatile bool start_pauto;
    // 1. 读取当前按键原始状态
    key_handle.current_raw_state = (HAL_GPIO_ReadPin(key_handle.port, key_handle.pin) 
                                    == key_handle.active_level) ? 1 : 0;
    
    // 2. 检测边沿变化
    uint8_t falling_edge = (key_handle.current_raw_state == 1 && key_handle.last_raw_state == 0);
    uint8_t rising_edge = (key_handle.current_raw_state == 0 && key_handle.last_raw_state == 1);
    
    // 3. 更新上一次状态
    key_handle.last_raw_state = key_handle.current_raw_state;
    
    // 4. 状态机处理
    switch (key_handle.state)
    {
        case KEY_STATE_IDLE:
            if (key_handle.current_raw_state == 1)
            {
                // 按键按下，进入消抖状态
                key_handle.state = KEY_STATE_PRESS_DB;
                key_handle.press_start_time = HAL_GetTick();
                key_handle.press_duration = 0;
            }
            break;
            
        case KEY_STATE_PRESS_DB:
            if (key_handle.current_raw_state == 0)
            {
                // 在消抖期间按键释放，可能是抖动，回到空闲
                key_handle.state = KEY_STATE_IDLE;
            }
            else
            {
                // 消抖计时
                if (HAL_GetTick() - key_handle.press_start_time >= key_handle.debounce_time)
                {
                    // 消抖完成，确认按下
                    key_handle.state = KEY_STATE_PRESSED;
                    // 可以在这里添加按键音反馈
                   // beep_play_key_press();
                }
            }
            break;
            
        case KEY_STATE_PRESSED:
            if (key_handle.current_raw_state == 0)
            {
                // 按键释放，进入释放消抖
                key_handle.state = KEY_STATE_RELEASE_DB;
            }
            else
            {
							  if(start_py)break;
                // 持续按下，检查是否达到长按时间
                key_handle.press_duration = HAL_GetTick() - key_handle.press_start_time;
						    uint32_t duration = Key_Get_Press_Duration();
								if (duration > 0 && (duration % 500) == 0)
								{
									// 每500ms播放一次按键音（长按反馈）
									beep_play_key_press();
								}               
                if (key_handle.press_duration >= key_handle.long_press_time)
                {
                    // 触发长按事件
                    key_handle.long_press_flag = 1;
                    key_handle.state = KEY_STATE_LONG_PRESS;
                }
            }
            break;
            
        case KEY_STATE_LONG_PRESS:
					   
            if (key_handle.current_raw_state == 0)
            {
                // 长按后释放，进入释放消抖
                key_handle.state = KEY_STATE_RELEASE_DB;
            }
            else
            {
                // 长按保持中
                key_handle.press_duration = HAL_GetTick() - key_handle.press_start_time;
                
                // 检查长按保持反馈间隔
                if (key_handle.press_duration - key_handle.last_long_hold_time >= key_handle.long_hold_interval)
                {
                    // 触发长按保持反馈
                    key_handle.long_hold_flag = 1;
                    key_handle.last_long_hold_time = key_handle.press_duration;
                    // 可以在这里添加反馈音
                   // beep_play_key_press();
                }
            }
            break;
            
        case KEY_STATE_RELEASE_DB:
            if (key_handle.current_raw_state == 1)
            {
                // 在释放消抖期间又按下，回到按下状态
                key_handle.state = KEY_STATE_PRESSED;
            }
            else
            {
                // 释放消抖完成
                if (HAL_GetTick() - (key_handle.press_start_time + key_handle.press_duration) >= key_handle.debounce_time)
                {
                    // 检查是否短按（按下时间小于长按时间）
                    if (key_handle.press_duration < key_handle.long_press_time)
                    {
											  extern void __exitpython(void);
											  if(start_py)
												{__exitpython();start_pauto = false;}
												else
												{start_py = true; key_handle.short_press_flag = 1;}  
											  
                    }
                    
                    // 回到空闲状态
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
/**
 * @brief  检查是否有短按事件
 * @retval 1:有短按事件，0:无短按事件
 * @note   调用后自动清除标志
 */
uint8_t Key_Check_Short_Press(void)
{
    if (key_handle.short_press_flag)
    {
        key_handle.short_press_flag = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief  检查是否有长按事件
 * @retval 1:有长按事件，0:无长按事件
 * @note   调用后自动清除标志
 */
uint8_t Key_Check_Long_Press(void)
{
    if (key_handle.long_press_flag)
    {
        key_handle.long_press_flag = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief  检查是否有长按保持事件
 * @retval 1:有长按保持事件，0:无长按保持事件
 * @note   调用后自动清除标志
 */
uint8_t Key_Check_Long_Hold(void)
{
    if (key_handle.long_hold_flag)
    {
        key_handle.long_hold_flag = 0;
        return 1;
    }
    return 0;
}
/**
 * @brief  获取按键当前按下持续时间
 * @retval 持续时间(ms)，0表示未按下
 */
uint32_t Key_Get_Press_Duration(void)
{
    if (key_handle.current_raw_state == 1)
    {
        return HAL_GetTick() - key_handle.press_start_time;
    }
    return 0;
}

/**
 * @brief  检查按键当前是否按下
 * @retval 1:按下，0:未按下
 */
uint8_t Key_Is_Pressed(void)
{
    return (key_handle.state == KEY_STATE_PRESSED || 
            key_handle.state == KEY_STATE_LONG_PRESS);
}

/**
 * @brief  重置按键状态（强制回到空闲）
 */
void Key_Reset_State(void)
{
    key_handle.state = KEY_STATE_IDLE;
    key_handle.short_press_flag = 0;
    key_handle.long_press_flag = 0;
    key_handle.long_hold_flag = 0;
    key_handle.last_long_hold_time = 0;
}
/**
 * @brief  按键配置参数设置
 * @param  debounce_ms: 消抖时间(ms)
 * @param  long_press_ms: 长按判定时间(ms)
 * @param  hold_interval_ms: 长按保持反馈间隔(ms)
 */
void Key_Config_Params(uint16_t debounce_ms, uint16_t long_press_ms, uint16_t hold_interval_ms)
{
    key_handle.debounce_time = debounce_ms;
    key_handle.long_press_time = long_press_ms;
    key_handle.long_hold_interval = hold_interval_ms;
}

void key_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    KEY1_GPIO_CLK_ENABLE();                                     /* KEY0时钟使能 */
    KEY2_GPIO_CLK_ENABLE();                                     /* KEY1时钟使能 */
    KEY3_GPIO_CLK_ENABLE();                                     /* WKUP时钟使能 */

    gpio_init_struct.Pin = KEY1_GPIO_PIN;                       /* KEY0引脚 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                    /* 输入 */
    gpio_init_struct.Pull = GPIO_NOPULL;                        /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;              /* 高速 */
    HAL_GPIO_Init(KEY1_GPIO_PORT, &gpio_init_struct);           /* KEY0引脚模式设置,上拉输入 */

    gpio_init_struct.Pin = KEY2_GPIO_PIN;                       /* KEY1引脚 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                    /* 输入 */
    gpio_init_struct.Pull = GPIO_NOPULL;                        /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;              /* 高速 */
    HAL_GPIO_Init(KEY2_GPIO_PORT, &gpio_init_struct);           /* KEY1引脚模式设置,上拉输入 */

    gpio_init_struct.Pin = KEY3_GPIO_PIN;                       /* WKUP引脚 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                    /* 输入 */
    gpio_init_struct.Pull = GPIO_NOPULL;                      /* 下拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;              /* 高速 */
    HAL_GPIO_Init(KEY3_GPIO_PORT, &gpio_init_struct);           /* WKUP引脚模式设置,下拉输入 */		
		
    gpio_init_struct.Pin = BLUE_STA_GPIO_PIN;                       /* WKUP引脚 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                    /* 输入 */
    gpio_init_struct.Pull = GPIO_NOPULL;                      /* 下拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;              /* 高速 */
    HAL_GPIO_Init(BLUE_STA_PORT, &gpio_init_struct);           /* WKUP引脚模式设置,下拉输入 */			
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
