#ifndef __MAIN_H
#define __MAIN_H

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

#include "malloc.h"


#include "btim.h"
#include "led.h"
#include "key.h"
#include "iic.h"
#include "spi.h"
#include "adc.h"
#include "wdg.h"
#include "event_manager.h"
#include "blue.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"

#include "protocol.h"
 
#include "lbsfilemanager.h"
#include "event_manager.h"
#include "json-maker.h"
#include "deviceidentify.h"
#include "display.h"
#include "motor.h"
#include "beep.h"
#include "ui_manager.h"
#include "animation.h"
#define APP_START_ADDR      0x08008000

#endif
