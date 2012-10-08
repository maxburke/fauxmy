#ifdef _MSC_VER

#pragma warning(push, 0)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(pop)

static LONG num_allocations;

void *
fxmy_malloc(size_t size)
{
    HANDLE heap_handle;
    void *mem;

    heap_handle = GetProcessHeap();

    mem = HeapAlloc(heap_handle, HEAP_GENERATE_EXCEPTIONS, size);
    InterlockedIncrement(&num_allocations);

    return mem;
}

void *
fxmy_calloc(size_t num_elements, size_t element_size)
{
    HANDLE heap_handle;
    void *mem;
    size_t block_size;

    heap_handle = GetProcessHeap();
    block_size = num_elements * element_size;

    mem = HeapAlloc(heap_handle, HEAP_GENERATE_EXCEPTIONS, block_size);
    InterlockedIncrement(&num_allocations);
    memset(mem, 0, block_size);

    return mem;
}

void *
fxmy_realloc(void *ptr, size_t size)
{
    HANDLE heap_handle;
    void *mem;

    heap_handle = GetProcessHeap();

    mem = HeapReAlloc(heap_handle, HEAP_GENERATE_EXCEPTIONS, ptr, size);

    return mem;
}

void
fxmy_free(void *ptr)
{
    HANDLE heap_handle;

    heap_handle = GetProcessHeap();

    HeapFree(heap_handle, 0, ptr);
    InterlockedDecrement(&num_allocations);
}

int
fxmy_get_num_allocations(void)
{
    return InterlockedCompareExchange(&num_allocations, 0, 0);
}

#endif