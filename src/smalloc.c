#include "smalloc.h"

#ifdef SMALLOC_DEBUG
  #include <stdio.h>
#endif

struct _smalloc_block_t {
    size_t len;
    unsigned freed;
    struct _smalloc_block_t* next;
};

static struct _smalloc_info {
    int ready;
    size_t pagesize;
    struct _smalloc_block_t head;
} _info = {0};

/*
* Private function prototypes.
*/
int _smalloc_append(struct _smalloc_block_t* list, void* block);
int _smalloc_init(void);
int _smalloc_memcpy(void* dst, const void* src, size_t len);

void *smalloc(size_t size)
{
    int result;
    struct _smalloc_block_t header;
    struct _smalloc_block_t *ptr;
    void* mem = NULL;

    header.len = size + sizeof(struct _smalloc_block_t);
    header.freed = 0;
    header.next = NULL;

    mem = mmap(NULL, header.len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: call to mmap(2) failed!\n");
#endif
        return NULL;
    }

    ptr = (struct _smalloc_block_t*)mem;
    ptr->len = header.len;
    ptr->freed = header.freed;
    ptr->next = header.next;

    /* memcpy(mem, &header, sizeof(struct _smalloc_block_t)); */

    if (!_info.ready) {
#ifdef SMALLOC_DEBUG
        fprintf(stdout, "INFO: Initializing allocator.\n");
#endif
        _info.head.len = header.len;
        _info.head.freed = header.freed;
        _info.head.next = header.next;
        /* memcpy(&_info.head, &header, sizeof(struct _smalloc_block_t)); */
        _info.ready = 1;
    } else {
        result = _smalloc_append(_info.head.next, mem);
        if (result) {
#ifdef SMALLOC_DEBUG
            fprintf(stdout, "ERROR: Failed to append memory to list.\n");
#endif
            return NULL;
        }
    }

    return (mem + sizeof(struct _smalloc_block_t));
}

int _smalloc_append(struct _smalloc_block_t* list, void* block)
{
    struct _smalloc_block_t header;
    struct _smalloc_block_t *ptr;

    ptr = (struct _smalloc_block_t*)block;
    header.len = ptr->len;
    header.freed = ptr->freed;
    header.next = ptr->next;

    /* memcpy(&header, block, sizeof(struct _smalloc_block_t)); */
    if (header.next) {
#ifdef SMALLOC_DEBUG
        fprintf(stdout, "INFO: Traversing memory node...\n");
#endif
        return _smalloc_append(header.next, block);
    }

    header.next = block;
#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: Attaching memory to end of list.\n");
#endif

    return 0;
}

int _smalloc_init(void)
{
    /*
    * Since mmap(2) will align your allocations to pages, it's best to
    * know what your memory page size is to avoid excessive fragmentation.
    */
    _info.pagesize = sysconf(_SC_PAGESIZE);
    _info.ready = 1;

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: initialized the allocator.\n");
#endif
    return 0;
}

