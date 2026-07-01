#include "adc.h"
#include "bat_manager.h"

/*1.6V -- 关机
  1.83V -- 红灯
	1.83 - 2.0 红绿
	2.0V  -- 绿灯*/
	
 
 
static uint16_t g_adc_dma_buf[ADC_DMA_BUF_SIZE];   
DMA_HandleTypeDef g_dma_nch_adc_handle = {0};                           
ADC_HandleTypeDef g_adc_nch_dma_handle = {0};                             
uint8_t g_adc_dma_sta = 0;                    

uint16_t g_adc_buf[SUM_ADC_CHANNL];
float    g_adc_voltage[SUM_ADC_CHANNL];
 
extern volatile float bat;
 
 
void adc_nch_dma_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};
    ADC_ChannelConfTypeDef adc_ch_conf = {0};

    ADC_ADCX_CHY_CLK_ENABLE();                                             
    __HAL_RCC_GPIOA_CLK_ENABLE();                                         

    if ((uint32_t)ADC_ADCX_DMACx > (uint32_t)DMA1_Channel7)               
    {
        __HAL_RCC_DMA2_CLK_ENABLE();                                        
    }
    else
    {
        __HAL_RCC_DMA1_CLK_ENABLE();                                         
    }

    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;                 
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;                       
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);                                 


    gpio_init_struct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;  
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;                                
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);


    g_dma_nch_adc_handle.Instance = ADC_ADCX_DMACx;                          
    g_dma_nch_adc_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;               
    g_dma_nch_adc_handle.Init.PeriphInc = DMA_PINC_DISABLE;                 
    g_dma_nch_adc_handle.Init.MemInc = DMA_MINC_ENABLE;                       
    g_dma_nch_adc_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  
    g_dma_nch_adc_handle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;   
    g_dma_nch_adc_handle.Init.Mode = DMA_NORMAL;                             
    g_dma_nch_adc_handle.Init.Priority = DMA_PRIORITY_MEDIUM;                
    HAL_DMA_Init(&g_dma_nch_adc_handle);

    __HAL_LINKDMA(&g_adc_nch_dma_handle, DMA_Handle, g_dma_nch_adc_handle);  

    g_adc_nch_dma_handle.Instance = ADC_ADCX;                                 
    g_adc_nch_dma_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;             
    g_adc_nch_dma_handle.Init.ScanConvMode = ADC_SCAN_ENABLE;                
    g_adc_nch_dma_handle.Init.ContinuousConvMode = ENABLE;                   
    g_adc_nch_dma_handle.Init.NbrOfConversion = SUM_ADC_CHANNL;             
    g_adc_nch_dma_handle.Init.DiscontinuousConvMode = DISABLE;              
    g_adc_nch_dma_handle.Init.NbrOfDiscConversion = 0;                        
    g_adc_nch_dma_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;          
    HAL_ADC_Init(&g_adc_nch_dma_handle);                                

    HAL_ADCEx_Calibration_Start(&g_adc_nch_dma_handle);                     

    adc_ch_conf.Channel = ADC_CHANNEL_0;                                     
    adc_ch_conf.Rank = ADC_REGULAR_RANK_1;                                   
    adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;                   
    HAL_ADC_ConfigChannel(&g_adc_nch_dma_handle, &adc_ch_conf);              
    
    adc_ch_conf.Channel = ADC_CHANNEL_4;                                    
    adc_ch_conf.Rank = ADC_REGULAR_RANK_2;                                  
    HAL_ADC_ConfigChannel(&g_adc_nch_dma_handle, &adc_ch_conf);              

    adc_ch_conf.Channel = ADC_CHANNEL_5;                                    
    adc_ch_conf.Rank = ADC_REGULAR_RANK_3;                                    
    HAL_ADC_ConfigChannel(&g_adc_nch_dma_handle, &adc_ch_conf);              

    adc_ch_conf.Channel = ADC_CHANNEL_6;                                    
    adc_ch_conf.Rank = ADC_REGULAR_RANK_4;                                   
    HAL_ADC_ConfigChannel(&g_adc_nch_dma_handle, &adc_ch_conf);              

    adc_ch_conf.Channel = ADC_CHANNEL_7;                                    
    adc_ch_conf.Rank = ADC_REGULAR_RANK_5;                                    
    HAL_ADC_ConfigChannel(&g_adc_nch_dma_handle, &adc_ch_conf);              

    HAL_NVIC_SetPriority(ADC_ADCX_DMACx_IRQn, 1,1);
    HAL_NVIC_EnableIRQ(ADC_ADCX_DMACx_IRQn);

 
	  HAL_DMA_Start_IT(&g_dma_nch_adc_handle, (uint32_t)&ADC1->DR, (uint32_t)&g_adc_dma_buf, 0);     /* 启动DMA，并开启中断 */
    HAL_ADC_Start_DMA(&g_adc_nch_dma_handle, (uint32_t*)&g_adc_dma_buf, 0);                        /* 开启ADC，通过DMA传输结果 */
		adc_dma_enable(ADC_DMA_BUF_SIZE);
}



void adc_dma_enable(uint16_t cndtr)
{
    ADC_ADCX->CR2 &= ~(1 << 0);               

    ADC_ADCX_DMACx->CCR &= ~(1 << 0);        
    while (ADC_ADCX_DMACx->CCR & (1 << 0));   
    ADC_ADCX_DMACx->CNDTR = cndtr;           
    ADC_ADCX_DMACx->CCR |= 1 << 0;          

    ADC_ADCX->CR2 |= 1 << 0;                    
    ADC_ADCX->CR2 |= 1 << 22;                 
}


void ADC_ADCX_DMACx_IRQHandler(void)
{
    if (ADC_ADCX_DMACx_IS_TC())
    {
        g_adc_dma_sta = 1;                     
        ADC_ADCX_DMACx_CLR_TC();               
    }
}
 
 
float getBatValute(void)
{
  return g_adc_voltage[0];
}

void sample_adc_data_callback(void *arg)
{
 
	uint16_t i,j;
	uint32_t sum;

   if(g_adc_dma_sta == 1)
	 { 
     for(j = 0; j < SUM_ADC_CHANNL; j++)  
    {
				sum = 0;
				for (i = 0; i < ADC_DMA_BUF_SIZE / SUM_ADC_CHANNL; i++) 
				{
					sum += g_adc_dma_buf[(SUM_ADC_CHANNL * i) + j];    
				}
				g_adc_buf[j] = sum / (ADC_DMA_BUF_SIZE / SUM_ADC_CHANNL);     

				g_adc_voltage[j] = (float)g_adc_buf[j] * (3.3f / 4096); 
    }		  
		  g_adc_dma_sta = 0;
			adc_dma_enable(ADC_DMA_BUF_SIZE);
      
		  update_battery_voltage(g_adc_voltage[0]);
	 }  
}
