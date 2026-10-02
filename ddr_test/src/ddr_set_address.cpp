#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Vivado MCS_0 control base and generated xmcs_hw.h counts offsets.
constexpr uint64_t CONTROL_BASE = 0x40000000;
constexpr size_t COUNTS_LOW = 0x10;
constexpr size_t COUNTS_HIGH = 0x14;

static void barrier()
{
#if defined(__arm__) || defined(__aarch64__)
    asm volatile("dsb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::printf("Usage: %s DDR_PHYSICAL_ADDRESS\n"
                    "Set MCS_0 counts pointer at control base 0x40000000.\n"
                    "Keep PL start inactive while programming both address words.\n"
                    "Use a reserved DDR buffer; this does not allocate or clear it.\n",
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
    const auto address = std::strtoull(argv[1], &end, hex ? 16 : 10);
    // Local SDT DDR range; leave room for all ten uint32_t bins.
    if (errno || end == argv[1] || *end ||
        argv[1][0] < '0' || argv[1][0] > '9' ||
        address < 0x00100000 || address > 0x20000000ULL - 40 || address % 4) {
        std::fprintf(stderr, "Address must be 4-byte aligned within DDR "
                             "0x00100000..0x1fffffd8.\n");
        return 2;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        std::fprintf(stderr, "Cannot determine page size\n");
        return 1;
    }
    const uint64_t map_base = CONTROL_BASE - CONTROL_BASE % page_size;
    const size_t offset = static_cast<size_t>(CONTROL_BASE - map_base);
    const size_t length = ((offset + COUNTS_HIGH + 4 + page_size - 1) / page_size) * page_size;
    const int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        std::fprintf(stderr, "open /dev/mem failed: %s\n", std::strerror(errno));
        return 1;
    }
    void* mapped = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                        fd, static_cast<off_t>(map_base));
    const int map_errno = errno;
    close(fd);
    if (mapped == MAP_FAILED) {
        std::fprintf(stderr, "mmap control failed: %s\n", std::strerror(map_errno));
        return 1;
    }
    auto* regs = reinterpret_cast<volatile uint32_t*>(
        static_cast<unsigned char*>(mapped) + offset);
    // Two separate AXI-Lite writes: the IP must remain idle throughout.
    regs[COUNTS_LOW / 4] = static_cast<uint32_t>(address);
    barrier();
    regs[COUNTS_HIGH / 4] = 0;
    barrier();
    const uint32_t low = regs[COUNTS_LOW / 4];
    const uint32_t high = regs[COUNTS_HIGH / 4];
    barrier();
    const uint64_t actual = (static_cast<uint64_t>(high) << 32) | low;
    int result = 0;
    if (actual != address) {
        std::fprintf(stderr, "Readback mismatch: requested 0x%016" PRIx64
                             ", read 0x%016" PRIx64 "\n",
                     static_cast<uint64_t>(address), actual);
        result = 1;
    } else {
        std::printf("HLS counts address set and verified: 0x%016" PRIx64 "\n", actual);
    }
    if (munmap(mapped, length) != 0) {
        std::fprintf(stderr, "munmap failed: %s\n", std::strerror(errno));
        result = 1;
    }
    return result;
}
