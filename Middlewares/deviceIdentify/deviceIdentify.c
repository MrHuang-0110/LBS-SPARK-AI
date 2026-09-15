#include "deviceidentify.h"
#include "malloc.h"
#include "motor.h"
#include "blue.h"
#include "ultrasion.h"
#include "ir_remote.h"
#include "touch.h"
#include "color.h"
#include "json-maker.h"
#include "lbsfilemanager.h"
 

_DEVICE_HUB hub_port[9];

void destroy_device(SensorBase *sensor)
{
    if(sensor == NULL) return;
    
    switch(sensor->type)
    {
        case DEVICE_MOTOR_ID:
        {
            DEV_MOTOR *motor = (DEV_MOTOR *)sensor;
            myfree(SRAMIN,motor);
            break;
        }
        case DEVICE_ULTRASION_ID:
        {
            DEV_ULTRASION *ultrasion = (DEV_ULTRASION *)sensor;
            myfree(SRAMIN,ultrasion);
            break;
        }
        case SENSOR_TYPE_IR_REMOTE:
        {
            DEV_IR_REMOTE *ir_remote = (DEV_IR_REMOTE *)sensor;
            myfree(SRAMIN,ir_remote);
            break;
        }
        case DEVICE_COLOR_ID:
        {
            DEV_COLOR *color = (DEV_COLOR *)sensor;
            myfree(SRAMIN,color);
            break;
        }
        case DEVICE_TOUCH_ID:
        {
            DEV_TOUCH *touch = (DEV_TOUCH *)sensor;
            myfree(SRAMIN,touch);
            break;
        }
        case DEVICE_BLUE_ID:
        {
            DEV_BLUE *blue = (DEV_BLUE *)sensor;
            myfree(SRAMIN,blue);
            break;
        }
        default:
            break;
    }
}

/* 按内部类型创建设备实例并绑定到端口。
   id 必须是 int：IR_REMOTE 的内部私有类型 0x1A3 不能被截断成 0xA3（超声波）。 */
void identify_and_bind(_DEVICE_HUB *manager, int id) 
{
    if(manager == NULL) return;

    /* 先销毁旧设备 */
    if(manager->sensors != NULL)
    {
        destroy_device(manager->sensors);
        manager->sensors = NULL;
    }
    
    switch(id)
    {
        case DEVICE_MOTOR_ID:
        {
            manager->sensors = (SensorBase*)create_motor();
            if(manager->sensors != NULL)
                manager->sensors->setParam = NULL;

            manager->LinkeDeviceID = DEVICE_MOTOR_ID;
            break;
        }
        case DEVICE_ULTRASION_ID:
        {
            manager->sensors = (SensorBase*)create_ultrasion();
            if(manager->sensors != NULL)
                manager->sensors->setParam = refsh_ultrasion;

            manager->LinkeDeviceID = DEVICE_ULTRASION_ID;
            break;
        }
        case SENSOR_TYPE_IR_REMOTE:
        {
            manager->sensors = (SensorBase*)create_ir_remote();
            if(manager->sensors != NULL)
                manager->sensors->setParam = refsh_ir_remote;

            /* 线端 ObjectID 与超声波相同：对外仍统一上报 0xA3 */
            manager->LinkeDeviceID = DEVICE_IR_REMOTE_ID;
            break;
        }
        case DEVICE_COLOR_ID:
        {
            manager->sensors = (SensorBase*)create_color();
            if(manager->sensors != NULL)
            {
                /* 读取颜色配置文件 */
                read_color_cfg(manager->sensors,manager->hub_id);
                manager->sensors->setParam = refsh_color;
            }

            manager->LinkeDeviceID = DEVICE_COLOR_ID;
            break;
        }
        case DEVICE_TOUCH_ID:
        {
            manager->sensors = (SensorBase*)create_touch();
            if(manager->sensors != NULL)
                manager->sensors->setParam = refsh_touch;

            manager->LinkeDeviceID = DEVICE_TOUCH_ID;
            break;
        }
        case DEVICE_BLUE_ID:
        {
            manager->sensors = (SensorBase*)create_blue();
            if(manager->sensors != NULL)
                manager->sensors->setParam = refsh_blue;

            manager->LinkeDeviceID = DEVICE_BLUE_ID;
            break;
        }
        default:
            break;
    }
}

void set_sensor_parameter(SensorBase* sensor,void *param)
{ 
    if (sensor == NULL) {     
				 
        return;
    }  
    if (sensor->setParam != NULL) {	 
        sensor->setParam(sensor, param);			   
    }
}

uint8_t GetHubLinkeDeviceId(uint8_t id)
{
   return hub_port[id].LinkeDeviceID;
}

SensorBase *getHubBase(uint8_t id)
{ 
    _DEVICE_HUB *g_device_manager;

    if(id >= 9U) {
        return NULL;   /* 防止 Python/越界端口号越界访问 hub_port[] */
    }

    g_device_manager = &hub_port[id];
    
    if(g_device_manager == NULL) {
        return NULL;
    }
    
    if(g_device_manager->sensors == NULL) {
        return NULL;
    }

    return g_device_manager->sensors;
}

/* 取端口设备实例并校验内部子类型：0xA3 复用线上 IR_REMOTE 与超声波
   必须靠内部类型区分，类型不匹配时返回 NULL，避免错误强转。 */
