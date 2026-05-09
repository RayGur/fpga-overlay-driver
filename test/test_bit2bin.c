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
 * Tests:
 *   1. find_sync_offset()  — still finds FF FF FF AA (preamble NOP sequence)
 *   2. convert_bit_to_bin() — TLV parse succeeds, output written
 *   3. Output size matches TLV 0x65 length field exactly
 *   4. Output size is a multiple of 4
 *   5. Output starts with swap32(AA 99 55 66) = 66 55 99 AA
 *      (real Xilinx sync word, byte-swapped for little-endian ARM)
 *
 * Manual verify:
 *   xxd /tmp/test_output.bin | head
 *   # offset 0x00 should NOT be AA FF FF FF anymore
 *   # look for 66 55 99 AA somewhere in the first few lines
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/bit2bin.h"

#define OUTPUT_PATH "/tmp/test_output.bin"

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

/* Read 4-byte big-endian uint32 from buf[off] */
static uint32_t read_be32(const uint8_t *buf, size_t off)
{
    return ((uint32_t)buf[off] << 24) |
           ((uint32_t)buf[off + 1] << 16) |
           ((uint32_t)buf[off + 2] << 8) |
           ((uint32_t)buf[off + 3]);
}

/*
 * Parse .bit TLV to find the 0x65 payload length field.
 * Returns the payload length, or 0 on failure.
 */
static size_t get_tlv_payload_length(const uint8_t *buf, size_t file_len)
{
    size_t off = 0;

    /* skip initial field */
    if (off + 2 > file_len)
        return 0;
    uint16_t init_len = (buf[off] << 8) | buf[off + 1];
    off += 2 + init_len;

    /* skip unknown field */
    off += 2;

    while (off < file_len)
    {
        if (off + 1 > file_len)
            return 0;
        uint8_t tag = buf[off++];

        if (tag == 0x65)
        {
            if (off + 4 > file_len)
                return 0;
            return (size_t)read_be32(buf, off);
        }

        if (off + 2 > file_len)
            return 0;
        uint16_t flen = (buf[off] << 8) | buf[off + 1];
        off += 2 + flen;
    }
    return 0;
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

    /* ---- load input file ------------------------------------------ */
    size_t in_len = 0;
    uint8_t *in_buf = read_file(input_path, &in_len);
    if (!in_buf)
    {
        fprintf(stderr, "FAIL: could not read input file\n");
        return 1;
    }

    /* ---- Test 1: find_sync_offset finds FF FF FF AA --------------- */
    ssize_t sync = find_sync_offset(in_buf, in_len);
    if (sync < 0)
    {
        fprintf(stderr, "FAIL [test1]: FF FF FF AA preamble not found\n");
        pass = 0;
    }
    else
    {
        printf("PASS [test1]: FF FF FF AA preamble at offset %zd\n", sync);
        printf("  Note: this is the NOP preamble, not the real sync word\n"
               "        Real sync word AA 99 55 66 follows shortly after\n");
    }

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
    free(in_buf);
    if (!pass)
        return 1;

    /* ---- Test 3: output size matches TLV 0x65 length -------------- */
    in_buf = read_file(input_path, &in_len); /* re-read for TLV parse */
    size_t tlv_len = get_tlv_payload_length(in_buf, in_len);
    free(in_buf);

    size_t out_len = 0;
    uint8_t *out_buf = read_file(OUTPUT_PATH, &out_len);
    if (!out_buf)
    {
        fprintf(stderr, "FAIL [test3]: cannot read output file\n");
        return 1;
    }

    if (tlv_len == 0)
    {
        fprintf(stderr, "FAIL [test3]: could not parse TLV 0x65 length\n");
        pass = 0;
    }
    else if (out_len != tlv_len)
    {
        fprintf(stderr, "FAIL [test3]: output size %zu != TLV length %zu\n",
                out_len, tlv_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test3]: output size %zu matches TLV 0x65 length ✓\n",
               out_len);
    }

    /* ---- Test 4: output is word-aligned --------------------------- */
    if (out_len % 4 != 0)
    {
        fprintf(stderr, "FAIL [test4]: output size %zu is not a multiple of 4\n",
                out_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test4]: output size %zu is word-aligned ✓\n", out_len);
    }

    /* ---- Test 5: real sync word appears byte-swapped -------------- */
    /*
     * In .bit (big-endian): AA 99 55 66
     * After swap32:         66 55 99 AA
     * ARM reads as LE u32:  0xAA995566  ← correct sync word ✓
     *
     * The sync word is not at byte 0 of the payload; it's preceded by
     * NOP words (FF FF FF FF → swap → FF FF FF FF) and bus-width
     * detection words. Scan the first 256 bytes for it.
     */
    static const uint8_t SWAPPED_SYNC[4] = {0x66, 0x55, 0x99, 0xAA};
    int found_sync = 0;
    size_t search_len = out_len < 256 ? out_len : 256;
    for (size_t i = 0; i + 4 <= search_len; i += 4)
    {
        if (out_buf[i] == SWAPPED_SYNC[0] &&
            out_buf[i + 1] == SWAPPED_SYNC[1] &&
            out_buf[i + 2] == SWAPPED_SYNC[2] &&
            out_buf[i + 3] == SWAPPED_SYNC[3])
        {
            printf("PASS [test5]: sync word 66 55 99 AA found at output "
                   "offset %zu ✓\n",
                   i);
            printf("  ARM reads as LE uint32 = 0xAA995566 (correct)\n");
            found_sync = 1;
            break;
        }
    }
    if (!found_sync)
    {
        fprintf(stderr, "FAIL [test5]: sync word 66 55 99 AA not found "
                        "in first 256 bytes\n");
        fprintf(stderr, "  First 16 bytes:");
        for (size_t i = 0; i < 16 && i < out_len; i++)
            fprintf(stderr, " %02X", out_buf[i]);
        fprintf(stderr, "\n");
        pass = 0;
    }

    free(out_buf);

    printf("\n%s\n", pass ? "All tests PASSED." : "Some tests FAILED.");
    printf("Inspect output: xxd %s | head -4\n", OUTPUT_PATH);
    return pass ? 0 : 1;
}