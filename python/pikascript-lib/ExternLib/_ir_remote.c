#include "ir_remote.h"
#include "_ir_remote.h"

/* _ir_remote.set_color(port,state)：0=灭 1=红 2=绿 3=蓝。
   端口不是已识别的 IR_REMOTE 或参数非法时静默忽略（无返回值）。 */
void _ir_remote_set_color(PikaObj *self, int port, int state)
{
    (void)self;
    ir_remote_set_color(port,state);
}
