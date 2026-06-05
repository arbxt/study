//
// Created by root on 3/17/16.
//

#include "manage.h"
/*
 * error is preserving for future.
 * */
__thread int error;
void* wsx_malloc(size_t sizes) {
    void * alloc = NULL;
    if(UNLIKELY(sizes < 0)) {
        error = MALLOC_ERR_SIZE;
        return NULL;
    }
    alloc = wsx_calloc(sizes);
    if(UNLIKELY(NULL == alloc)){
        error = MALLOC_ERR_NO_MEM;
        return alloc;
    }
    return alloc;
}

void* wsx_calloc(size_t sizes) {
    void * alloc = NULL;
    if(UNLIKELY(sizes < 0)) {
        error = CALLOC_ERR_SIZE;
        return alloc;
    }
    alloc = calloc(1, sizes);
    if(UNLIKELY(NULL == alloc))
        error = CALLOC_ERR_NO_MEM;
    return alloc;
}

MM_STATUS wsx_free(void * pointer) {
    if(UNLIKELY(NULL == pointer))
        return FREE_ERR_EMPTY;
    free(pointer);
    return FREE_SUCCEED;
}