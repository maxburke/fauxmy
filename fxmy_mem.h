#ifndef FXMY_MEM_H
#define FXMY_MEM_H

void *
fxmy_malloc(size_t size);

void *
fxmy_calloc(size_t num_elements, size_t element_size);

void *
fxmy_realloc(void *ptr, size_t size);

void
fxmy_free(void *ptr);

int
fxmy_get_num_allocations(void);

#endif