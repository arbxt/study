#ifndef _QUEUE_H
#define _QUEUE_H

#define SIZE 100
#include <stdbool.h>

typedef struct queue{
    int buf[SIZE];
    int head;
    int tail;
    int qty; // 队列中有效元素个数
}queue;

void init_queue(queue *q);
void deinit_queue(queue *q);
int sizeof_queue(const queue *q);
bool is_full_queue(const queue *q);
bool is_empty_queue(const queue *q);
bool enqueue(queue *q, int value);
bool dequeue(queue *q, int *value);
bool front_queue(const queue *q, int *value);
#endif