#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define DEFAULT_SHM_FILE "/tmp/coverage_shm.bin"
#define SHM_FILE (getenv("COVERAGE_SHM_FILE") != NULL ? getenv("COVERAGE_SHM_FILE") : DEFAULT_SHM_FILE)
#define SHM_SIZE_BYTES (512 * 1024 * 1024)

int main() {
    int fd = open(SHM_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    uint32_t *shm = mmap(NULL, SHM_SIZE_BYTES, PROT_READ, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);
    uint32_t len = shm[0];
    printf("Collected Trace Length: %u\n", len);
    for (uint32_t i = 0; i < len && i < ((SHM_SIZE_BYTES / 4) - 1); i++) {
        printf("Block %u ID: %u\n", i, shm[i + 1]);
    }
    munmap(shm, SHM_SIZE_BYTES);
    return 0;
}
