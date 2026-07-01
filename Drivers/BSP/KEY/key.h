
#ifndef __KEY_H
#define __KEY_H

#include "./SYSTEM/sys/sys.h"
// 按键状态管理结构体
typedef struct {
    // 按键硬件配置
    GPIO_TypeDef* port;
    uint16_t pin;
    GPIO_PinState active_level;  // 按下时的电平
    
    // 状态管理
    enum {
        KEY_STATE_IDLE = 0,      // 空闲
        KEY_STATE_PRESS_DB,      // 按下消抖
        KEY_STATE_PRESSED,       // 已按下
        KEY_STATE_LONG_PRESS,    // 长按触发
        KEY_STATE_RELEASE_DB     // 释放消抖
    } state;
    
    // 事件标志
    volatile uint8_t short_press_flag;
    volatile uint8_t long_press_flag;
    volatile uint8_t long_hold_flag;
    
    // 时间记录
    uint32_t press_start_time;
    uint32_t press_duration;
    uint32_t last_long_hold_time;
    
    // 配置参数
    uint16_t debounce_time;      // 消抖时间(ms)
    uint16_t long_press_time;    // 长按判定时间(ms)
    uint16_t long_hold_interval; // 长按保持反馈间隔(ms)
    
    // 按键原始状态
    uint8_t current_raw_state;
    uint8_t last_raw_state;
} Key_Scan_Handle_t;

typedef struct{ 
  int advance_offset1,advance_offset2;
	int retreat_offset1,retreat_offset2;
}REMOTE_CFG;

/******************************************************************************************/
/* 引脚 定义 */
#if WIALL_HARDWARE_ENABLE
	#define KEY1_GPIO_PORT                  GPIOC
	#define KEY1_GPIO_PIN                   GPIO_PIN_4
	#define KEY1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */

	#define KEY2_GPIO_PORT                  GPIOC
	#define KEY2_GPIO_PIN                   GPIO_PIN_3
	#define KEY2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

	#define KEY3_GPIO_PORT                  GPIOC
	#define KEY3_GPIO_PIN                   GPIO_PIN_2
	#define KEY3_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */
	
	#define BLUE_STA_PORT                  GPIOA
	#define BLUE_STA_GPIO_PIN              GPIO_PIN_8
	#define BLUE_STA_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */	
#else
	#define KEY0_GPIO_PORT                  GPIOC
	#define KEY0_GPIO_PIN                   GPIO_PIN_5
	#define KEY0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */

	#define KEY1_GPIO_PORT                  GPIOA
	#define KEY1_GPIO_PIN                   GPIO_PIN_15
	#define KEY1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

	#define WKUP_GPIO_PORT                  GPIOA
	#define WKUP_GPIO_PIN                   GPIO_PIN_0
	#define WKUP_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */
#endif

 

/******************************************************************************************/

#define KEY0        HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN)     /* 读取KEY0引脚 0按下 1弹起 中键*/
#define KEY1        HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN)     /* 读取KEY1引脚 0按下，1弹起 左键*/
#define WK_UP       HAL_GPIO_ReadPin(KEY3_GPIO_PORT, KEY3_GPIO_PIN)     /* 读取WKUP引脚 0按下，1弹起 右键*/
#define BLUE_STA    HAL_GPIO_ReadPin(BLUE_STA_PORT, BLUE_STA_GPIO_PIN)     /* 读取WKUP引脚 0按下，1弹起 右键*/
 
void key_init(void);                /* 按键初始化函数 */
void Key_Config_Params(uint16_t debounce_ms, uint16_t long_press_ms, uint16_t hold_interval_ms);
void Key_Reset_State(void);
uint8_t Key_Is_Pressed(void);
uint32_t Key_Get_Press_Duration(void);
uint8_t Key_Check_Long_Hold(void);
uint8_t Key_Check_Long_Press(void);
uint8_t Key_Check_Short_Press(void);
void Key_Scan_Handler(uint32_t scan_interval_ms);
void key_middle_callback(void *arg);
void set_entery_short(uint8_t key);
 
void loader_remote_cfg(void);
void write_advance_remote_cfg(int offset1,int offset2);
void write_retreat_remote_cfg(int offset1,int offset2);
int read_advance_offset1(void);
int read_advance_offset2(void);
int read_retreat_offset1(void);
int read_retreat_offset2(void);
#endif


















