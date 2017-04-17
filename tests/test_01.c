#include <stdio.h>
#include <unistd.h>

#include "smalloc.h"

#define SMALLOC_COUNT       (30)
#define SMALLOC_CHUNKS      (1024)

int main(int argc, char* argv[])
{
    int i;
    void* ptrs[SMALLOC_COUNT];

    fprintf(stdout, "%ld\n", sysconf(_SC_PAGESIZE));

    for (i = 0; i < SMALLOC_COUNT; i++) {
        ptrs[i] = smalloc(SMALLOC_CHUNKS * (i + 1));
        if (ptrs[i]) {
            fprintf(stdout, "Allocated %d bytes of memory!\n",
                SMALLOC_CHUNKS * (i + 1));
        } else {
            fprintf(stderr, "TEST FAILED TO ALLOCATE MEMORY!\n");
        }
    }

    return 0;
}
