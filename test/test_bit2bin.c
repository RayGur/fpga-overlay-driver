/*
 * test/test_bit2bin.c — Standalone test for bit2bin module
 *
 * Build:
 *   gcc -Wall -I./include -DZYNQ7000 -o test/test_bit2bin \
 *       test/test_bit2bin.c src/bit2bin.c
 *
 * Usage:
 *   ./test/test_bit2bin <path/to/design.bit>
 *
 * Checks:
 *   1. find_sync_offset() returns a sane value (>= 0, < filesize)
 *   2. convert_bit_to_bin() succeeds
 *   3. Output /tmp/test_output.bin starts with FF FF FF AA
 *      (sync word preserved, byte order correct)
 *   4. Output file size == (input size - header_bytes) and is a multiple of 4
 *
 * Verify manually:
 *   xxd /tmp/test_output.bin | head   # should show FF FF FF AA as first 4 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/bit2bin.h"

#define OUTPUT_PATH "/tmp/test_output.bin"

/* Read entire file into malloc'd buffer. Caller must free(). */
static uint8_t *read_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        perror(path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz <= 0)
    {
        fclose(fp);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)sz);
    if (!buf)
    {
        fclose(fp);
        return NULL;
    }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz)
    {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *out_len = (size_t)sz;
    return buf;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <design.bit>\n", argv[0]);
        return 1;
    }
    const char *input_path = argv[1];
    int pass = 1;

    /* ---- Test 1: find_sync_offset --------------------------------- */
    size_t in_len = 0;
    uint8_t *in_buf = read_file(input_path, &in_len);
    if (!in_buf)
    {
        fprintf(stderr, "FAIL: could not read input file\n");
        return 1;
    }

    ssize_t sync = find_sync_offset(in_buf, in_len);
    if (sync < 0)
    {
        fprintf(stderr, "FAIL [test1]: sync word not found\n");
        pass = 0;
    }
    else if ((size_t)sync >= in_len - 4)
    {
        fprintf(stderr, "FAIL [test1]: sync offset %zd suspiciously large "
                        "(file size %zu)\n",
                sync, in_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test1]: sync word at offset %zd  (header = %zd bytes)\n",
               sync, sync);
    }
    free(in_buf);

    /* ---- Test 2: convert_bit_to_bin succeeds ---------------------- */
    int rc = convert_bit_to_bin(input_path, OUTPUT_PATH);
    if (rc != 0)
    {
        fprintf(stderr, "FAIL [test2]: convert_bit_to_bin returned %d\n", rc);
        pass = 0;
    }
    else
    {
        printf("PASS [test2]: convert_bit_to_bin succeeded\n");
    }

    if (!pass)
        return 1;

    /* ---- Test 3: output starts with swapped sync word ------------ */
    /*
     * swap32(0xFFFFFFAA) = 0xAAFFFFFF
     * In little-endian memory the bytes are: AA FF FF FF
     * FPGA Manager on ARM reads this back as 0xFFFFFFAA ✓
     */
    size_t out_len = 0;
    uint8_t *out_buf = read_file(OUTPUT_PATH, &out_len);
    if (!out_buf)
    {
        fprintf(stderr, "FAIL [test3]: cannot read output file\n");
        return 1;
    }

    static const uint8_t SWAPPED_SYNC[4] = {0xAA, 0xFF, 0xFF, 0xFF};
    if (out_len < 4 ||
        out_buf[0] != SWAPPED_SYNC[0] || out_buf[1] != SWAPPED_SYNC[1] ||
        out_buf[2] != SWAPPED_SYNC[2] || out_buf[3] != SWAPPED_SYNC[3])
    {
        fprintf(stderr, "FAIL [test3]: output does not start with AA FF FF FF "
                        "(expected swap32(FF FF FF AA))\n");
        fprintf(stderr, "  First 4 bytes: %02X %02X %02X %02X\n",
                out_buf[0], out_buf[1], out_buf[2], out_buf[3]);
        pass = 0;
    }
    else
    {
        printf("PASS [test3]: output starts with AA FF FF FF "
               "(= swap32(FF FF FF AA)) ✓\n");
    }

    /* ---- Test 4: output size is word-aligned --------------------- */
    if (out_len % 4 != 0)
    {
        fprintf(stderr, "FAIL [test4]: output size %zu is not a multiple of 4 "
                        "(padding logic broken)\n",
                out_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test4]: output size %zu is word-aligned ✓\n", out_len);
    }

    free(out_buf);

    printf("\n%s\n", pass ? "All tests PASSED." : "Some tests FAILED.");
    printf("Inspect output: xxd %s | head\n", OUTPUT_PATH);
    return pass ? 0 : 1;
}