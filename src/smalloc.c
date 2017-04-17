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
#ifdef _WIN32
    HANDLE heap_ptr;
#endif
} _info = {0};

/*
* Private function prototypes.
*/
int _smalloc_append(struct _smalloc_block_t* list, void* block);
int _smalloc_clean(struct _smalloc_block_t* list);
int _smalloc_free(struct _smalloc_block_t* list, void* block);
int _smalloc_init(void);
void* _smalloc_osalloc(size_t size);

void *smalloc(size_t size)
{
    int result;
    struct _smalloc_block_t header;
    struct _smalloc_block_t *ptr;
    void* mem = NULL;

    if (!_info.ready) {
        result = _smalloc_init();
        if (result) {
#ifdef SMALLOC_DEBUG
            fprintf(stderr, "ERROR: failed to initialize smalloc\n");
#endif
            return NULL;

        }
    }

    header.len = size + sizeof(struct _smalloc_block_t);
    header.freed = 0;
    header.next = NULL;

    result = header.len % _info.pagesize;
    if (result) {
        header.len += _info.pagesize - result;
    }

    mem = _smalloc_osalloc(header.len);
    if (mem == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: call to _osalloc failed!\n");
#endif
        return NULL;
    }

    ptr = (struct _smalloc_block_t*)mem;
    ptr->len = header.len;
    ptr->freed = header.freed;
    ptr->next = header.next;

    result = _smalloc_append(&_info.head, mem);
    if (result) {
#ifdef SMALLOC_DEBUG
        fprintf(stdout, "ERROR: Failed to append memory to list.\n");
#endif
        return NULL;
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
    * Since we're going to attempt to be as effecient as possible,
    * we have to ensure that we're allocating memory on page boundaries.
    * Page faults and cache misses are things to be avoided.
    */
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    _info.pagesize = si.dwPageSize;
    _info.heap_ptr = GetProcessHeap();
#else
    _info.pagesize = sysconf(_SC_PAGESIZE);
#endif

    _info.ready = 1;
#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: initialized the allocator.\n");
#endif
    return 0;
}

void* _smalloc_osalloc(size_t len)
{
    void* ret = NULL;

    if (len % _info.pagesize != 0) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: osalloc: must allocate in multiples of %ld.",
            _info.pagesize);
        fprintf(stderr, "Requested %u.\n", len);
#endif
        return NULL;
    }

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: Requested %lu bytes of memory.\n", len);
#endif

#ifdef _WIN32
    ret = HeapAlloc(_info.heap_ptr, 0, len);
#else
    ret = mmap(NULL, len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    if (ret == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: osalloc: failed to allocate memory.\n");
#endif
    }

    return ret;
}
