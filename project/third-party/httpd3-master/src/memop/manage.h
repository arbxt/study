//
// Created by root on 3/17/16.
//

#ifndef HTTPD3_MANAGE_H
#define HTTPD3_MANAGE_H
#include <malloc.h>
#include <stdlib.h>
#if defined(__GNUC__) && (__GNUC__ >= 3) || defined(__clang__) && (__clang_major__ >= 3)
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define UNLIKELY(x) (x)
#endif
enum mm_error {
    ALLOC_SUCCEED     = 0x00, /* Successful */
    MALLOC_ERR_SIZE   = 0x01, /* Invalid Size For Allocate */
    CALLOC_ERR_SIZE   = 0x02,
    MALLOC_ERR_NO_MEM = 0x08, /* No Enough memory For Allocation */
    CALLOC_ERR_NO_MEM = 0x10,
    FREE_ERR_EMPTY    = 0x04, /* Free Pointer is NULL */
    FREE_SUCCEED      = ALLOC_SUCCEED,
};

#define WSX_MM_DEPENT
typedef enum mm_error MM_STATUS;
#endif

#if defined(WSX_MM_DEPENT)
void* wsx_malloc(size_t sizes);
void* wsx_calloc(size_t sizes);
MM_STATUS wsx_free(void * pointer);
#endif

#if !defined(WSX_MM_DEPENT)
#define Malloc malloc
#define Calloc calloc
#define Free   free
#elif defined(WSX_MM_JEMALLOC) /* extension for jemalloc or tcmalloc */
    /* TODO JEMALLOC */
#else
#define Malloc wsx_malloc
#define Calloc wsx_calloc
#define Free   wsx_free

#endif //HTTPD3_MANAGE_H
