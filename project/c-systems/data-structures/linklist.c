#include "linklist.h"

Node* init_list(){
    Node* head = (Node*)malloc(sizeof(Node));
    if (!head)
    {
        printf("init fail: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    head->next = NULL;
    return head;
}

void destroy_list(Node* head){
    Node* current = head;
    Node* next_node;
    while(current != NULL){
        next_node = current -> next;
        free(current);
        current = next_node;
    }
}

// Print the linked list
void print_list(Node* head){
    Node* current = head->next; // Skip the dummy head node
    while(current != NULL){
        printf("%d -> ", current->data);
        current = current->next;
    }
    printf("NULL\n");
}

//size of the list
int sizeof_list(Node* head){
    int count = 0;
    Node* current = head->next; // Skip the dummy head node
    while(current != NULL){
        count++;
        current = current->next;
    }
    return count;
}

// Insert a new node with the given data at the head of the list
void push_front(Node* head, int data){
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node)
    {
        printf("push_front fail: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    new_node->data = data;
    new_node->next = head->next;
    head->next = new_node;
}

// Insert a new node with the given data at the tail of the list
void push_back(Node* head, int data){
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node)
    {
        printf("push_back fail: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    new_node->data = data;
    new_node->next = NULL;
    Node* current = head;
    while(current->next != NULL){
        current = current->next;
    }
    current->next = new_node;
}

//Insert a new node with the given data at the specified node
void insert(Node* head, int pos, int data){
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node)
    {
        printf("insert fail: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    if(pos < 0){
        printf("insert fail: Invalid position\n");
        free(new_node);
        return;
    }
    new_node->data = data;
    Node* current = head;
    for(int i = 0; i < pos && current; i++){
        current = current->next;
    }
    if (!current) {
        printf("insert fail: Position out of bounds\n");
        free(new_node);
        return;
    }
    new_node->next = current->next;
    current->next = new_node;
}

//remove node at the head of the list
void pop_front(Node* head){
    if (head->next == NULL) {
        printf("List is empty, nothing to pop_front\n");
        return;
    }
    Node* temp = head->next;
    head->next = temp->next;
    free(temp);
}

//remove node at the tail of the list
void pop_back(Node* head){
    if (head->next == NULL) {
        printf("List is empty, nothing to pop_back\n");
        return;
    }
    Node* current = head;
    while(current->next != NULL && current->next->next != NULL){
        current = current->next;
    }
    Node* temp = current->next;
    current->next = NULL;
    free(temp);
}

//remove node at the specified position
void remove_at(Node* head, int pos){
    if (head->next == NULL) {
        printf("List is empty, nothing to remove_at\n");
        return;
    }
    if (pos < 0) {
        printf("remove_at fail: Invalid position\n");
        return;
    }
    Node* current = head;
    for(int i = 0; i < pos && current->next != NULL; i++){
        current = current->next;
    }
    if (current->next == NULL) {
        printf("remove_at fail: Position out of bounds\n");
        return;
    }
    Node* temp = current->next;
    current->next = temp->next;
    free(temp);
}

//remove node with the specified value,0 means first occurrence,1 means all occurrences
void remove_value(Node* head, int value, int choice){
    if (head->next == NULL) {
        printf("List is empty, nothing to remove_value\n");
        return;
    }
    Node* current = head;
    while(current->next != NULL){
        if(current->next->data == value){
            Node* temp = current->next;
            current->next = temp->next;
            free(temp);
            if(choice == 0) // Remove only the first occurrence
                return;
        } else {
            current = current->next;
        }
    }
    if(choice == 0 && current->next == NULL)
        printf("Value %d not found in the list\n", value);
}

//find the first node with the specified value
Node* find_value(Node* head, int value){
    Node* current = head->next; // Skip the dummy head node
    while(current != NULL){
        if(current->data == value){
            return current;
        }
        current = current->next;
    }
    return NULL; // Not found
}

//get the value of the node at the specified position
int get_value_at(Node* head, int pos){
    if (head->next == NULL) {
        printf("List is empty,nothing to get\n");
        return -1; // Indicate that the list is empty
    }
    if (pos < 0) {
        printf("get_value_at fail: Invalid position\n");
        return -1; // Indicate invalid position
    }
    Node* current = head->next; // Skip the dummy head node
    for(int i = 0; i < pos && current != NULL; i++){
        current = current->next;
    }
    if (current == NULL) {
        printf("get_value_at fail: Position out of bounds\n");
        return -1; // Indicate position out of bounds
    }
    return current->data;
}

//update the value of the node at the specified position
void update_value(Node* head, int pos, int new_value){
    if (head->next == NULL) {
        printf("List is empty,nothing to update\n");
        return;
    }
    if (pos < 0) {
        printf("update_value fail: Invalid position\n");
        return;
    }
    Node* current = head->next; // Skip the dummy head node
    for(int i = 0; i < pos && current != NULL; i++){
        current = current->next;
    }
    if (current == NULL) {
        printf("update_value fail: Position out of bounds\n");
        return;
    }
    current->data = new_value;
}

//reverse the linked list
void reverse_list(Node* head){
    if (head->next == NULL || head->next->next == NULL) {
        return; // List is empty or has only one element
    }
    Node* prev = NULL;
    Node* current = head->next;
    Node* next = NULL;
    while(current != NULL){
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    head->next = prev; // Update head to point to the new first element
}

