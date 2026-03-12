typedef struct
{
    char *data;
    usize length;
} str_slice;

str_slice push_zstring(arena *a, const char *str)
{
    usize len = strlen(str) + 1;
    str_slice result = {};
    result.data = arena_push(a, len);
    result.length = len - 1;

    memcpy(result.data, str, len);

    return result;
}

