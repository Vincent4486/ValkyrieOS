#include <std/stdio.h>

void __attribute__((section(".entry"))) start()
{
    printf("hello\n");
}