//
// Created by root on 4/3/16.
//

#ifndef HTTPD3_WSX_STRING_H_H
#define HTTPD3_WSX_STRING_H_H
/// README FIRST!
/// While Using the string Data Structure, we should use the string_t instead of String_s
/// Because We should initialize it before using, so keep it away if it has
/// not make by make_Strings
/// It means that using the string_t carefully
/// To be Safety,
/// call the Interface make_Strings, and use the return value
/// Destroy if it has nothing to live by using dele_Strings
/// Both the two functions is manipulate the string_t or return it.
/// Update at 20160819
/// Now Support Binary message

#include <stdint.h>
#include <assert.h>
#include <memory.h>
typedef char boolen;
/*
 * ReDesign String Structure
 * For Binary Saft
 * */
struct string_s{
    char *   str;
    uint32_t length;
    uint32_t cap;
};
#if defined(__GNUC__) && (__GNUC__ >= 3) || defined(__clang__) && (__clang_major__ >= 3)
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define UNLIKELY(x) (x)
#endif
typedef struct string_s String_s;
typedef String_s * string_t;

#define MAKE_STRING_S(c_str) make_string(c_str, strlen(c_str))
#define STRING(c_str) c_str, (uint32_t)strlen(c_str)
#define CHAR_OF(string_s, index) (index)>=(string_s)->length?assert(("string index out of range",0)),1:((string_s)->str)[index]

/*
 * Create String
 * receive buffer and length of the buffer content
 * There is no limit about the function, it could be binary-message in buffer
 * */
String_s * make_string(const char * content, uint32_t length);
/*
 * Copy String
 * */
String_s * copy_string(const String_s * src);

/*
 * Get Length
 * Work For Any situation
 * */
uint32_t get_length(const String_s * itself);
/*
 * Get Capacity
 * return the value of the whole capacity that apply from heap
 * */
uint32_t get_capacity(const String_s * itself);
/*
 * Get status
 * check for empty
 * */
boolen is_empty(const String_s * itself);
/*
 * Search For content
 * Use For string searching EXCLUDE binary-message!!!
 * */
char * search_content(String_s * itself, const char * search, uint32_t length);
/*
 * get inner buffer
 * get the inner buffer pointer in unchanged way
 * */
const char * get_inner_buf(const String_s * itself);

void free_string(String_s * itself);
/*
 * free String
 * */
#define Free_string(op) {free_string(op); op = NULL;}

/*
 * clear string
 * Set String content empty
 * */
void clear_string(String_s * itself);
/*
 * append content
 * */
uint32_t append_string(String_s * itself, const char * content, uint32_t length);
/*
 * Compare String and char buffer
 * */
boolen compare_string_char(String_s * itself, const char * content, uint32_t length);
/*
 * Compare String and String
 * */
boolen compare_string_string(String_s * first, String_s * second);
/*
 * fetch and generate sub-String
 * */
String_s * make_substring(const String_s * itself, uint32_t start, uint32_t length);
/*
 * fetch inner buffer
 * */
char * make_inner_buf(const String_s * itself);

/*
 * compare the string with char buffer from tail to top
 * */
int32_t rcmp_content(const String_s * cmp1, const char * cmp2, uint32_t n);

#if defined(WSX_DEBUG)
void print_string(string_t print);
#endif
#endif //HTTPD3_WSX_STRING_H_H
