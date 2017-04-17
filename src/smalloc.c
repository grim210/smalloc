#include "smalloc.h"

#ifdef SMALLOC_DEBUG
  #include <stdio.h>
#endif

/*
* The actual chunks of memory that are given to the calling function.
* Enough memory will be used to fulfill the request, plus store the
* metadata in the _smalloc_chunk_t structure as well.
*
* |---------------------- total chunk memory ---------------------------|
* |-- chunk metadata ---|------------------ user memory ----------------|
*
*/
struct _smalloc_chunk_t {
    size_t len;
    unsigned freed;
    struct _smalloc_chunk_t* next;
};

/*
* This structure represents a group of pages that the allocator can
* allocate smaller chunks from.
*
* npages - The number of pages allocated for this group.  This data
*     structure occupies the first few pages of that group of pages.
* lenbytes - The number of bytes in this page group, minus the space
*     taken by this structure.
* bytesfree - The total number of bytes free that can be allocated to
*     a calling function.
* maxfree - The largest piece of unfragmented memory.  Useful for quickly
*     determine if a page group can support a memory allocation request.
* chunks - a singly linked list of the allocated chunks in this page group.
* next - the next page group.
*
* |------------------------- raw page group ----------------------------|
* |-- metadata --|--------------------- chunks -------------------------|
*
*/
struct _smalloc_pagegroup_t {
    size_t npages;
    size_t lenbytes;
    size_t bytesfree;
    size_t maxfree;
    struct _smalloc_chunk_t* chunks;
    struct _smalloc_pagegroup_t* next;
};

/*
* This variable allows you to tune the smallest group of pages your
* program can allocate.  If you know that you'll be working with large
* contiguous blocks of memory, it's best to tune this higher; the allocator
* will spend less time hunting for page groups to fulfill a request.
*/
#ifndef SMALLOC_SMALLEST_PAGE_GROUP
#define SMALLOC_SMALLEST_PAGE_GROUP     (1)
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
    struct _smalloc_pagegroup_t *pglist;
#ifdef _WIN32
    HANDLE heap_ptr;
#endif
} _info = {0};

/*
* Private function prototypes for page group management.
*/
void* _pgroup_alloc(size_t count);
int   _pgroup_append(struct _smalloc_pagegroup_t* list, void* block);
int   _pgroup_cleanup(struct _smalloc_pagegroup_t* list);

int _smalloc_append(struct _smalloc_block_t* list, void* block);
int _smalloc_clean(struct _smalloc_block_t* list);
int _smalloc_free(struct _smalloc_block_t* list, void* block);
int _smalloc_init(void);
void* _smalloc_osalloc(size_t size);

void *smalloc(size_t size)
{
    int result;
    void* mem = NULL;

    struct _smalloc_block_t header;
    struct _smalloc_block_t *ptr;

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
        return NULL;
    }

#ifdef _WIN32
    ret = HeapAlloc(_info.heap_ptr, 0, len);
#else
    ret = mmap(NULL, len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    return ret;
}

void*
_pgroup_alloc(size_t count)
{
    void* ret = NULL;
    size_t len = count * _info.pagesize;

    ret = mmap(0, len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0L);

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: _pgroup_alloc: requested %lu bytes.\n", len);
    if (!ret) {
        fprintf(stderr, "ERROR: failed to allocate page group.\n");
    }
#endif

    return ret;
}

int
_pgroup_append(struct _smalloc_pagegroup_t* list, void* block)
{
    if (list->next) {
        return _pgroup_append(list->next, block);
    }

    list->next = (struct _smalloc_pagegroup_t*)block;
    return 0;
}

int
_pgroup_cleanup(struct _smalloc_pagegroup_t* list)
{
    return -1;
}
