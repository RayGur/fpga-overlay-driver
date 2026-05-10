/*
 * test/test_mmio.c — Test for mmio module
 *
 * 需要先載入有 axi_gpio 的 bitstream，再執行此測試。
 *
 * Build:
 *   gcc -Wall -I./include -DZYNQ7000 -o test/test_mmio \
 *       test/test_mmio.c src/mmio.c
 *
 * Usage:
 *   sudo ./test/test_mmio <phys_base_addr_hex> [write_value_hex]
 *
 * Examples:
 *   sudo ./test/test_mmio 0x41200000            # read-only test
 *   sudo ./test/test_mmio 0x41200000 0xFF       # read-write-readback test
 *
 * Tests:
 *   1. mmio_open() succeeds
 *   2. mmio_read32() at offset 0x00 returns without crash
 *   3. (if write_value given) mmio_write32() + readback matches
 *   4. mmio_close() completes without error
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "../src/mmio.h"
#include "../include/config.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <phys_base_addr_hex> [write_value_hex]\n", argv[0]);
        return 1;
    }

    fpga_addr_t base_addr = (fpga_addr_t)strtoull(argv[1], NULL, 16);
    int do_write = (argc >= 3);
    uint32_t write_val = do_write ? (uint32_t)strtoul(argv[2], NULL, 16) : 0;
    int pass = 1;

    printf("[test_mmio] phys_base = " FPGA_ADDR_FMT "\n", base_addr);

    /* ---- Test 1: mmio_open -------------------------------------------- */
    mmio_region_t *region = mmio_open(base_addr, 0x10000);
    if (!region) {
        fprintf(stderr, "FAIL [test1]: mmio_open returned NULL\n");
        return 1;
    }
    printf("PASS [test1]: mmio_open succeeded ✓\n");

    /* ---- Test 2: read offset 0x00 -------------------------------------- */
    uint32_t val = mmio_read32(region, 0x00);
    printf("PASS [test2]: mmio_read32(0x00) = 0x%08X ✓\n", val);

    /* ---- Test 3: write + readback (optional) --------------------------- */
    if (do_write) {
        mmio_write32(region, 0x00, write_val);
        uint32_t readback = mmio_read32(region, 0x00);
        if (readback == write_val) {
            printf("PASS [test3]: write 0x%08X → readback 0x%08X ✓\n",
                   write_val, readback);
        } else {
            fprintf(stderr, "FAIL [test3]: wrote 0x%08X but read back 0x%08X\n",
                    write_val, readback);
            pass = 0;
        }
    } else {
        printf("SKIP [test3]: no write_value provided\n");
    }

    /* ---- Test 4: mmio_close ------------------------------------------- */
    mmio_close(region);
    printf("PASS [test4]: mmio_close completed ✓\n");

    printf("\n%s\n", pass ? "All tests PASSED." : "Some tests FAILED.");
    return pass ? 0 : 1;
}
