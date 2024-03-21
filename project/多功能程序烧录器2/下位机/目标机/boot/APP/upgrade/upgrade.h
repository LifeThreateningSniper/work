#ifndef __UPGRADE_H
#define __UPGRADE_H
#include "system.h"

#define UPDATE_RX_BUFFER_LEN 1024

typedef struct UpdateData_S {

	u8 data[UPDATE_RX_BUFFER_LEN];
	u8 head;
	u8 tail;
	u16 size;
} UpdateData_t;

extern UpdateData_t g_UpdataRxBuff;

// ��ʼ������
void QueueInit(UpdateData_t *q);
// ���Ƿ�Ϊ��
bool QueueIsEmpty(UpdateData_t *q);
// �����Ƿ���
bool QueueisFull(UpdateData_t* q);
// ���
void QueueIn(UpdateData_t* q, u8 data);
// ����
int QueueOut(UpdateData_t* q, u8 *val);

#endif
