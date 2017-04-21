#include <stdio.h>
#include <string.h>

#include "smalloc.h"

#define TEST_MEMORY_AMOUNT      (489)

int main(int argc, char* argv[])
{
    char* tmp1 = (char*)smalloc(TEST_MEMORY_AMOUNT);
    char* tmp2 = (char*)smalloc(TEST_MEMORY_AMOUNT);

    if (tmp1 == NULL || tmp2 == NULL) {
        fprintf(stdout, "Failed to allocated memory!\n");
        return -1;
    }

    /* fill the buffer...just for fun. */
    memset(tmp1, 0x05, TEST_MEMORY_AMOUNT);
    memset(tmp2, 0x07, TEST_MEMORY_AMOUNT);

    fprintf(stdout, "%p\n", tmp1);
    fprintf(stdout, "%p\n", tmp2);

    return 0;
}
