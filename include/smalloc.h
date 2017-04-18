#ifndef SMALLOC_H
#define SMALLOC_H

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

void *smalloc(size_t size);
void  sfree(void *ptr);
void *scalloc(size_t nmemb, size_t size);
void *srealloc(void *ptr, size_t size);

/* XXX: For testing purposes only. */
void *smalloc2(size_t size);
void  sfree2(void *ptr);
void *scalloc2(size_t nmemb, size_t size);
void *srealloc2(void* ptr, size_t size);

#endif
