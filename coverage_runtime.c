#include <stdint.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define SHM_FILE "/tmp/coverage_shm.bin"
#define SHM_SIZE_BYTES (512 * 1024 * 1024)
#define NUM_ENTRIES (SHM_SIZE_BYTES / 4)
#define MAX_TRACE_ENTRIES (NUM_ENTRIES - 1)

#ifdef __cplusplus
extern "C" {
#endif

// Define the shared memory pointer.
uint32_t *coverage_shm = 0;

#ifdef __cplusplus
}
#endif

// Helper function to map the shared memory file.
static void map_shared_memory() {
    if (coverage_shm) return;
    int fd = open(SHM_FILE, O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    coverage_shm = (uint32_t *)mmap(NULL, SHM_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (coverage_shm == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    close(fd);
}

void __coverage_push(uint32_t block_id) {
    if (!coverage_shm) {
        map_shared_memory();
    }
    // Atomically increment the trace length (stored at index 0).
    uint32_t index = atomic_fetch_add((_Atomic uint32_t*)coverage_shm, 1);
    if (index < MAX_TRACE_ENTRIES) {
        coverage_shm[index + 1] = block_id;
    }
}
