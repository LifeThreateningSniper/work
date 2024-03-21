#include "upgrade.h"

UpdateData_t g_UpdataRxBuff;

// 初始化队列
void QueueInit(UpdateData_t *q)
{
	memset(q->data, 0, UPDATE_RX_BUFFER_LEN);
	q->head = 0;
	q->tail = 0;
	q->size = UPDATE_RX_BUFFER_LEN;
}

// 队是否为空
bool QueueIsEmpty(UpdateData_t *q)
{
	return (bool)(q->head == q->tail);
}

// 队列是否满

bool QueueisFull(UpdateData_t* q) 
{
    return (q->tail + 1) % q->size == q->head;
}

// 入队
void QueueIn(UpdateData_t* q, u8 data) 
{
    if (QueueisFull(q)) {
        printf("Error: Queue is full!\r\n");
        return;
    }
    q->data[q->tail] = data;
    q->tail = (q->tail + 1) % q->size;
}

// 出队
int QueueOut(UpdateData_t* q, u8 *val) 
{
    if (QueueIsEmpty(q)) {
        return -1;
    }
    *val = q->data[q->head];
    q->head = (q->head + 1) % q->size;
    return 0;
}

