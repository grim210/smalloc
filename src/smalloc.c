#include "smalloc.h"

#ifdef SMALLOC_DEBUG
  #include <assert.h>
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
#define SMALLOC_SMALLEST_PAGE_GROUP     (8)
#endif

static struct _smalloc_info {
    int ready;
    size_t pagesize;
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
int _pgroup_append(struct _smalloc_pagegroup_t* list, void* block);

/*
* _pgroup_cleanup:
* This function traverses the entire list and looks for page groups that
* have no memory in use and releases them back to the OS.
*
* list - the pagegroup list to be pruned of free page groups.
*
* returns 0 on success, less than 0 on failure.
*/
int _pgroup_cleanup(struct _smalloc_pagegroup_t* list);

/*
* _pgroup_fits:
* This function queries a particular page group to determine if the
* page group 'pg' can support a chunk allocation of 'size' bytes.
*
* pg - the page group to query.
* size - the size that needs to be supported by the allocation.  This
*     function does the math for calcuating chunk metadata plus size.
*
* returns a number greater than zero if the allocation is supported, but
*     zero is returned if the allocation cannot be supported in this group.
*/
int _pgroup_fits(struct _smalloc_pagegroup_t* pg, size_t size);

/*
* _pgroup_reserve:
* Reserves a chunk in the pagegroup 'pg' of 'size' bytes.  When the chunk
* is allocated, it is not initialized.  However, the chunk *will* be added
* to the page group's list of chunks.
*
* pg - the page group to reserve the chunk from
* size - the number of bytes that needs to be supported.  This size, plus
*     the size of the chunk structure will be reserved in the page group.
*
* returns a chunk of size + sizeof(chunk_t) bytes.  Or NULL on failure.
*/
struct _smalloc_chunk_t* _pgroup_reserve(struct _smalloc_pagegroup_t* pg,
    size_t size);

int _smalloc_init(void);

/*
* Public functions exposed in smalloc.h
*/
void *smalloc(size_t size)
{
    struct _smalloc_chunk_t* chk;
    struct _smalloc_pagegroup_t* pg;

    /*
    * If the structure has been initialized (most likely case), we
    * look through the existing list of pages and see if we have any
    * groups that can support the size request.
    */

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: smalloc: Asking for %lu bytes.\n", size);
#endif

    if (_info.ready) {
        pg = _info.pglist;
        while (pg && !_pgroup_fits(pg, size)) {
            pg = pg->next;
        }
    } else {
        if (_smalloc_init()) {
#ifdef SMALLOC_DEBUG
            fprintf(stderr, "ERROR: smalloc: Failed to initialization.\n");
#endif
            return NULL;
        }
        pg = _pages_alloc(size, SMALLOC_SMALLEST_PAGE_GROUP);
        if (!pg) {
#ifdef SMALLOC_DEBUG
            fprintf(stderr, "ERROR: failed to initialize smalloc\n");
#endif
            return NULL;

        }
        _info.pglist = pg;
    }


#ifdef SMALLOC_DEBUG
    /* Sanity check to ensure the allocator was initialized. */
    assert(_info.ready);
#endif

    /*
    * If we weren't able to find a page group to support the
    * memory request in the above while() loop, we must ask
    * the OS for more pages with a call to _pages_alloc.
    */
    if (!pg) {
#ifdef SMALLOC_DEBUG
        fprintf(stdout, "INFO: smalloc: No page group was found "
            "to support %lu bytes.\n", size);
#endif
        pg = _pages_alloc(size, SMALLOC_SMALLEST_PAGE_GROUP);
        if (!pg) {
#ifdef SMALLOC_DEBUG
            fprintf(stderr, "ERROR: smalloc: Failed to allocate %lu "
                "bytes.\n", size);
#endif
            return NULL;
        }

        /*
        * Once the page group has been successfully allocated,
        * ensure the reference to the group isn't left dangling.
        * Append it to the list of pages in the _info structure.
        */
        _pgroup_append(_info.pglist, pg);
    }


    if (pg == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: smalloc: Failed to find/allocate page "
            "group.\n");
#endif
        return NULL;
    }

    /*
    * Ask for a chunk from the page group.  When we get the chunk, all
    * the internal metadata will not be initialized.  Do that here.
    */
    chk = _pgroup_reserve(pg, size);
    if (chk == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: smalloc: Failed to reserve chunk from "
            "page group.\n");
#endif
        return NULL;
    }

    chk->ptr = chk + sizeof(struct _smalloc_chunk_t);
    chk->len = size;
    chk->freed = 0;

    return chk->ptr;
}

int
_smalloc_init(void)
{
    /* Determine the page size of the underlying OS */
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    _info.pagesize = si.dwPageSize;
    _info.heap_ptr = GetProcessHeap();
    if (_info.heap_ptr == NULL) {
#ifdef SMALLOC_DEBUG
        fprintf(stderr, "ERROR: _smalloc_init: Failed to get heap pointer "
            "from GetProcessHeap() call.\n");
#endif
        return -1;
    }
#else
    _info.pagesize = sysconf(_SC_PAGESIZE);
#endif
    _info.ready = 1;

    return 0;
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

struct _smalloc_chunk_t*
_pgroup_reserve(struct _smalloc_pagegroup_t* pg, size_t size)
{
    struct _smalloc_chunk_t* chunk;

    /* Sanity check. */
    assert(pg && (size != 0));

    /*
    * Allocate the chunk, but let the calling function do
    * the initalization and cleanup on the chunks behalf.
    */
    chunk = (struct _smalloc_chunk_t*)pg->top;
    chunk->next = NULL;

    /*
    * Make changes to our internal pagegroup structure to ensure accuracy.
    */
    pg->top += (size + sizeof(struct _smalloc_chunk_t));
    pg->bytesfree -= (size + sizeof(struct _smalloc_chunk_t));

    /* Append the newly allocated chunk to the group's chunk list. */
    if (pg->chunks == NULL) {
        pg->chunks = chunk;
    } else {
        while (pg->chunks->next != NULL) {
            pg->chunks = pg->chunks->next;
        }
        pg->chunks->next = chunk;
    }

#ifdef SMALLOC_DEBUG
    fprintf(stdout, "INFO: _pgroup_reserve: %lu bytes free in current "
        " page group.\n", pg->bytesfree);
#endif

    return chunk;
}
