#ifndef DEVICEIDENTIFY_H
#define DEVICEIDENTIFY_H

#include "./SYSTEM/sys/sys.h"
#include "stdbool.h"
#include "protocol.h"

/* 传感器上报帧 typeIndex：超声波与 IR_REMOTE 线端 ID 复用 0xA3，上报都用 0xED */
#define SENSOR_UPLOAD_INDEX   0xED

typedef struct {
   int  type;
   char name[16];
   void (*setParam)(void* self, void *data);
}SensorBase;


typedef struct{
	SensorBase *sensors;
	uint8_t hub_id;
  uint8_t LinkeDeviceID;
	uint16_t portTimeOutTick;
}_DEVICE_HUB;

SensorBase *getHubBase(uint8_t id);
extern _DEVICE_HUB hub_port[9];

/* 取端口设备实例并校验内部子类型：类型不匹配时返回 NULL。
   IR_REMOTE 与超声波线端 ID 同为 0xA3，只能靠内部私有类型区分。 */
SensorBase *getHubBaseByType(uint8_t id,int type);
/* 端口内部子类型（0=未绑定），仅供 C 内部使用 */
int get_port_sensor_type(uint8_t id);

void destroy_device(SensorBase *sensor);
void identify_and_bind(_DEVICE_HUB *manager, int id);
void set_sensor_parameter(SensorBase* sensor,void *param);
SensorBase *HubBase_And_identify(uint8_t id,int sourceId);
/* 端口收到有效协议帧后的统一入口：识别/热替换/刷新超时/分发数据 */
void HubBase_Frame_Process(uint8_t port,_AGREEMENT *frame);
uint8_t GetHubLinkeDeviceId(uint8_t id);
void HubBase_Scan_TimeOut(void);
void init_identify_dev(void);
#endif
