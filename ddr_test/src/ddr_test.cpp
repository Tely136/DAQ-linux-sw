#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    constexpr size_t BIN_COUNT = 10;
    constexpr size_t DATA_SIZE = BIN_COUNT * sizeof(uint32_t);
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::printf("Usage: %s DDR_PHYSICAL_ADDRESS\n"
                    "Read 10 contiguous uint32_t bins from reserved DDR.\n"
                    "Address is decimal or 0x-prefixed hex, aligned to 4 bytes.\n"
                    "Reserve memory before boot; stop the HLS writer before reading.\n",
                    argv[0]);
        return 0;
    }
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s DDR_PHYSICAL_ADDRESS (see --help)\n", argv[0]);
        return 2;
    }
    char* end = nullptr;
    errno = 0;
    const bool hex = std::strncmp(argv[1], "0x", 2) == 0 ||
                     std::strncmp(argv[1], "0X", 2) == 0;
    const unsigned long long address = std::strtoull(argv[1], &end, hex ? 16 : 10);
    if (errno || end == argv[1] || *end != '\0' ||
        argv[1][0] < '0' || argv[1][0] > '9' ||
        address > UINT32_MAX - (DATA_SIZE - 1) || address % sizeof(uint32_t)) {
        std::fprintf(stderr, "Invalid aligned 32-bit physical address: %s\n", argv[1]);
        return 2;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        std::fprintf(stderr, "Cannot determine system page size\n");
        return 1;
    }
    const auto page = static_cast<uint64_t>(page_size);
    const uint64_t map_base = address - address % page;
    const size_t offset = static_cast<size_t>(address - map_base);
    const size_t map_size = ((offset + DATA_SIZE + page_size - 1) / page_size) * page_size;

    // Requires a reserved no-map DDR buffer and an uncached /dev/mem mapping.
    // O_SYNC/volatile do not make ordinary Linux RAM DMA coherent.
    const int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) {
        std::fprintf(stderr, "open /dev/mem failed: %s\n", std::strerror(errno));
        return 1;
    }
    void* mapped = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd,
                        static_cast<off_t>(map_base));
    const int map_errno = errno;
    close(fd);
    if (mapped == MAP_FAILED) {
        std::fprintf(stderr, "mmap DDR failed: %s\n", std::strerror(map_errno));
        return 1;
    }

    const volatile uint32_t* bins = reinterpret_cast<const volatile uint32_t*>(
        static_cast<const unsigned char*>(mapped) + offset);
    uint32_t values[BIN_COUNT];
    for (size_t i = 0; i < BIN_COUNT; ++i)
        values[i] = bins[i];

    std::printf("DDR physical base: 0x%08" PRIx32 " (10 x uint32_t)\n",
                static_cast<uint32_t>(address));
    for (size_t i = 0; i < BIN_COUNT; ++i)
        std::printf("bin %2zu = %" PRIu32 "\n", i, values[i]);

    if (munmap(mapped, map_size) != 0) {
        std::fprintf(stderr, "munmap failed: %s\n", std::strerror(errno));
        return 1;
    }
    return 0;
}
