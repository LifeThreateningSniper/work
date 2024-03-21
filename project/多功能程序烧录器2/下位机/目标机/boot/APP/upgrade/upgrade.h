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

// 初始化队列
void QueueInit(UpdateData_t *q);
// 队是否为空
bool QueueIsEmpty(UpdateData_t *q);
// 队列是否满
bool QueueisFull(UpdateData_t* q);
// 入队
void QueueIn(UpdateData_t* q, u8 data);
// 出队
int QueueOut(UpdateData_t* q, u8 *val);

#endif
