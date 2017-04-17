#ifndef SMALLOC_H
#define SMALLOC_H

#include <sys/mman.h>
#include <unistd.h>

#ifdef SMALLOC_DEBUG
  #include <stdio.h>
  #include <string.h>
#endif

#define SMALLOC_SUCCESS     ( 0)
#define SMALLOC_SBRK_FAIL   (-1)

void *smalloc(size_t size);
void *scalloc(size_t nmemb, size_t size);
void  sfree(void *ptr);

#endif
