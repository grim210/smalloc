#include <stdio.h>

#include "smalloc.h"

#define SMALLOC_COUNT       (30)
#define REQUEST_SIZE        (200)

int main(int argc, char* argv[])
{
    int i;
    void* ptrs[SMALLOC_COUNT];
    size_t len, accumulator;

    accumulator = 0;
    for (i = 0; i < SMALLOC_COUNT; i++) {
        len = (REQUEST_SIZE * (i + 1));
        accumulator += len;
        ptrs[i] = smalloc(len);

        if (ptrs[i]) {
            fprintf(stdout, "Allocated %lu bytes of memory! Total: %lu "
                "[%d]\n", len, accumulator, i);
        } else {
            fprintf(stderr, "TEST FAILED TO ALLOCATE MEMORY!\n");
            return -1;
        }
    }

    return 0;
}
