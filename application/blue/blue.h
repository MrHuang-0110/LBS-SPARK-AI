#ifndef __BLUE_H
#define __BLUE_H
#include "stdbool.h"
#include "deviceidentify.h"

#define PORT_BLUE    0x08
#define DEVICE_BLUE_ID      0xAF
#define BLUE_REMOTE_DATA_SIZE 10U

typedef struct{
  int blue_init_state,on_off;
}BLUE_CFG;

typedef struct{ 
	SensorBase base;
	UART_HandleTypeDef *huart;
	volatile bool is_resh_flag;
	bool is_off_on;
	char    at_cmd_bufer[32];
	BLUE_CFG cfg;
}DEV_BLUE;

DEV_BLUE *read_blue(void *self);
DEV_BLUE *create_blue(void);
void refsh_blue(void* self, void* data);
void blue_init(void);
void blue_set_on(void);
void blue_set_off(void);
void blue_logo_blinke(void);
void blue_send_data(void *data,uint16_t len);
void blue_update_remote(const uint8_t *data,uint16_t length);
void blue_read_remote(uint8_t *data);
bool blue_remote_active(void);
void blue_remote_session_start(void);
void blue_remote_session_end(void);
#endif
