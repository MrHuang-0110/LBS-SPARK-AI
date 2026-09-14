#include "lbsfilemanager.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pikaObj.h"
#include "pikascript.h"
#include "pikaVm.h"
#include "pikaParser.h"
#include "PikaCompiler.h"
#include "pikaScript.h"
#include "beep.h"
#include "ui_manager.h"
#include "key.h"
#include "matrix_port.h"
#include "exfuns.h"
#include "malloc.h"
extern void usb_printf(char* fmt,...); 
volatile bool is_refresh_matrix = false;
void touchFileOKCallBack(void)
{ 
  beep_play_notice();
}
void touchFileErrorCallBack(void)
{ 
  beep_play_error();
}

int8_t addUIteam(char *UIiteam_name)
{ 
   if(strstr(UIiteam_name, ".o") == NULL) return -1;			 
		   char *name;
			 char *name_part = strtok(UIiteam_name, ".");
		   if(name_part == NULL) return -1;
		   bool is_digit = true;
		   for(uint8_t j = 0; name_part[j] != '\0'; j++)
		  { 
			  if(name_part[j] < '0' || name_part[j] > '9')
			 { is_digit = false;
				 break;
			 }		  
		  }
			if(!is_digit) return -1;
		  int num = atoi(name_part);
		  if(num < 0 || num > 9) return -1;
      
			return num;
}


void run_python(const char *name)
{ 
    extern void matrix_port_init(void);
	  extern void display_clear(void);
	  extern void pid_line_follow_reset(void);
	  extern void set_event_enable(char*name);
	  extern void set_event_disable(char*name);
	
    uint8_t *code = NULL;
    size_t fileSize = 0;

	  if(strcmp(name,"Pauto") == 0)
		{
			extern volatile bool start_pauto;
		  extern void pauto_play(void);
			start_pauto = true;
			set_event_enable("monitor_event");
			display_clear();
			pauto_play();
			set_event_disable("monitor_event");
		  extern void cloase_all_motor(void);
	    cloase_all_motor();
			return;
		}
		
		fileSize = get_file_length((char*)name);
		if(fileSize!=0)
		{ 
					 code = mymalloc(SRAMIN,fileSize);
					 if(code!=NULL)
					 { 				   
							memset(code,0,fileSize);
							 if(fatfs_read_file((char*)name,(uint8_t*)code,fileSize) == FR_OK)
							 {
									 set_event_enable("monitor_event");
								loader_remote_cfg();
								matrix_port_init();  
								display_clear();	 
								pid_line_follow_reset();
			
									 PikaObj* pikaMain = newRootObj("pikaMain", New_PikaMain);
									 pikaVM_runByteCodeInconstant(pikaMain, (uint8_t*)code);
									 set_event_disable("monitor_event");
									 obj_deinit(pikaMain);

								extern void cloase_all_motor(void);
								cloase_all_motor();
								matrix_port_set_brightness(7);
								myfree(SRAMIN,code);
								 
								return;
							}
					 }
			}	
 
		myfree(SRAMIN,code);
}
void exit_python(void){
  pks_vm_exit();
}

bool returnDownLoadState(void)
{ 
  return exfuns_file_transfer_active();
}

void refreshFwlibInfo(void)
{ 
  char fileStr[4];
	memset(fileStr,0,sizeof(fileStr));
 
	fatfs_read_file("version.txt",(char*)fileStr,4);
	extern volatile uint32_t spark_version;
	spark_version = atoi(fileStr);
}
