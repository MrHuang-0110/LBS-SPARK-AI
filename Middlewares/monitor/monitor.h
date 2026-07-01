// monitor.h
#ifndef __MONITOR_H
#define __MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// �����������ö��
typedef enum {
    MONITOR_TYPE_INT,
    MONITOR_TYPE_STRING,
    MONITOR_TYPE_FLOAT,
    MONITOR_TYPE_BOOL,
    MONITOR_TYPE_JSON_OBJECT,  // Ƕ��JSON����
    MONITOR_TYPE_JSON_ARRAY    // JSON����
} MonitorDataType;

// �����ص���������
typedef char* (*MonitorCallback)(void* context, size_t* remLen);

// ������
typedef struct {
    const char* key;           // JSON����
    MonitorDataType type;      // ��������
    MonitorCallback callback;  // ��ȡ���ݵĻص�����
    void* context;            // �ص�����������
    const char* description;   // �����������ã�
} MonitorItem;

// �豸��Ϣ�ṹ
typedef struct {
    uint8_t port;
    void* sensor;
    uint32_t device_id;
} DeviceInfo;

// ��ع�����
typedef struct {
    MonitorItem* items;        // ���������
    uint16_t item_count;       // ���������
    uint16_t max_items;        // ���������
    
    DeviceInfo* devices;       // �豸��Ϣ����
    uint8_t device_count;      // �豸����
    
    char* json_buffer;         // JSON������
    size_t buffer_size;        // ��������С
} MonitorManager;

// ��ʼ����ع�����
MonitorManager* monitor_init(char* buffer, size_t size);

// ע������
bool monitor_register_item(MonitorManager* manager, 
                          const char* key, 
                          MonitorDataType type,
                          MonitorCallback callback,
                          void* context,
                          const char* desc);

// ע���豸
bool monitor_register_device(MonitorManager* manager,
                            uint8_t port,
                            void* sensor,
                            uint32_t device_id);

// ���ɼ�ر���
bool monitor_generate_report(MonitorManager* manager);

// �������
void monitor_output_report(MonitorManager* manager);
														
void monitor_call_back(void*arg);
char* monitor_get_json(void);

/* 中断驱动的监控发送(由 btim 事件派发)：
 * 仅在脚本运行(start_py/start_pauto，主循环被阻塞)时发送，
 * 空闲期仍由主循环发送，二者互斥避免重复。 */
void monitor_send_usb(void*arg);    /* 1ms：USB CDC */
void monitor_send_blue(void*arg);   /* 25ms：蓝牙 UART5，非阻塞 */
#endif // __MONITOR_H