#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define SHM_FILE "/tmp/coverage_shm.bin"
#define SHM_SIZE_BYTES (512 * 1024 * 1024)

int main() {
    // Create or truncate the file
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Extend to desired size - this will create a sparse file filled with zeros
    if (ftruncate(fd, SHM_SIZE_BYTES) != 0) {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Shared memory file initialized at %s with size %lu bytes\n", SHM_FILE, (unsigned long)SHM_SIZE_BYTES);
    return 0;
}
