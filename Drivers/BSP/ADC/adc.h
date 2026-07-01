#ifndef __ADC_H
#define __ADC_H

#include "./SYSTEM/sys/sys.h"

#define ADC_ADCX                            ADC1 
#define ADC_ADCX_DMACx                      DMA1_Channel1
#define ADC_ADCX_DMACx_IRQn                 DMA1_Channel1_IRQn
#define ADC_ADCX_DMACx_IRQHandler           DMA1_Channel1_IRQHandler

#define ADC_ADCX_DMACx_IS_TC()              ( DMA1->ISR & (1 << 1) )    /* 判断 DMA1_Channel1 传输完成标志, 这是一个假函数形式,
                                                                         * 不能当函数使用, 只能用在if等语句里面 
                                                                         */
#define ADC_ADCX_DMACx_CLR_TC()             do{ DMA1->IFCR |= 1 << 1; }while(0) /* 清除 DMA1_Channel1 传输完成标志 */
#define ADC_ADCX_CHY_CLK_ENABLE()           do{ __HAL_RCC_ADC1_CLK_ENABLE(); }while(0)   /* ADC1 时钟使能 */

#define ADC_DMA_BUF_SIZE 50 * 5
#define SUM_ADC_CHANNL   5

extern uint16_t g_adc_buf[SUM_ADC_CHANNL];
extern float    g_adc_voltage[SUM_ADC_CHANNL];
void adc_nch_dma_init(void);
void adc_dma_enable(uint16_t cndtr);
void sample_adc_data_callback(void *arg);
float getBatValute(void);
#endif
