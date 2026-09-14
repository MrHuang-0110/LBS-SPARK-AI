#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#include "./SYSTEM/sys/sys.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"


typedef struct
{
  uint8_t  Head;
  uint8_t  sID;
  uint8_t  oID;
  uint16_t length;
  uint8_t  index;
  uint8_t  data[256];
  uint8_t  crc;
  uint8_t  tard;
}_AGREEMENT;

enum
{
  AGREE_MEN_ERROR,
  AGREE_MEN_OK,
};

#define FRAME_HEADER    0x5A
#define SRC_ID          0x97
#define DEST_ID         0x98
#define FRAME_FOOTER    0xA5
#define MAX_FRAME_SIZE  300  // ���֡����
#define MIN_FRAME_SIZE  7    // 最小帧长：头+源+目标+长度+索引+校验+尾

typedef enum {
    STATE_IDLE,         // ����״̬
    STATE_HEADER,       // ����֡ͷ
    STATE_SRC_ID,       // ����ԴID
    STATE_DEST_ID,      // ����Ŀ��ID
    STATE_LENGTH,       // ���ճ���
    STATE_TYPE,         // ��������
    STATE_DATA,         // ��������
    STATE_CHECKSUM,     // ����У���
    STATE_FOOTER        // ����֡β
} ParserState;

typedef struct {
    ParserState state;              // ��ǰ����״̬
    uint8_t *buffer; // ֡������
    uint16_t index;                // ��ǰд��λ��
    uint16_t expected_length;     // Ԥ�����ݳ���
	  uint16_t data_bytes_received;
    uint8_t calc_checksum;       // �����У���
    uint8_t frame_type;          // ֡����
    bool frame_valid;            // ֡��Ч��־
} FrameParser;



void frame_parser_init(FrameParser *parser);
uint8_t dataAgreeAnalys(_AGREEMENT *_agreement_,uint8_t *data,uint16_t length);
bool frame_parser_process_byte(FrameParser *parser, uint8_t byte);
void MultiUart_SendFrame(void (*transerf_data)(void*,uint16_t),uint8_t *data,uint16_t len,uint8_t index);

/* OTA/Python�ļ����ؾ�� */
typedef struct{ 
  _AGREEMENT frame;
	void (*port_transerf_data)(void *data,uint16_t length);
	bool is_refresh_data;
}OTA_PY_FILE;
#endif
