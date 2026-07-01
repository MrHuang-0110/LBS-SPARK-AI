/**
 ****************************************************************************************************
 * @file        btim.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-20
 * @brief       基本定时器 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211216
 * 第一次发布
 *
 ****************************************************************************************************
 */

#ifndef __BTIM_H
#define __BTIM_H

#include "./SYSTEM/sys/sys.h"

#define PWM_MAX (100)

/******************************************************************************************/
/* 基本定时器 定义 */

#define BTIM_TIMX_INT                       TIM6
#define BTIM_TIMX_INT_IRQn                  TIM6_DAC_IRQn
#define BTIM_TIMX_INT_IRQHandler            TIM6_DAC_IRQHandler
#define BTIM_TIMX_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM6_CLK_ENABLE(); }while(0)  


#define GTIM_TIM2_INT                       TIM2
#define GTIM_TIM2_INT_IRQn                  TIM2_IRQn
#define GTIM_TIM2_INT_IRQHandler            TIM2_IRQHandler
#define GTIM_TIM2_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)  

#define GTIM_TIM3_INT                       TIM3
#define GTIM_TIM3_INT_IRQn                  TIM3_IRQn
#define GTIM_TIM3_INT_IRQHandler            TIM3_IRQHandler
#define GTIM_TIM3_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0) 

#define GTIM_M1A_PWM_CHY_GPIO_PORT         GPIOA
#define GTIM_M1A_PWM_CHY_GPIO_PIN          GPIO_PIN_15
#define GTIM_M1A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)  

#define GTIM_M1A_PWM                       TIM2 
#define GTIM_M1A_PWM_CHY                   TIM_CHANNEL_1                                
#define GTIM_M1A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR1                          
#define GTIM_M1A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)  

#define GTIM_M1B_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M1B_PWM_CHY_GPIO_PIN          GPIO_PIN_3
#define GTIM_M1B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  

#define GTIM_M1B_PWM                       TIM2 
#define GTIM_M1B_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_M1B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                          
#define GTIM_M1B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)   


#define GTIM_M2A_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M2A_PWM_CHY_GPIO_PIN          GPIO_PIN_6
#define GTIM_M2A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M2A_PWM                       TIM3 
#define GTIM_M2A_PWM_CHY                   TIM_CHANNEL_1                                
#define GTIM_M2A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR1                         
#define GTIM_M2A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  

#define GTIM_M2B_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M2B_PWM_CHY_GPIO_PIN          GPIO_PIN_7
#define GTIM_M2B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M2B_PWM                       TIM3 
#define GTIM_M2B_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_M2B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                         
#define GTIM_M2B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  


#define GTIM_M3A_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M3A_PWM_CHY_GPIO_PIN          GPIO_PIN_8
#define GTIM_M3A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)  

#define GTIM_M3A_PWM                       TIM3 
#define GTIM_M3A_PWM_CHY                   TIM_CHANNEL_3                                
#define GTIM_M3A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR3                          
#define GTIM_M3A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  

#define GTIM_M3B_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M3B_PWM_CHY_GPIO_PIN          GPIO_PIN_9
#define GTIM_M3B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M3B_PWM                       TIM3
#define GTIM_M3B_PWM_CHY                   TIM_CHANNEL_4                               
#define GTIM_M3B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR4                         
#define GTIM_M3B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  


#define GTIM_M4A_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M4A_PWM_CHY_GPIO_PIN          GPIO_PIN_8
#define GTIM_M4A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  

#define GTIM_M4A_PWM                       TIM4 
#define GTIM_M4A_PWM_CHY                   TIM_CHANNEL_3                                
#define GTIM_M4A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR3                          
#define GTIM_M4A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  

#define GTIM_M4B_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M4B_PWM_CHY_GPIO_PIN          GPIO_PIN_9
#define GTIM_M4B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   

#define GTIM_M4B_PWM                       TIM4
#define GTIM_M4B_PWM_CHY                   TIM_CHANNEL_4                               
#define GTIM_M4B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR4                         
#define GTIM_M4B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  


#define GTIM_BUZZ_PWM_CHY_GPIO_PORT         GPIOA
#define GTIM_BUZZ_PWM_CHY_GPIO_PIN          GPIO_PIN_1
#define GTIM_BUZZ_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   

#define GTIM_BUZZ_PWM                       TIM5
#define GTIM_BUZZ_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_BUZZ_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                        
#define GTIM_BUZZ_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM5_CLK_ENABLE(); }while(0)  

typedef struct {
    uint16_t year;   // 年份，如2025
    uint8_t  month;  // 月份 1-12
    uint8_t  day;    // 日期 1-31
    uint8_t  hour;   // 小时 0-23
    uint8_t  minute; // 分钟 0-59
    uint8_t  second; // 秒 0-59
} DateTime_t;

uint32_t getTim6Tick(void);
void btim_timx_int_init(uint16_t arr, uint16_t psc);    /* 基本定时器 定时中断初始化函数 */
void pwm_init(void);

void pwm_set_output(uint8_t id,int pwm);
void pwm_stop_breaking(uint8_t id);
void pwm_stop_slide(uint8_t id);


void resetUserCPUTick(void);
uint32_t getUserCPUTick(void);


void Time_Init(void);
// 设置系统时间
void Time_Set(DateTime_t *dt);
// 获取当前系统时间
void Time_Get(DateTime_t *dt);
// 增加秒数（用于校准或跳转）
void Time_AddSeconds(int32_t seconds);
// 将当前时间写入FLASH（掉电保存）
void Time_SaveToFlash(void);
// 从FLASH加载时间（上电时调用）
void Time_LoadFromFlash(void);


void SavePowerOnStartTimer(void);
void SavePowerDownTimer(void);

void GetPowerOnStartTimer(DateTime_t *dt);
void GetPoweDownTimer(DateTime_t *dt);

#endif

















