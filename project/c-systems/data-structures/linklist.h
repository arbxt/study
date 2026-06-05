#ifndef _LINKLIST_H_
#define _LINKLIST_H_

#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

Node* init_list();
void destroy_list(Node* head);
void print_list(Node* head);
void push_front(Node* head, int data);
void push_back(Node* head, int data);
void insert(Node* head, int pos, int data);
int sizeof_list(Node* head);
void pop_front(Node* head);
void pop_back(Node* head);
void remove_at(Node* head, int pos);
void remove_value(Node* head, int value, int choice);
Node* find_value(Node* head, int value);
int get_value_at(Node* head, int pos);
void update_value(Node* head, int pos, int new_value);
void reverse_list(Node* head);

#endif