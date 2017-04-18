#include <stdio.h>
#include <string.h>

#include "smalloc.h"

#define TEST_MEMORY_AMOUNT      (489)

int main(int argc, char* argv[])
{
    int i;
    void* tmp = NULL;
    char* cast = NULL;

    tmp = smalloc2(TEST_MEMORY_AMOUNT);
    memset(tmp, 0x5A, TEST_MEMORY_AMOUNT);

    cast = (char*)tmp;

    fprintf(stdout, "Memory Test 00:");
    for (i = 0; i < 489; i++) {
        if (i % 20 == 0) {
            fprintf(stdout, "\n");
        }
        fprintf(stdout, "%2x ", cast[i]);
    }
    fprintf(stdout, "\n");

    return 0;
}
