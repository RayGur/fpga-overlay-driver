/*
 * test/test_fpga_load.c — End-to-end test for fpga_load module
 *
 * ⚠️  WARNING: This test writes to /lib/firmware/ and triggers FPGA bitstream
 *     loading via sysfs. This is an IRREVERSIBLE operation — it overwrites the
 *     current PL design. Requires root privileges.
 *
 * Build:
 *   gcc -Wall -I./include -DZYNQ7000 -o test/test_fpga_load \
 *       test/test_fpga_load.c src/bit2bin.c src/fpga_load.c
 *
 * Usage:
 *   sudo ./test/test_fpga_load <path/to/design.bit>
 *
 * Tests:
 *   1. convert_bit_to_bin() succeeds and produces a .bin file
 *   2. fpga_load_bitstream() returns 0 (FPGA Manager accepted the bitstream)
 *   3. fpga_get_state() reports "operating" after load
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/bit2bin.h"
#include "../src/fpga_load.h"

/* Derive .bin path from .bit path: replace last extension with .bin */
static void make_bin_path(const char *bit_path, char *bin_path, int bin_len)
{
    strncpy(bin_path, bit_path, bin_len - 1);
    bin_path[bin_len - 1] = '\0';

    char *dot = strrchr(bin_path, '.');
    if (dot)
        *dot = '\0';
    strncat(bin_path, ".bin", bin_len - strlen(bin_path) - 1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <design.bit>\n", argv[0]);
        return 1;
    }
    const char *bit_path = argv[1];
    int pass = 1;

    /* ---- Test 1: convert .bit → .bin ---------------------------------- */
    char bin_path[512];
    make_bin_path(bit_path, bin_path, sizeof(bin_path));

    printf("[test1] Converting %s → %s\n", bit_path, bin_path);
    if (convert_bit_to_bin(bit_path, bin_path) != 0) {
        fprintf(stderr, "FAIL [test1]: convert_bit_to_bin failed\n");
        return 1;
    }
    printf("PASS [test1]: .bin created at %s ✓\n", bin_path);

    /* ---- Test 2: load bitstream via FPGA Manager ---------------------- */
    printf("[test2] Loading bitstream (requires root, writes to /lib/firmware/)...\n");
    if (fpga_load_bitstream(bin_path) != 0) {
        fprintf(stderr, "FAIL [test2]: fpga_load_bitstream failed\n");
        pass = 0;
    } else {
        printf("PASS [test2]: fpga_load_bitstream returned 0 ✓\n");
    }

    /* ---- Test 3: verify state is "operating" -------------------------- */
    char state[64] = {0};
    if (fpga_get_state(state, sizeof(state)) != 0) {
        fprintf(stderr, "FAIL [test3]: fpga_get_state failed\n");
        pass = 0;
    } else if (strcmp(state, "operating") != 0) {
        fprintf(stderr, "FAIL [test3]: expected 'operating', got '%s'\n", state);
        pass = 0;
    } else {
        printf("PASS [test3]: FPGA state = '%s' ✓\n", state);
    }

    printf("\n%s\n", pass ? "All tests PASSED." : "Some tests FAILED.");
    return pass ? 0 : 1;
}
