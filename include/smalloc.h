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
void *scalloc(size_t nmemb, size_t size);
void  sfree(void *ptr);

#endif
