#include "btim.h"
#include "event_manager.h"
#include "stdlib.h"
#include "exfuns.h"
volatile uint32_t cpuTick;
volatile uint32_t userCPUTick;
TIM_HandleTypeDef g_timx_handle;  /* 定时器句柄 */
 
 
// 静态变量
static volatile DateTime_t sys_time;  // 系统时间，volatile供中断使用
  DateTime_t start_time; 
  DateTime_t onff_time;   
static volatile uint32_t tick_counter = 0; // 秒计数（用于辅助计算）
static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// 判断闰年
static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 获取某月的天数
static uint8_t get_month_days(uint16_t year, uint8_t month) {
    if (month == 2 && is_leap_year(year))
        return 29;
    else
        return month_days[month - 1];
}

// 时间增加1秒（内部函数，在中断中调用）
static void time_add_one_second(void) {
    sys_time.second++;
    if (sys_time.second < 60) return;
    sys_time.second = 0;
    sys_time.minute++;
    if (sys_time.minute < 60) return;
    sys_time.minute = 0;
    sys_time.hour++;
    if (sys_time.hour < 24) return;
    sys_time.hour = 0;
    sys_time.day++;
    uint8_t max_day = get_month_days(sys_time.year, sys_time.month);
    if (sys_time.day <= max_day) return;
    sys_time.day = 1;
    sys_time.month++;
    if (sys_time.month <= 12) return;
    sys_time.month = 1;
    sys_time.year++;
}
 void Time_LoadFromFlash(void)
{ 
		if(fatfs_read_file("sys_time.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t))!=FR_OK)
		{ 
		   sys_time.year = 2026;
			 sys_time.month = 3;
			 sys_time.day = 30;
			 sys_time.hour = 15;
			 sys_time.minute = 0;
			 sys_time.second = 0;
		}		
}
 void Time_SaveToFlash(void)
{ 
   fatfs_create_file("sys_time.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t));
}
void Time_Init(void) {    
    Time_LoadFromFlash(); 
	  SavePowerOnStartTimer();
	
	 // GetPowerOnStartTimer(&start_time);
	  GetPoweDownTimer(&onff_time);
}

void SavePowerOnStartTimer(void)
{ 
   fatfs_create_file("power_start_Timer.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t));
}
void SavePowerDownTimer(void)
{ 
   fatfs_create_file("power_Down_Timer.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t));
}

void GetPowerOnStartTimer(DateTime_t *dt)
{ 
	  fatfs_read_file("power_start_Timer.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t));
    memcpy(dt, (void*)&sys_time, sizeof(DateTime_t));
}

void GetPoweDownTimer(DateTime_t *dt)
{ 
	  fatfs_read_file("power_Down_Timer.cfg",(DateTime_t*)&sys_time,sizeof(DateTime_t));
    memcpy(dt, (void*)&sys_time, sizeof(DateTime_t));
}

// 设置系统时间（注意需关中断保护）
void Time_Set(DateTime_t *dt) {
    __disable_irq();
    memcpy((void*)&sys_time, dt, sizeof(DateTime_t));
    __enable_irq();
}

// 获取系统时间（关中断复制）
void Time_Get(DateTime_t *dt) {
    __disable_irq();
    memcpy(dt, (void*)&sys_time, sizeof(DateTime_t));
    __enable_irq();
}
/**
 * @brief       基本定时器TIMX定时中断初始化函数
 * @note
 *              基本定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              基本定时器的时钟为APB1时钟的2倍, 而APB1为36M, 所以定时器时钟 = 72Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=定时器工作频率,单位:Mhz
 *
 * @param       arr: 自动重装值。
 * @param       psc: 时钟预分频数
 * @retval      无
 */
void btim_timx_int_init(uint16_t arr, uint16_t psc)
{
    g_timx_handle.Instance = BTIM_TIMX_INT;                      /* 通用定时器X */
    g_timx_handle.Init.Prescaler = psc;                          /* 设置预分频系数 */
    g_timx_handle.Init.CounterMode = TIM_COUNTERMODE_UP;         /* 递增计数模式 */
    g_timx_handle.Init.Period = arr;                             /* 自动装载值 */
    HAL_TIM_Base_Init(&g_timx_handle);

    HAL_TIM_Base_Start_IT(&g_timx_handle);    /* 使能定时器x及其更新中断 */
}

/**
 * @brief       定时器底层驱动，开启时钟，设置中断优先级
                此函数会被HAL_TIM_Base_Init()函数调用
 * @param       htim:定时器句柄
 * @retval      无
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIMX_INT)
    {
        BTIM_TIMX_INT_CLK_ENABLE();                     /* 使能TIM时钟 */
        HAL_NVIC_SetPriority(BTIM_TIMX_INT_IRQn, 2, 0); /* 抢占1，子优先级3，组2 */
        HAL_NVIC_EnableIRQ(BTIM_TIMX_INT_IRQn);         /* 开启ITM3中断 */
    }
}

/**
 * @brief       定时器TIMX中断服务函数
 * @param       无
 * @retval      无
 */
void BTIM_TIMX_INT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_timx_handle); /* 定时器中断公共处理函数 */
}

/**
 * @brief       定时器更新中断回调函数
 * @param       htim:定时器句柄
 * @retval      无
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIMX_INT)
    {
			  cpuTick++;	
			  userCPUTick++;
        extern void event_schedlucer(uint32_t cpu_tick);
			  event_schedlucer(cpuTick);
			  extern volatile uint32_t usb_idle_tick;
			  if(usb_idle_tick > 0)
				{
				   usb_idle_tick--;
					 if(usb_idle_tick == 0)
					 {
							set_event_enable("usb_receive");
					 }
				}
				if(cpuTick%1000 == 0)
				{ 
					time_add_one_second();
				tick_counter++;			  
				}
 
    }
}

uint32_t getTim6Tick(void)
{ 
   return cpuTick;
}

void resetUserCPUTick(void)
{ 
   userCPUTick = 0;
}
uint32_t getUserCPUTick(void)
{ 
  return userCPUTick;
}
/*reg config*/

static void RCC_Configuration(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN |RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN | RCC_APB1ENR_TIM2EN |RCC_APB1ENR_TIM5EN;
	
 
	  /*重映射TIM2*/
	  sys_gpio_remap_set(8,2,3);
	  /*重映射TIM3*/
	  sys_gpio_remap_set(10,2,3);
}

static void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc,uint8_t chy,TIM_TypeDef *GTIM_TIMX_PWM)
{
 
    GTIM_TIMX_PWM->ARR = arr;       /* 设定计数器自动重装值 */
    GTIM_TIMX_PWM->PSC = psc;       /* 设置预分频器  */
    GTIM_TIMX_PWM->BDTR |= 1 << 15; /* 使能MOE位(仅TIM1/8 有此寄存器,必须设置MOE才能输出PWM), 其他通用定时器, 这个
                                     * 寄存器是无效的, 所以设置/不设置并不影响结果, 为了兼容这里统一改成设置MOE位
                                     */

    if (chy <= 2)
    {
        GTIM_TIMX_PWM->CCMR1 |= 6 << (4 + 8 * (chy - 1));   /* CH1/2 PWM模式1 */
        GTIM_TIMX_PWM->CCMR1 |= 1 << (3 + 8 * (chy - 1));   /* CH1/2 预装载使能 */
    }
    else if (chy <= 4)
    {
        GTIM_TIMX_PWM->CCMR2 |= 6 << (4 + 8 * (chy - 3));   /* CH3/4 PWM模式1 */
        GTIM_TIMX_PWM->CCMR2 |= 1 << (3 + 8 * (chy - 3));   /* CH3/4 预装载使能 */
    }

    GTIM_TIMX_PWM->CCER |= 1 << (4 * (chy - 1));        /* OCy 输出使能 */
		
  //  GTIM_TIMX_PWM->CCER |= 1 << (1 + 4 * (chy - 1));    /* OCy 低电平有效 */
		GTIM_TIMX_PWM->CCER &=  ~(1 << (1 + 4 * (chy - 1))); /*高电平有效*/
		
    GTIM_TIMX_PWM->CR1 |= 1 << 7;   /* ARPE使能 */
    GTIM_TIMX_PWM->CR1 |= 1 << 0;   /* 使能定时器TIMX */
}

static void GPIO_Configuration(void)
{
      
	     
	
	    sys_gpio_set(GPIOA,
									 SYS_GPIO_PIN15,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
 
		  sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN3,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
	
	
 
	    sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN6,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
 
		  sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN7,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
									 
 
	    sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN8,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);

		  sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN9,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
									 
									 
	    sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN8,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
	    sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN9,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									
}
 
void pwm_init(void)
{ 
   RCC_Configuration();
	 GPIO_Configuration();
 
	 gtim_timx_pwm_chy_init(100-1,720-1,1,TIM2);
	 gtim_timx_pwm_chy_init(100-1,720-1,2,TIM2);
	 gtim_timx_pwm_chy_init(100-1,720-1,3,TIM4);
	 gtim_timx_pwm_chy_init(100-1,720-1,4,TIM4);	
	
	 gtim_timx_pwm_chy_init(100-1,720-1,1,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,2,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,3,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,4,TIM3);
}


 

void pwm_set_output(uint8_t id, int pwm)
{ 
    uint16_t pwm1 = 0, pwm2 = 0;
    
    if(pwm > PWM_MAX) pwm = PWM_MAX;
    if(pwm < -PWM_MAX) pwm = -PWM_MAX;
    
    // 高电平有效时的逻辑
    if(pwm > 0) {
        // 正向：通道1输出PWM，通道2输出低电平
        pwm1 = pwm;    // PWM占空比
        pwm2 = 0;      // 0%占空比 = 持续低电平
    } else if(pwm < 0) {
        // 反向：通道1输出低电平，通道2输出PWM
        pwm1 = 0;      // 0%占空比 = 持续低电平
        pwm2 = -pwm;   // PWM占空比
    } else {
        // pwm=0：滑行模式（后续由滑行函数处理）
        pwm1 = 0;
        pwm2 = 0;
    }
    
    // 根据ID设置对应定时器
    switch(id) {
        case 4:
            //TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM2->CCR1 = pwm1;
            TIM2->CCR2 = pwm2;
            break;
        case 5:
            //TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = pwm1;
            TIM3->CCR2 = pwm2;
            break;
        case 6:
            //TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = pwm1;
            TIM3->CCR4 = pwm2;
            break;
        case 7:
            //TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = pwm1;
            TIM4->CCR4 = pwm2;
            break;
    }
}

 
void pwm_stop_slide(uint8_t id)
{
    switch(id) {
        case 4:
           // TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
						TIM2->CCR1 = 0;
            TIM2->CCR2 = 0;				
            break;
        case 5:
						//TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = 0;
            TIM3->CCR2 = 0;
            break;
        case 6:
						//TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = 0;
            TIM3->CCR4 = 0;
            break;
        case 7:
						//TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = 0;
            TIM4->CCR4 = 0;
            break;
    }
}
 
void pwm_stop_breaking(uint8_t id)
{ 
    switch(id) {
        case 4:
           // TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM2->CCR1 = PWM_MAX;  // 100%占空比 = 持续高电平
            TIM2->CCR2 = PWM_MAX;  // 100%占空比 = 持续高电平
				    
            break;
        case 5:
            //TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = PWM_MAX;
            TIM3->CCR2 = PWM_MAX;
            break;
        case 6:
            //TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = PWM_MAX;
            TIM3->CCR4 = PWM_MAX;
            break;
        case 7:
            //TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = PWM_MAX;
            TIM4->CCR4 = PWM_MAX;
            break;
    }  
}
	
