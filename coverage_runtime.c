#include <stdint.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SHM_BASE "/tmp/coverage_shm"
#define SHM_BASE (getenv("COVERAGE_SHM_BASE") != NULL ? getenv("COVERAGE_SHM_BASE") : DEFAULT_SHM_BASE)
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
    
    // Get FUZZER_ID from environment
    const char* fuzzer_id = getenv("FUZZER_ID");
    char shm_file[256] = {0};

    if (fuzzer_id) {
        snprintf(shm_file, sizeof(shm_file), "%s_%s.bin", SHM_BASE, fuzzer_id);
    } else {
        snprintf(shm_file, sizeof(shm_file), "%s.bin", SHM_BASE);
    }

    if (getenv("COVERAGE_DEBUG")) {
        fprintf(stderr, "mapping shared memory file: %s\n", shm_file);
    }

    int fd = open(shm_file, O_RDWR);
    if (fd < 0) {
        // Try to create the file if it doesn't exist
        fd = open(shm_file, O_RDWR | O_CREAT, 0666);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        // Extend to desired size
        if (ftruncate(fd, SHM_SIZE_BYTES) != 0) {
            perror("ftruncate");
            close(fd);
            exit(1);
        }
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
        if (getenv("COVERAGE_DEBUG")) {
            fprintf(stderr, "pushing block_id: %d\n", block_id);
        }
        coverage_shm[index + 1] = block_id;
    } else {
        if (getenv("COVERAGE_DEBUG")) {
            fprintf(stderr, "max trace entries reached\n");
        }
    }
}
