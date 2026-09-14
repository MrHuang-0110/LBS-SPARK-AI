#ifndef __LBSFILEMANAGER_H
#define __LBSFILEMANAGER_H
 
#include "protocol.h"
void run_python(const char *name);
 bool returnDownLoadState(void);
void refreshFwlibInfo(void);
void exit_python(void);
void touchFileOKCallBack(void);
void touchFileErrorCallBack(void);
int8_t addUIteam(char *UIiteam_name);
#endif
