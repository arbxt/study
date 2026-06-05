#include<stdio.h>

char *mystrcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while((*dest++ = *src++) != '\0');
    return original_dest;
}

int main() {
    char source[] = "Hello, World!";
    char destination[50];
    mystrcpy(destination, source);
    printf("Copied string: %s\n", destination);
    return 0;
}