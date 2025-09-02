#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    size_t os_page_size;
    size_t file_size;
} MmapConfig;

static void fatal(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static size_t get_os_page_size(void)
{
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size < 0)
    {
        return 4096;
    }
    return (size_t)page_size;
}

static const void *find_bytes(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
    if (nlen == 0) return haystack;
    if (hlen < nlen) return NULL;

    const unsigned char *h = haystack;
    const unsigned char *n = needle;
    size_t last = hlen - nlen;

    for (size_t i = 0; i <= last; ++i)
    {
        if (h[i] == n[0] && memcmp(h + i, n, nlen) == 0)
            return h + i;
    }
    return NULL;
}

static uint8_t *map_file(int fd, size_t length)
{
    void *address = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (address == MAP_FAILED)
    {
        fatal("mmap");
    }
    return (uint8_t *)address;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <file> <keyword>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];
    const char *keyword = argv[2];
    size_t keyword_len = strlen(keyword);

    if (keyword_len == 0)
    {
        fprintf(stderr, "Empty keyword not allowed\n");
        return EXIT_FAILURE;
    }

    MmapConfig cfg = {0};
    cfg.os_page_size = get_os_page_size();

    int fd = open(path, O_RDONLY);
    if (fd < 0) fatal("open");

    struct stat st;
    if (fstat(fd, &st) != 0) fatal("fstat");
    cfg.file_size = st.st_size;

    if (cfg.file_size == 0)
    {
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    uint8_t *base = map_file(fd, cfg.file_size);

    printf("Searching \"%s\" in file: %s\n", keyword, path);

    const uint8_t *cursor = base;
    size_t remaining = cfg.file_size;
    size_t matches = 0;

    while (true)
    {
        const void *found = find_bytes(cursor, remaining, keyword, keyword_len);
        if (!found) break;

        off_t pos = (const uint8_t *)found - base;
        printf("Match at byte offset: %" PRIu64 "\n", (uint64_t)pos);

        cursor = (const uint8_t *)found + keyword_len;
        remaining = cfg.file_size - (cursor - base);
        matches++;
    }

    if (matches == 0)
    {
        printf("No matches found.\n");
    }

    if (munmap(base, cfg.file_size) != 0) fatal("munmap");
    if (close(fd) != 0) fatal("close");

    return 0;
}
