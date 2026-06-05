#include "queue.h"

void init_queue(queue *q){
    q->head = 0;
    q->tail = 0;
    q->qty = 0;
}

void deinit_queue(queue *q){
    q->head = 0;
    q->tail = 0;
    q->qty = 0;
}

int sizeof_queue(const queue *q){
    return q->qty;
}

bool is_full_queue(const queue *q){
    return sizeof_queue(q) == SIZE;
}

bool is_empty_queue(const queue *q){
    return sizeof_queue(q) == 0;
}
bool enqueue(queue *q, int value){
    if(is_full_queue(q)) return false;
    q->buf[q->tail++ % SIZE] = value;
    q->qty++;
    return true;
}

bool dequeue(queue *q, int *value){
    if(is_empty_queue(q)) return false;
    *value = q->buf[q->head++];
    if( q->head == SIZE ) q->head = 0; // 环形队列
    q->qty--;
    return true;
}

bool front_queue(const queue *q, int *value){
    if(is_empty_queue(q)) return false;
    *value = q->buf[q->head];
    return true;
}
