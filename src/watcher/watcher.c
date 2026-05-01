#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

#define ROOT_PARENT_INDEX  (-1)
#define MAX_EVENTS         (1024)
#define MAX_DIRS           (8192)
#define MAX_NAME_SIZE      (NAME_MAX + 1)
#define MAX_EVENT_SIZE     (sizeof(struct inotify_event) + MAX_NAME_SIZE)
#define INOTIFY_BUF_SIZE   (MAX_EVENTS * MAX_EVENT_SIZE)
#define EVENT_FLAGS        (IN_CREATE | IN_MOVED_TO)
#define MAX_WD             (8192)
#define INDEX_NOT_FOUND    (MAX_WD)
#define MEMORY_SIZE        (1024 * 1024)

typedef struct
{
    int    wd;
    int    parent_index;
    str_slice name;
} directory;

typedef struct
{
    int    index;
    directory entries[MAX_DIRS];
} watch_list;

// globals
static char       memory[MEMORY_SIZE] = {};
static int        wd_to_index[MAX_WD];
static char       inotify_buffer[INOTIFY_BUF_SIZE];
static watch_list watched_dirs;

str_slice build_full_path(int parent_index, const char* dirname)
{
    static char buffer[PATH_MAX] = {};

    // null terminate the buffer
    char *p = buffer + PATH_MAX;
    *--p = '\0';

    size_t total_len = 0;

    // copy basename into buffer
    usize len = strlen(dirname);
    p -= len;
    memcpy(p, dirname, len);
    total_len += len;

    if (parent_index != ROOT_PARENT_INDEX)
    {
        *--p = '/';
        total_len++;
    }

    while (parent_index != ROOT_PARENT_INDEX)
    {
        directory *dir = &watched_dirs.entries[parent_index];
        p -= dir->name.length;
        memcpy(p, dir->name.data, dir->name.length);
        total_len += dir->name.length;

        if (dir->parent_index != ROOT_PARENT_INDEX)
        {
            *--p = '/';
            total_len++;
        }

        parent_index = dir->parent_index;
    }

    str_slice result = {};
    result.length = total_len;
    result.data = p;

    return result;
}

int register_dir(const char* dirname, int wd, int parent_index, arena* arena)
{
    if (watched_dirs.index >= MAX_DIRS)
    {
        fprintf(stderr, "max directories reached\n");
        exit(EXIT_FAILURE);
    }

    if (wd >= MAX_WD)
    {
        fprintf(stderr, "watch descriptor %d is greater than MAX_WD: %d\n", wd, MAX_WD);
        exit(EXIT_FAILURE);
    }

    int index = watched_dirs.index++;
    wd_to_index[wd] = index;

    directory* dir = &watched_dirs.entries[index];
    dir->wd = wd;
    dir->parent_index = parent_index;
    dir->name = push_zstring(arena, dirname);

    return index;
}

void watch_dir(int inotify_fd, const char* dirname, int parent_index, arena* arena)
{
    // add it to watch list
    str_slice fullpath = build_full_path(parent_index, dirname);
    int wd = inotify_add_watch(inotify_fd, fullpath.data, EVENT_FLAGS);
    if (wd < 0)
    {
        printf("inotify_fd: %d dirname: %s parent_index: %d result: %d\n",
                inotify_fd, dirname, parent_index, wd);
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    int index = register_dir(dirname, wd, parent_index, arena);

    printf("watching :: %s\n", fullpath.data);

    // recurse on it
    DIR *dirp = opendir(fullpath.data);
    if (dirp)
    {
        struct dirent *dp;
        while ((dp = readdir(dirp)) != NULL)
        {
            if ((dp->d_type == DT_DIR) && ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..") != 0)))
            {
                watch_dir(inotify_fd, dp->d_name, index, arena);
            }
        }

        closedir(dirp);
    }
    else
    {
        perror("opendir");
    }

}

int find_directory_index(int wd)
{
    if (wd >= MAX_WD)
        return INDEX_NOT_FOUND;

    return wd_to_index[wd];
}

int watcher_main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: %s <DIR>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int inotify_fd = inotify_init();
    if (inotify_fd < 0)
    {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    arena arena = arena_create(memory, MEMORY_SIZE);
    char *root_dirname = argv[1];
    watch_dir(inotify_fd, root_dirname, ROOT_PARENT_INDEX, &arena);

    for (;;)
    {
        ssize_t bytes_read = read(inotify_fd, inotify_buffer, INOTIFY_BUF_SIZE);
        if (bytes_read < 0) perror("read");

        for (ssize_t i = 0; i < bytes_read;)
        {
            struct inotify_event *event = (struct inotify_event *)&inotify_buffer[i];

            // register new dir
            if ((event->mask & IN_ISDIR) && (event->mask & (IN_CREATE | IN_MOVED_TO)))
            {
                int parent_index = find_directory_index((size_t)event->wd);
                if (parent_index != INDEX_NOT_FOUND)
                {
                    watch_dir(inotify_fd, event->name, parent_index, &arena);
                }
            }

            i += (ssize_t)sizeof(struct inotify_event) + event->len;
        }
    }

    close(inotify_fd);

    exit(EXIT_SUCCESS);
}

