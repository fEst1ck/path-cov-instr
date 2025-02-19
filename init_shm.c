#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define SHM_FILE "/tmp/coverage_shm.bin"
#define SHM_SIZE_BYTES 4096

int main() {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (ftruncate(fd, SHM_SIZE_BYTES) != 0) {
        perror("ftruncate");
        close(fd);
        return 1;
    }
    // Initialize the file with zeros.
    char buf[SHM_SIZE_BYTES];
    memset(buf, 0, sizeof(buf));
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("write");
        close(fd);
        return 1;
    }
    close(fd);
    printf("Shared memory initialized at %s with size %d bytes.\n", SHM_FILE, SHM_SIZE_BYTES);
    return 0;
}
