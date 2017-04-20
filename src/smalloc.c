#include "smalloc.h"

#ifdef SMALLOC_DEBUG
  #include <stdio.h>
#endif

/*
* The actual chunks of memory that are given to the calling function.
* Enough memory will be used to fulfill the request, plus store the
* metadata in the _smalloc_chunk_t structure as well.
*
* ptr - the memory handed off to the function calling malloc(3)
* len - the length of the memory pointed to by 'ptr'
* freed - initially set to 0, but after the user calls free(3) with
*     ptr, freed will be set to 1.  And the allocator can then do what
*     it wants with this chunk of memory.
* next - the next chunk.  This is a list whose limit is the number of
*     allocations that can fit within a page group.
*
* |---------------------- total chunk memory ---------------------------|
* |-- chunk metadata ---|------------------ user memory ----------------|
*
*/
struct _smalloc_chunk_t {
    void *ptr;
    size_t len;
    unsigned freed;
    struct _smalloc_chunk_t* next;
};

/*
* This structure represents a group of pages that the allocator can
* allocate smaller chunks from.
*
* top - The start of the memory available for 'chunking'.
* npages - The number of pages allocated for this group.  This data
*     structure occupies the first few pages of that group of pages.
* lenbytes - The number of bytes in this page group, minus the space
*     taken by this structure.
* bytesfree - The total number of bytes free that can be allocated to
*     a calling function.
* chunks - a singly linked list of the allocated chunks in this page group.
* next - the next page group.
*
* |------------------------- raw page group ----------------------------|
* |-- metadata --|--------------------- chunks -------------------------|
*
*/
struct _smalloc_pagegroup_t {
    void* top;
    size_t npages;
    size_t lenbytes;
    size_t bytesfree;
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

/*
* _pages_alloc:
* This function calls the underlying OS memory allocation routines to
* reserve pages for the allocator.
*
* size - number of bytes that are required to allocate to the program.
* pcount - number of pages to allocate to fulfill the request.  If a value
*     of zero is provided, the function will make its best guess to fulfill
*     the 'size' requirement.  Extra pages may be requested regardless if
*     those pages will not be needed to meet the size required.  If pcount
*     is less than the minimum amount of pages required to fulfill the size
*     requirement, the minimum number of pages required to fulfill the
*     request will be allocated.
*
* returns a page group that was allocated.
*/
struct _smalloc_pagegroup_t* _pages_alloc(size_t size, size_t pcount);

/*
* _pgroup_append:
* This takes a group of pages, taken from _pages_alloc, and attaches
* it to the end of the pagegroup list.  All the pagegroup metadata is
* initialized before it is appended.
*
* list - list of pagegroups that have already been reserved by the
*     allocator.  The head node for this list is found in '_info'.
* block - block of memory to append to the end of the list.  The function
*     calls for a void pointer, but the memory can be of type void* or
*     of type pagegroup_t*.
*
* returns 0 on success, less than 0 on failure.
*/
int   _pgroup_append(struct _smalloc_pagegroup_t* list, void* block);

/*
* _pgroup_cleanup:
* This function traverses the entire list and looks for page groups that
* have no memory in use and releases them back to the OS.
*
* list - the pagegroup list to be pruned of free page groups.
*
* returns 0 on success, less than 0 on failure.
*/
int   _pgroup_cleanup(struct _smalloc_pagegroup_t* list);

int   _pgroup_fits(struct _smalloc_pagegroup_t* pg, size_t size);
void* _pgroup_reserve(struct _smalloc_pagegroup_t* pg, size_t size);

int   _smalloc_init_(size_t initial);

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

void *smalloc2(size_t size)
{
    int result;
    void* mem;
    struct _smalloc_pagegroup_t* pg;

    if (!_info.ready) {
        result = _smalloc_init_(size);
        if (result) {
#ifdef SMALLOC_DEBUG
            fprintf(stderr, "ERROR: failed to initialize smalloc\n");
#endif
            return NULL;

        }

        return _info.pglist->chunks->ptr;
    }

    pg = _info.pglist;
    while (pg && !_pgroup_fits(pg, size)) {
        pg = pg->next;
    }

    /* XXX:
    * For now I'm going to leave this for later.  If we end up with a null
    * pointer, it means that the above while loop was not able to find
    * memory available in the page group list to support the malloc(3)
    * request.  Another group of pages will need to be allocated to support
    * the request from here.
    *
    * The intelligent thing to do is to create another page group and set
    * the pg variable to the new page group.
    */
    if (!pg) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "TODO: Allocate a new page group in the event that"
            " we run out of memory in the current page group.\n");
#endif
        return NULL;
    }

    mem = _pgroup_reserve(pg, size);

