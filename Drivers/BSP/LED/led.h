
#ifndef _LED_H
#define _LED_H
#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* 引脚 定义 */
 
#if WIALL_HARDWARE_ENABLE
#define LED0_GPIO_PORT                  GPIOC
#define LED0_GPIO_PIN                   GPIO_PIN_13
#define LED0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PA口时钟使能 */

#define LED1_GPIO_PORT                  GPIOC
#define LED1_GPIO_PIN                   GPIO_PIN_14
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PD口时钟使能 */

#define LED2_GPIO_PORT                  GPIOC
#define LED2_GPIO_PIN                   GPIO_PIN_15
#define LED2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PD口时钟使能 */ 
#else

#define LED0_GPIO_PORT                  GPIOA
#define LED0_GPIO_PIN                   GPIO_PIN_8
#define LED0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)             /* PA口时钟使能 */

#define LED1_GPIO_PORT                  GPIOA
#define LED1_GPIO_PIN                   GPIO_PIN_2
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)             /* PD口时钟使能 */

#endif

/******************************************************************************************/
/* LED端口定义 */
#define LED0(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED0翻转 */

#define LED1(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED1翻转 */
#define LED2(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED1翻转 */

/* LED取反定义 */
#define LED0_TOGGLE()   do{ HAL_GPIO_TogglePin(LED0_GPIO_PORT, LED0_GPIO_PIN); }while(0)        /* 翻转LED0 */
#define LED1_TOGGLE()   do{ HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN); }while(0)        /* 翻转LED1 */

#if WIALL_HARDWARE_ENABLE
#define LED2_TOGGLE()   do{ HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN); }while(0)        /* 翻转LED1 */
#endif

/******************************************************************************************/

/* 外部接口函数*/
void led_init(void);                                                                            /* 初始化 */

 
void led3_event_callback(void *arg);
#endif
