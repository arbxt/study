#include "stack.h"

// 初始化栈
void init_stack(stack * s){
    s->qty = 0;
}

//栈清理
void deinit_stack(stack *s){
    s->qty = 0;
}

// 有效元素个数
int sizeof_stack(const stack *s){
    return s->qty;
}

//是否满栈
bool is_full_stack(const stack *s){
    return s->qty == SIZE;
}

// 是否空栈
bool is_empty_stack(const stack *s){
    return s->qty == 0;
}

// 入栈
bool push_stack(stack *s, int value){
    if(is_full_stack(s)) return false; // 栈满
    s->buf[s->qty++] = value; // 将值压入栈顶
    return true;
}

// 出栈
bool pop_stack(stack *s, int *value){
    if(is_empty_stack(s)) return false; // 栈空
    *value = s->buf[--s->qty]; // 将栈顶元素出栈
    return true;
}

// 获取栈顶元素
bool top_stack(const stack *s, int *value){
    if(is_empty_stack(s)) return false; // 栈空
    *value = s->buf[s->qty - 1]; // 获取栈顶
    return true;
}