#include <stdint.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SHM_BASE "/dev/shm/coverage_shm"
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

__attribute__((constructor))
static void cov_init(void){ map_shared_memory(); }

void __coverage_push(uint32_t block_id) {
    if (index < MAX_TRACE_ENTRIES) {
        coverage_shm[++*coverage_shm] = block_id;
    }
}
