#include <stdio.h>
#include "tree.h"

int main() {
    treeNode* root;
    init_tree(&root);
    printf("Tree initialized successfully.\n");
    print_tree(root);
    printf("Insert 1: %d\n", insert_tree(&root,1));
    printf("Insert 2: %d\n", insert_tree(&root,2));
    printf("Insert 3: %d\n", insert_tree(&root,3));
    printf("Insert 4: %d\n", insert_tree(&root,4));
    printf("Insert 5: %d\n", insert_tree(&root,5));
    printf("Insert 0: %d\n", insert_tree(&root,0));
    printf("Insert 5 again (should fail): %d\n", insert_tree(&root,5));
    printf("Tree after insertions:\n");
    print_tree(root);
    deinit_tree(&root);
    printf("\nTree deinitialized successfully.\n");
    print_tree(root);
    return 0;
}