#ifdef SMALLOC_DEBUG
    if (mem == NULL) {
        fprintf(stderr, "ERROR: smalloc: failed to reserve a chunk of "
            "memory of size %lu\n", size);
    }
#endif

    return mem;
}

int _smalloc_init_(size_t initial)
{
    struct _smalloc_pagegroup_t *pg;

    size_t chunk_size;
    struct _smalloc_chunk_t *chk;

    /* Determine the page size of the underlying OS */
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    _info.pagesize = si.dwPageSize;
    _info.heap_ptr = GetProcessHeap();
#else
    _info.pagesize = sysconf(_SC_PAGESIZE);
#endif

    pg = _pages_alloc(initial, SMALLOC_SMALLEST_PAGE_GROUP);
    if (!pg) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: _smalloc_init: Failed to allocate "
            "%lu bytes.\n", initial);
#endif
        return -1;
    }

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: _smalloc_init: %lu bytes free in allocated "
        "page group.\n", pg->bytesfree);
#endif

    chunk_size = initial + sizeof(struct _smalloc_chunk_t);

    chk = (struct _smalloc_chunk_t*)pg->top;
    chk->len = initial;
    chk->freed = 0;
    chk->ptr = chk + sizeof(struct _smalloc_chunk_t);
    chk->next = NULL;

    pg->top += chunk_size;
    pg->chunks = chk;
    _info.pglist = pg;

    _info.ready = 1;
    return 0;
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

struct _smalloc_pagegroup_t*
_pages_alloc(size_t size, size_t pcount)
{
    void* ret = NULL;
    size_t len = pcount * _info.pagesize;
    size_t adjusted;
    size_t npages;
    struct _smalloc_pagegroup_t* pg;


    /*
    * 'adjusted' is how much memory will actually
    * be required to fulfill the size request.
    */
    adjusted = size + sizeof(struct _smalloc_pagegroup_t) +
        sizeof(struct _smalloc_chunk_t);

    /*
    * If the page count requested will fit all of
    * the adjusted size, no need to do more math.
    *
    * Otherwise, we need to figure out how many
    * pages it will take to fit all of size.
    */
    npages = 0;
    if (pcount * _info.pagesize >= adjusted) {
        len = pcount * _info.pagesize;
        npages = pcount;
    } else {
        len = _info.pagesize * (adjusted / _info.pagesize);
        len += _info.pagesize;
        npages = len / _info.pagesize;
    }

#ifdef SMALLOC_DEBUG
    if (len % _info.pagesize != 0) {
        fprintf(stderr, "ERROR: _pages_alloc: attempting improper page "
            "alignment for mmap(2)\n");
    }
#endif

#ifdef _WIN32
    ret = HeapAlloc(_info.heap_ptr, 0, len);
#else
    ret = mmap(0, len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0L);
#endif

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: _pgroup_alloc: requested %lu bytes, %lu pages\n",
        len, len / _info.pagesize);
    if (!ret) {
        fprintf(stderr, "ERROR: failed to allocate page group.\n");
    }
#endif

    pg = (struct _smalloc_pagegroup_t*)ret;
    pg->top = ret + sizeof(struct _smalloc_pagegroup_t);
    pg->npages = npages;
    pg->bytesfree = (pg->npages * _info.pagesize) -
        sizeof(struct _smalloc_pagegroup_t);
    pg->chunks = NULL;
    pg->next = NULL;

    return pg;
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
_pgroup_fits(struct _smalloc_pagegroup_t* pg, size_t size)
{
    /* we have to ensure we save space for the metadata when looking */
    if (pg->bytesfree >= (size + sizeof(struct _smalloc_chunk_t))) {
        return 1;
    }

    return 0;
}

int
_pgroup_cleanup(struct _smalloc_pagegroup_t* list)
{
    return -1;
}

void*
_pgroup_reserve(struct _smalloc_pagegroup_t* pg, size_t size)
{
    struct _smalloc_chunk_t* chunk;
    struct _smalloc_chunk_t* clist;

    chunk = (struct _smalloc_chunk_t*)pg->top;
    pg->top += (size + sizeof(struct _smalloc_chunk_t));
    pg->bytesfree -= (size + sizeof(struct _smalloc_chunk_t));

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: _pgroup_reserve: %lu bytes free in current "
        " page group.\n", pg->bytesfree);
#endif

    chunk->ptr = (chunk + sizeof(struct _smalloc_chunk_t));
    chunk->len = size;
    chunk->freed = 0;
    chunk->next = NULL;

    /*
    * Don't forget to modify the linked list of chunks so that
    * we don't lose track of memory that we've allocated out.
    */
    clist = pg->chunks;
    while (clist->next) {
        clist = clist->next;
    }

    clist->next = chunk;

    return chunk->ptr;
}
