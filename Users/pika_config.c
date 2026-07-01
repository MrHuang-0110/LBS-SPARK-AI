#include "pika_config.h"
#include "malloc.h"
#include "./SYSTEM/sys/sys.h"
#include "stdarg.h"
#include "usbd_cdc_interface.h"
extern void usb_printf(char *fmt, ...);
void pika_platform_free(void* ptr) {
    myfree(SRAMIN,ptr);
}
void* pika_platform_realloc(void* ptr, size_t size) {

    return myrealloc(SRAMIN,ptr, size);
}
void* pika_platform_malloc(size_t size) {

    return mymalloc(SRAMIN,size);
}
void pika_platform_printf(char* fmt, ...)
{
    /* 脚本 print() 走带 PRIMASK 锁的统一通路，与 usb_printf、中断内监控发送串行，
     * 避免共享 g_usb_usart_printf_buffer / CDC TxState 被破坏。 */
    va_list ap;
    va_start(ap, fmt);
    vcdc_send_locked(fmt, ap);
    va_end(ap);
}
