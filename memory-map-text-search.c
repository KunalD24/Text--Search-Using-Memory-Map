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
    size_t page_size;
    size_t file_size;
} Config;

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static size_t get_page_size(void)
{
    long p = sysconf(_SC_PAGE_SIZE);
    if (p < 0) return 4096;
    return (size_t)p;
}

static const void *find_buf(const void *buf, size_t blen, const void *pat, size_t plen)
{
    if (plen == 0) return buf;
    if (blen < plen) return NULL;

    const unsigned char *b = buf;
    const unsigned char *p = pat;
    size_t last = blen - plen;

    for (size_t i = 0; i <= last; i++)
    {
        if (b[i] == p[0] && memcmp(b + i, p, plen) == 0)
            return b + i;
    }
    return NULL;
}

static uint8_t *map_fd(int fd, size_t len)
{
    void *addr = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) die("mmap");
    return (uint8_t *)addr;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <file> <pattern>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file = argv[1];
    const char *pat = argv[2];
    size_t plen = strlen(pat);

    if (plen == 0)
    {
        fprintf(stderr, "Empty pattern not allowed\n");
        return EXIT_FAILURE;
    }

    Config cfg = {0};
    cfg.page_size = get_page_size();

    int fd = open(file, O_RDONLY);
    if (fd < 0) die("open");

    struct stat st;
    if (fstat(fd, &st) != 0) die("fstat");
    cfg.file_size = st.st_size;

    if (cfg.file_size == 0)
    {
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    uint8_t *data = map_fd(fd, cfg.file_size);

    printf("Searching \"%s\" in file: %s\n", pat, file);

    const uint8_t *cur = data;
    size_t remain = cfg.file_size;
    size_t count = 0;

    while (1)
    {
        const void *pos = find_buf(cur, remain, pat, plen);
        if (!pos) break;

        off_t off = (const uint8_t *)pos - data;
        printf("Match at byte offset: %" PRIu64 "\n", (uint64_t)off);

        cur = (const uint8_t *)pos + plen;
        remain = cfg.file_size - (cur - data);
        count++;
    }

    if (count == 0) printf("No matches found.\n");

    if (munmap(data, cfg.file_size) != 0) die("munmap");
    if (close(fd) != 0) die("close");

    return 0;
}
