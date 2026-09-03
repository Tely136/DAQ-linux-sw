#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <cstring>

int main()
{
    constexpr off_t BRAM_BASE = 0x40000000;   // change to your Vivado address
    constexpr size_t MAP_SIZE = 0x10000;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("open /dev/mem failed: %s\n", strerror(errno));
        return 1;
    }

    void* mapped = mmap(
        nullptr,
        MAP_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        BRAM_BASE
    );

    if (mapped == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    volatile uint32_t* bram =
        reinterpret_cast<volatile uint32_t*>(mapped);

    for (int i = 0; i < 16; ++i) {
        printf("bin %2d = %u\n", i, bram[i]);
    }

    munmap(mapped, MAP_SIZE);
    close(fd);

    return 0;
}