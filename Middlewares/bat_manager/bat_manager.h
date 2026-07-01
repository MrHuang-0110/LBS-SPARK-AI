#ifndef __BAT_MANAGER_H
#define __BAT_MANAGER_H

#include "string.h"

#define VOLTAGE_CRITICAL_LOW  1.53f    // 临界低电量

#define VOLTAGE_LOW_MIN       1.58f    // 低电量最小值
#define VOLTAGE_LOW_MAX       1.68f    // 低电量最大值

#define VOLTAGE_MID_MIN       1.68f    // 中电量最小值
#define VOLTAGE_MID_MAX       1.78f    // 中电量最大值

#define VOLTAGE_HIGH_MIN      1.78f    // 高电量最小值
#define VOLTAGE_HIGH_MAX      1.88f    // 高电量最大值

// 定义电量状态枚举
typedef enum {
    BATTERY_CRITICAL = 0,  // 临界电量（关机）
    BATTERY_LOW,           // 低电量
    BATTERY_MID,           // 中电量
    BATTERY_HIGH,          // 高电量
    BATTERY_INVALID        // 无效电量
} BatteryLevel;

typedef struct {
    float voltage_buffer[10];  // 电压缓冲区
    int buffer_index;          // 缓冲区索引
    float filtered_voltage;    // 滤波后的电压
    BatteryLevel last_level;   // 上次电量状态
    unsigned int last_check_time;  // 上次检查时间
    int low_battery_count;     // 低电量计数（防抖）
} BatteryManager;


void battery_manager_init(void);
void update_battery_voltage(float new_voltage);
void check_battery_with_debounce(void);
int calculate_battery_percentage(float voltage, float min_v, float max_v);
float get_bat_filtered_volatge(void);
#endif