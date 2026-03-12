typedef struct
{
    char *base;
    usize pos;
    usize size;
} arena;

arena arena_create(void *memory, usize size)
{
    arena result = {};
    result.base = memory;
    result.pos = 0;
    result.size = size;

    return result;
}

void *arena_push(arena *a, usize size)
{
    if (a->pos + size > a->size)
        return NULL;

    void *ptr = a->base + a->pos;
    a->pos += size;

    return ptr;
}

