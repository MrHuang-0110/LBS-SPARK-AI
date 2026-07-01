
#include "./BSP/LED/led.h"

#include "key.h"
#include "adc.h"
#include "stdio.h"
#include "string.h"
#include "blue.h"
/**
 * @brief       初始化LED相关IO口, 并使能时钟
 * @param       无
 * @retval      无
 */
void led_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
	  
	  #if WIALL_HARDWARE_ENABLE
    LED0_GPIO_CLK_ENABLE();                                 /* LED0时钟使能 */
    LED1_GPIO_CLK_ENABLE();                                 /* LED1时钟使能 */
    LED2_GPIO_CLK_ENABLE();                                 /* LED2时钟使能 */
	
    gpio_init_struct.Pin = LED0_GPIO_PIN;                   /* LED0引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* 高速 */
    HAL_GPIO_Init(LED0_GPIO_PORT, &gpio_init_struct);       /* 初始化LED0引脚 */

    gpio_init_struct.Pin = LED1_GPIO_PIN;                   /* LED1引脚 */
    HAL_GPIO_Init(LED1_GPIO_PORT, &gpio_init_struct);       /* 初始化LED1引脚 */

    gpio_init_struct.Pin = LED2_GPIO_PIN;                   /* LED1引脚 */
    HAL_GPIO_Init(LED2_GPIO_PORT, &gpio_init_struct);       /* 初始化LED1引脚 */

	  gpio_init_struct.Pin = GPIO_PIN_5;                   /* LED1引脚 */
    HAL_GPIO_Init(GPIOC, &gpio_init_struct);       /* 初始化LED1引脚 */
		
    LED0(1);                                                /* 关闭 LED0 */
    LED1(1);                                                /* 关闭 LED1 */	
		LED2(1);                                                /* 关闭 LED1 */	
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
	  #else
    LED0_GPIO_CLK_ENABLE();                                 /* LED0时钟使能 */
    LED1_GPIO_CLK_ENABLE();                                 /* LED1时钟使能 */

    gpio_init_struct.Pin = LED0_GPIO_PIN;                   /* LED0引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
    
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* 高速 */
    HAL_GPIO_Init(LED0_GPIO_PORT, &gpio_init_struct);       /* 初始化LED0引脚 */

    gpio_init_struct.Pin = LED1_GPIO_PIN;                   /* LED1引脚 */
    HAL_GPIO_Init(LED1_GPIO_PORT, &gpio_init_struct);       /* 初始化LED1引脚 */
	
    LED0(1);                                                /* 关闭 LED0 */
    LED1(1);                                                /* 关闭 LED1 */
	  #endif
}

 
#if WIALL_HARDWARE_ENABLE
void led3_event_callback(void *arg)
{ 
	extern void usb_printf(char *fmt, ...);
 DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(PORT_BLUE));
 
			if(blue->is_off_on)
			{ 
			  if(BLUE_STA)
					LED2(0);
				else{ 
					LED2_TOGGLE();
				}
					 
			}	 
			else{ 
			  LED2(1);
			}
}
#endif
