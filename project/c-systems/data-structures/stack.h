#ifndef _STACK_H
#define _STACK_H

#include <stdbool.h>

typedef struct{
    int buf[SIZE];
    int qty; // 栈中有效元素个数
}stack;

void init_stack(stack * s); // 初始化栈

void deinit_stack(stack *s); //栈清理

int sizeof_stack(const stack *s); // 有效元素个数

bool is_full_stack(const stack *s); //是否满栈

bool is_empty_stack(const stack *s); // 是否空栈

bool push_stack(stack *s, int value); // 入栈

bool pop_stack(stack *s, int *value); // 出栈

bool top_stack(const stack *s, int *value); // 获取栈顶元素

#endif