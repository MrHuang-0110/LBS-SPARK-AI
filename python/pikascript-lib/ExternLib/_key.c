#include "key.h"
#include "blue.h"
#include "_key.h"

typedef struct{
    const char* name;
    uint8_t down_state;
    uint8_t up_state;
    uint8_t current_state;
}KeyHandler;

enum{
   /*方向值*/
   KeyUp = 0,
   KeyDown,
   KeyLeft,
   KeyRight,
   /*功能值*/
   Y,
   A,
   X,
   B,
   /*L1,R2*/
   R1Key,
   L1Key,
};

int _key_key_mast(PikaObj *self, char* keyname, pika_float State)
{ 
 
   static KeyHandler keys[2] = {
   {"left", 0, 1, 0},
   {"right", 0, 1, 0},
   };    
	 
	 keys[0].current_state = KEY1;
	 keys[1].current_state = WK_UP;
	 
	 
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        if (strcmp(keyname, keys[i].name) == 0) {
            if (keys[i].current_state == keys[i].down_state) {
                return (State != 0.0f) ? 1.0f : 0.0f;
            }
            if (keys[i].current_state == keys[i].up_state) {
                return (State == 0.0f) ? 1.0f : 0.0f;
            }
            break;
        }
    } 
    return 0.0f;		 
}

int _key_key_remote(PikaObj *self, char* keys, char* coor)
{ 
    uint8_t keyValue = 0;  
   
    if (keys == NULL || coor == NULL) {
        return 0;
    }

		DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(8));
		if(blue == NULL)
				return 0;
		
		uint8_t *remote  = blue->remoteValue;
		
		#if 0
        if (strcmp(coor, "x") == 0 || strcmp(coor, "y") == 0) {
        if (strcmp(keys, "left") != 0 && strcmp(keys, "right") != 0) {
            return 0;
        }
  
        uint8_t rockey = 0;
        int stick_offset = (strcmp(keys, "left") == 0) ? 1 : 2;

        int axis_offset = (strcmp(coor, "x") == 0) ? 1 : 2;
				
				if(stick_offset == 1)
				{		   
					rockey = (axis_offset == 1)?L_Rocker_X:L_Rocker_Y;
				}
				else
				{		   
					rockey = (axis_offset == 1)?R_Rocker_X:R_Rocker_Y;
				}
 
        return remote[rockey];
    }
    #endif
		
    static const char* RemoteKeyValue[] = {
       "up","down","left","right","Y","A","B","X","L1","R1"
    };

    static int keyIndexMap[256] = {0}; // 使用ASCII值作为索引
    static bool mapInitialized = false;
    
    if (!mapInitialized) {
        for (int i = 0; i < 10; i++) {
            if (RemoteKeyValue[i] != NULL) {
                // 使用键名的第一个字符创建简单哈希
                keyIndexMap[(uint8_t)RemoteKeyValue[i][0]] = i + 1;
            }
        }
        mapInitialized = true;
    }

    int keyIndex = -1;
    if (keys[0] != '\0') {
        int mapIndex = keyIndexMap[(uint8_t)keys[0]];
        if (mapIndex > 0 && strcmp(RemoteKeyValue[mapIndex - 1], keys) == 0) {
            keyIndex = mapIndex - 1;
        }
    }

    if (keyIndex == -1) {
        for (int i = 0; i < 10; i++) {
            if (strcmp(RemoteKeyValue[i], keys) == 0) {
                keyIndex = i;
                break;
            }
        }
    }
    
 
    if (keyIndex != -1) {
        pika_GIL_EXIT();
        keyValue = remote[keyIndex];
        pika_GIL_ENTER();
    }

    if (strcmp(coor, "press") == 0) {
        return (keyValue == 1) ? 1 : 0;
    }
    else if (strcmp(coor, "unpress") == 0) {
        return (keyValue == 0) ? 1 : 0;
    }
    
    return 0;
}

int _key_read_adcance_left_offset(PikaObj *self)
{ 
   return read_advance_offset1();
}
int _key_read_advance_right_offset(PikaObj *self)
{ 
   return read_advance_offset2();
}
int _key_read_retreat_left_offset(PikaObj *self)
{ 
   return read_retreat_offset1();
}
int _key_read_retreat_right_offset(PikaObj *self)
{ 
   return read_retreat_offset2();
}
