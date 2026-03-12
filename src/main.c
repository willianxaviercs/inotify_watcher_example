#include <string.h>
// base
#include "base/base_types.c"
#include "base/arena.c"
#include "base/string.c"

// app
#include "watcher/watcher.c"

int main(int argc, char **argv)
{
    return watcher_main(argc, argv);
}

