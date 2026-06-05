#ifndef _TREE_H_
#define _TREE_H_

#include <stdio.h>
#include <stdlib.h>

typedef struct treeNode {
    int value;
    struct treeNode *left;
    struct treeNode *right;
} treeNode;

void init_tree(treeNode** root);
void deinit_tree(treeNode** root);
static treeNode* search_tree(const treeNode* root, int value);
int insert_tree(treeNode** root, int value);
void print_tree(const treeNode* root);

#endif // _TREE_H_