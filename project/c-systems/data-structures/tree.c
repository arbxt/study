#include "tree.h"

void init_tree(treeNode** root) {
    *root = NULL;
}

void deinit_tree(treeNode** root){
    if(*root != NULL){
        deinit_tree(&((*root)->left));
        deinit_tree(&((*root)->right));
        free(*root);
        *root = NULL; 
    }
}

treeNode* search_tree(const treeNode* root, int value){
    if(root == NULL || root->value == value){
        return (treeNode*)root;
    }
    if(value < root->value){
        return search_tree(root->left, value);
    } else {
        return search_tree(root->right, value);
    }
}

int insert_tree(treeNode** root, int value){
    if(*root == NULL){
        treeNode* newNode = (treeNode*)malloc(sizeof(treeNode));
        if(newNode == NULL){
            return -1; // Memory allocation failed
        }
        newNode->value = value;
        newNode->left = NULL;
        newNode->right = NULL;
        *root = newNode;
        return 0; // Success
    }
    if(value < (*root)->value){
        return insert_tree(&((*root)->left), value);
    } else if(value > (*root)->value){
        return insert_tree(&((*root)->right), value);
    } else {
        return -2; // Value already exists
    }
}

void print_tree(const treeNode* root){
    if(root != NULL){
        print_tree(root->left);
        printf("%d ", root->value);
        print_tree(root->right);
    }
}