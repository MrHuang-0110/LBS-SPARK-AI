#include "adc.h"
#include "led.h"
#include "display.h"

#include "bat_manager.h"
 
#include "./SYSTEM/delay/delay.h"
 
extern void usb_printf(char* fmt,...); 
static BatteryManager battery_mgr = {0};

static BatteryLevel get_battery_level(float voltage)
{
    if (voltage < VOLTAGE_CRITICAL_LOW) {
        return BATTERY_CRITICAL;
    } else if (voltage >= VOLTAGE_LOW_MIN && voltage < VOLTAGE_LOW_MAX) {
        return BATTERY_LOW;
    } else if (voltage >= VOLTAGE_MID_MIN && voltage < VOLTAGE_MID_MAX) {
        return BATTERY_MID;
    } else if (voltage >= VOLTAGE_HIGH_MIN && voltage <= VOLTAGE_HIGH_MAX) {
        return BATTERY_HIGH;
    }
    return BATTERY_INVALID;
}
static void set_battery_leds(BatteryLevel level)
{
	 extern void SavePowerDownTimer(void);
    switch (level) {
        case BATTERY_LOW:      // 低电量：红灯亮，绿灯灭
            LED1(1);  // 绿灯灭
            LED0(0);  // 红灯亮
            break;
            
        case BATTERY_MID:      // 中电量：红灯亮，绿灯亮（黄色）
            LED1(0);  // 绿灯亮
            LED0(0);  // 红灯亮
            break;
            
        case BATTERY_HIGH:     // 高电量：红灯灭，绿灯亮
            LED1(0);  // 绿灯亮
            LED0(1);  // 红灯灭
            break;
            
        case BATTERY_CRITICAL: // 临界电量：处理关机
             display_play_power_off_animation();      
             delay_ms(1000);       
             HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);      
             while(1);  // 停机
            break;
            
        case BATTERY_INVALID: break; // 无效电量：闪烁警示
			  default:break;
    }
}
int calculate_battery_percentage(float voltage, float min_v, float max_v)
{
    if (voltage <= min_v) return 0;
    if (voltage >= max_v) return 100;
    
    // 线性计算百分比
    int percentage = (int)((voltage - min_v) / (max_v - min_v) * 100.0f);
    return percentage;
}


// 初始化电池管理器
void battery_manager_init(void)
{
    memset(&battery_mgr, 0, sizeof(battery_mgr));
    battery_mgr.last_level = BATTERY_INVALID;
}

// 更新电压滤波
void update_battery_voltage(float new_voltage)
{
    // 更新缓冲区
    battery_mgr.voltage_buffer[battery_mgr.buffer_index] = new_voltage;
    battery_mgr.buffer_index = (battery_mgr.buffer_index + 1) % 10;
    
    // 计算平均值
    float sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += battery_mgr.voltage_buffer[i];
    }
    battery_mgr.filtered_voltage = sum / 10.0f;
}
float get_bat_filtered_volatge(void)
{ 
  return battery_mgr.filtered_voltage;
}
// 检查电量状态（带防抖）
void check_battery_with_debounce(void)
{
    static uint32_t last_update_time = 0;
    uint32_t current_time = HAL_GetTick();
  
    // 每秒检查一次
    if (current_time - last_update_time < 1000) {
        return;
    }
    last_update_time = current_time;
    
    // 更新滤波电压
   // update_battery_voltage(g_adc_voltage[0]);
    
    // 获取当前电量状态
    BatteryLevel new_level = get_battery_level(battery_mgr.filtered_voltage);
     
    // 临界电量处理（需要连续多次检测才触发）
    if (new_level == BATTERY_CRITICAL) {
        battery_mgr.low_battery_count++;
        if (battery_mgr.low_battery_count >= 3) {  // 连续3次检测到才触发
            set_battery_leds(BATTERY_CRITICAL);
        }
    } else {
        battery_mgr.low_battery_count = 0;  // 重置计数器
        
        // 状态改变时更新LED
        if (new_level != battery_mgr.last_level) {
            set_battery_leds(new_level);
            battery_mgr.last_level = new_level;
        }
    }
 
}