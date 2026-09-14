#include "protocol.h"
#include "malloc.h"

 
static uint8_t calculate_checksum(const uint8_t *data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum&0xFF;
}

uint8_t dataAgreeAnalys(_AGREEMENT *_agreement_,uint8_t *data,uint16_t length)
{ 
  if(_agreement_ == NULL || data == NULL || length < MIN_FRAME_SIZE)
    return AGREE_MEN_ERROR;

  if(data[0] != FRAME_HEADER || data[length - 1] != FRAME_FOOTER)
    return AGREE_MEN_ERROR;

  /* length 字段兼容两种历史约定：
   * - payload 长度：整帧 = data[3] + 7（本仓库上位机/传感器节点）
   * - 整帧长度：  整帧 = data[3]（部分旧发送端，如遥控 App）
   * CRC 仍会校验，不会因此放松数据正确性。 */
  if((uint16_t)data[3] + 7U != length && (uint16_t)data[3] != length)
    return AGREE_MEN_ERROR;

  uint8_t mycrc;
  mycrc = calculate_checksum((const uint8_t *)data,length - 2);

  if(mycrc!=data[length - 2])
  {
    return AGREE_MEN_ERROR;
  }
  else
  {
    memset(_agreement_->data,0,256);
    _agreement_->Head = data[0];
    _agreement_->sID = data[1];
    _agreement_->oID = data[2];
    _agreement_->length = data[3];
    _agreement_->index = data[4];

    memcpy(_agreement_->data,data+5,data[3]);

    _agreement_->crc = data[length - 2];
    _agreement_->tard = data[length - 1];
    return AGREE_MEN_OK;
  }
}


void frame_parser_init(FrameParser *parser) {
	  
    parser->state = STATE_IDLE;
    parser->index = 0;
    parser->expected_length = 0;
    parser->calc_checksum = 0;
    parser->frame_valid = false;
	
    if(parser->buffer != NULL)
		{
		   myfree(SRAMIN,parser->buffer);
		}
		parser->buffer = mymalloc(SRAMIN,MAX_FRAME_SIZE);
		memset(parser->buffer,0,MAX_FRAME_SIZE);
}

bool frame_parser_process_byte(FrameParser *parser, uint8_t byte) {
    switch (parser->state) {
        case STATE_IDLE:
            if (byte == FRAME_HEADER) {
                parser->state = STATE_HEADER;
                parser->index = 0;
                parser->calc_checksum = 0;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;  // ��ʼ��У���
            }
            break;
            
        case STATE_HEADER:
            if (byte == SRC_ID) {
                parser->state = STATE_SRC_ID;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_SRC_ID:
            if (byte == DEST_ID) {
                parser->state = STATE_DEST_ID;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_DEST_ID:
            parser->state = STATE_LENGTH;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->expected_length = byte;  // �����ֶ�
            break;
            
        case STATE_LENGTH:
            parser->state = STATE_TYPE;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->frame_type = byte;  // �����ֶ�
            
            // ����Ƿ��������ֶ�
            if (parser->expected_length == 0) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_TYPE:
            // ���״̬Ӧ��ֻ��expected_length>0ʱ�Ż����
            parser->state = STATE_DATA;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->data_bytes_received = 1;  // �����ѽ��յ������ֽ���
            
            // ����Ƿ��Ѿ���������������
            if (parser->data_bytes_received >= parser->expected_length) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_DATA:
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->data_bytes_received++;
            
            // ����Ƿ��������������
            if (parser->data_bytes_received >= parser->expected_length) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_CHECKSUM:
            parser->buffer[parser->index++] = byte;
            
            // ��֤У���
            if (byte == (parser->calc_checksum & 0xFF)) {
                parser->state = STATE_FOOTER;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_FOOTER:
            parser->buffer[parser->index++] = byte;
            if (byte == FRAME_FOOTER) {
                // ����֡���ճɹ�
                parser->frame_valid = true;
                return true;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        default:
            frame_parser_init(parser);
            break;
    }
    
    // ��黺�������
    if (parser->index >= MAX_FRAME_SIZE) {
        frame_parser_init(parser);
    }
    
    return false;
}
void MultiUart_SendFrame(void (*transerf_data)(void*,uint16_t),
															 uint8_t *data,
															 uint16_t len,
															 uint8_t index)
{
    if(transerf_data == NULL)return;
	
    _AGREEMENT frame;
    
    memset(frame.data,0,sizeof(frame.data));
	
    frame.length = len+7;
	
	  frame.data[0] = 0x5A;
	  frame.data[1] = 0x97;
	  frame.data[2] = 0x98;
	  frame.data[3] = len;
	  frame.data[4] = index;
	
	  for(uint16_t i = 0;i<len;i++)
	 { 
	   frame.data[5+i] = data[i];
	 }
	 
	 uint32_t checksum = 0;
	 for (size_t i = 0; i < frame.length - 2; i++) {
		checksum += frame.data[i];
   }	
	 
	 frame.data[len + 5] = checksum&0xFF;
	 frame.data[len + 6] = 0xA5;	
	
   transerf_data((void *)frame.data,frame.length);
}
