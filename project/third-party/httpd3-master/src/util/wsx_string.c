//
// Created by root on 4/3/16.
//

#include "wsx_string.h"
#include <stdlib.h>
#include <assert.h>

#define WSX_STR_CAP_DEFAULT 32
static boolen expand_size(String_s * self) {
    uint32_t capacity = self->cap;
    if (UNLIKELY(UINT32_MAX == capacity))
        assert(0);
    uint32_t comfortable = WSX_STR_CAP_DEFAULT * 2 - 1; /* 0x1F */
    while ( comfortable <= capacity && comfortable != UINT32_MAX) {
        comfortable <<= 1;
        comfortable |= 1;
    }
    capacity = comfortable;
    char * tmp = calloc(1, capacity);
    if (UNLIKELY(NULL == tmp)) assert(0);
    memcpy(tmp, self->str, self->length);
    free(self->str);
    self->str = tmp;
    self->cap = capacity;
    return 1;
}
String_s * make_string(const char * content, uint32_t length)
{
    if (UNLIKELY(NULL == content)) {
        content = "";
        length  = 0;
    }
    String_s * local = calloc(1, sizeof(String_s));
    if (UNLIKELY(NULL == local)) assert(0);
    local->cap     = WSX_STR_CAP_DEFAULT > length ? WSX_STR_CAP_DEFAULT : (length + 1);
    local->length  = 0;
    local->str     = NULL;
    expand_size(local);
    memcpy(local->str, content, length);
    (local->str)[length] = '\0';
    local->length = length;
    return local;
}

uint32_t get_length(const String_s * itself)
{
    if (UNLIKELY(NULL == itself))
        assert(0);
    return itself->length;
}

uint32_t get_capacity(const String_s * itself)
{
    if (UNLIKELY(NULL == itself))
        assert(0);
    return itself->cap;
}

boolen is_empty(const String_s * itself) {
    return itself->length == 0;
}

void free_string(String_s * itself) {
    free(itself->str);
    free(itself);
}

void clear_string(String_s * itself) {
    (itself->str)[0] ='\0';
    itself->length = 0;
}

char * make_inner_buf(const String_s * itself) {
    uint32_t length = itself->length;
    char * local = calloc(1, length);
    if (UNLIKELY(NULL == local))
        return NULL;
    memcpy(local, itself->str, length);
    return local;
}

const char * get_inner_buf(const String_s * itself) {
    return itself->str;
}

boolen compare_string_string(String_s * first, String_s * second) {
    uint32_t first_len  = first->length;
    uint32_t second_len = second->length;
    return !(first_len ^ second_len) && !memcmp(first->str, second->str, first_len);
}

boolen compare_string_char(String_s * itself, const char * content, uint32_t length) {
    if ( UNLIKELY(length > itself->length) ) {
        return 0;
    }
    return !(boolen)memcmp(itself->str, content, length);
}

String_s * copy_string(const String_s * src) {
    return make_string(src->str, src->length);
}

String_s * make_substring(const String_s * itself, uint32_t start, uint32_t length) {
    const char * local_start = itself->str + start;
    if (length > itself->length - start)
        length = itself->length - start;
    String_s * local = make_string(local_start, length);
    return local;
}

uint32_t append_string(String_s * itself, const char * content, uint32_t length) {
    while (itself->cap - itself->length <= length)
        expand_size(itself);
    char * local = itself->str + itself->length;
    memcpy(local, content, length);
    itself->length += length;
    (itself->str)[itself->length] = '\0';
    return length;
}

char * search_content(String_s * itself, const char * search, uint32_t length) {
    if (UNLIKELY(strlen(search) != length)) {
        return NULL;
    }
    return strstr(itself->str, search);
}

static int wsx_rstrncmp(const char * str1, const char * str2, int32_t nbytes) {
    if (nbytes < 0)
        return -1;
    if (NULL == str1 || NULL == str2)
        return -2;
    size_t str1_byte = strlen(str1);
    size_t str2_byte = strlen(str2);
    if (str1_byte < nbytes || str2_byte < nbytes)
        return -3;
    for (int i = 0; i < nbytes; ++i) {
        if (str1[--str1_byte] != str2[--str2_byte])
            return 1;
    }
    return 0;
}

int32_t rcmp_content(const String_s * cmp1, const char * cmp2, uint32_t n) {
    if (UNLIKELY(0 == cmp1->length) || UNLIKELY(0 == n))
        return 1;
    return wsx_rstrncmp(cmp1->str, cmp2, n);
}

#if defined(WSX_DEBUG)
void print_string(string_t print) {
    fprintf(stderr, "capacity:%d, length:%d -> %s\n",print->cap, print->length, print->str);
}
#endif