SensorBase *getHubBaseByType(uint8_t id,int type)
{ 
    SensorBase *base = getHubBase(id);

    if(base == NULL || base->type != type) {
        return NULL;
    }

    return base;
}

/* 端口内部子类型查询（仅供 C 内部使用，不暴露给 Python / 上位机） */
int get_port_sensor_type(uint8_t id)
{ 
    if(id >= 9U || hub_port[id].sensors == NULL) {
        return 0;
    }

    return hub_port[id].sensors->type;
}

SensorBase *HubBase_And_identify(uint8_t id,int sourceId)
{ 
    _DEVICE_HUB *g_device_manager = &hub_port[id];
    if(g_device_manager == NULL) {
        return NULL;
    }

    if(g_device_manager->sensors == NULL) {
        g_device_manager->hub_id = id;
        identify_and_bind(g_device_manager,sourceId);		
    }
			 
    if(g_device_manager->sensors == NULL)return NULL;
		
    g_device_manager->portTimeOutTick = 250;/*500ms 没有刷新，自动解绑*/
    return  g_device_manager->sensors;
}

/* 超声波上报载荷：ASCII "<cm>/<dt>"（例如 "45/12"）。
   严格校验：数字 + '/' + 数字，且必须恰好消耗整个载荷。 */
static bool ultrasion_payload_match(const _AGREEMENT *frame)
{
    uint16_t i = 0;
    uint16_t digits = 0;

    if(frame == NULL || frame->index != SENSOR_UPLOAD_INDEX) return false;
    if(frame->length < 3U || frame->length > 32U) return false;

    while(i < frame->length && frame->data[i] >= '0' && frame->data[i] <= '9') i++;
    if(i == 0 || i >= frame->length || frame->data[i] != '/') return false;
    i++;

    while(i < frame->length && frame->data[i] >= '0' && frame->data[i] <= '9')
    {
        i++;
        digits++;
    }

    return (digits > 0 && i == frame->length);
}

/* 用帧内容把线端 ID 细分为内部类型。
   返回 0 表示"弱特征"：无法确定子类型。 */
static int resolve_internal_type(const _AGREEMENT *frame)
{
    if(frame == NULL) return 0;

    switch(frame->sID)
    {
        case DEVICE_MOTOR_ID:
        case DEVICE_COLOR_ID:
        case DEVICE_TOUCH_ID:
        case DEVICE_BLUE_ID:
            return frame->sID;

        case DEVICE_ULTRASION_ID:
            /* 0xA3 复用：IR_REMOTE 与超声波 ObjectID 相同，按强特征帧区分 */
            if(ir_remote_frame_is_ir(frame)) return SENSOR_TYPE_IR_REMOTE;
            if(ultrasion_payload_match(frame)) return DEVICE_ULTRASION_ID;
            break;

        default:
            break;
    }

    return 0;
}

/* 端口收到有效协议帧后的统一处理：
   1) 未绑定：按内部类型识别并绑定（0xA3 弱特征帧沿用历史行为按超声波兜底）；
   2) 已绑定但收到另一种设备的强特征帧：销毁旧实例重新绑定（同端口热替换）；
   3) 类型一致：刷新超时并把数据分发给设备实例。 */
void HubBase_Frame_Process(uint8_t port,_AGREEMENT *frame)
{
    _DEVICE_HUB *manager;
    int internal_type;

    if(port > 3U || frame == NULL) return;

    manager = &hub_port[port];
    internal_type = resolve_internal_type(frame);

    if(internal_type == 0)
    {
        /* 弱特征帧不触发子类型切换 */
        if(frame->sID != DEVICE_ULTRASION_ID) return;

        if(manager->sensors != NULL)
        {
            /* 已绑定：保持原绑定，仅刷新超时并分发 */
            manager->portTimeOutTick = 250;
            set_sensor_parameter(manager->sensors,frame);
            return;
        }

        /* 未绑定：兼容历史，按超声波兜底（后续收到 IR 强特征帧会自动改绑） */
        internal_type = DEVICE_ULTRASION_ID;
    }

    if(manager->sensors == NULL)
    {
        manager->hub_id = port;
        identify_and_bind(manager,internal_type);
    }
    else if(manager->sensors->type != internal_type)
    {
        /* 同端口热替换：收到另一种设备的强特征帧，立即重新识别绑定，
           不必等错误解析或超时解绑。 */
        sys_intx_disable();
        destroy_device(manager->sensors);
        manager->sensors = NULL;
        sys_intx_enable();
        identify_and_bind(manager,internal_type);
    }

    if(manager->sensors == NULL) return;

    manager->portTimeOutTick = 250;/*500ms 没有刷新，自动解绑*/
    set_sensor_parameter(manager->sensors,frame);
}

void HubBase_Scan_TimeOut(void)
{ 
   for(uint8_t i = 0;i<4;i++)
	 { 
	    if(hub_port[i].portTimeOutTick > 0)
			{
			  hub_port[i].portTimeOutTick-=10;
				if(hub_port[i].portTimeOutTick == 0)
				{
					hub_port[i].LinkeDeviceID = 0;
					sys_intx_disable();
				  destroy_device(hub_port[i].sensors);
					hub_port[i].sensors = NULL;
					sys_intx_enable();
				}
			}
	 }
} 

void init_identify_dev(void)
{ 
	 identify_and_bind(&hub_port[PORT_MOTOR_A],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_B],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_C],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_D],DEVICE_MOTOR_ID);   
}
