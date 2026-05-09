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
 *   1. bit_find_payload() returns a sane offset and word-aligned length
 *   2. convert_bit_to_bin() succeeds and output size matches TLV length
 *   3. Output is word-aligned
 *   4. Real sync word (AA 99 55 66 -> swapped: 66 55 99 AA) is present
 *      in the output within the first 256 bytes
 *
 * Test independence:
 *   Each test re-derives its expected values independently.
 *   Test 2 re-parses the TLV header itself rather than reusing payload_len
 *   from Test 1, so tests can be read and reasoned about in isolation.
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <design.bit>\n", argv[0]);
        return 1;
    }
    const char *input_path = argv[1];
    int pass = 1;

    /* ---- load input ------------------------------------------------ */
    size_t in_len = 0;
    uint8_t *in_buf = read_file(input_path, &in_len);
    if (!in_buf)
    {
        fprintf(stderr, "FAIL: could not read '%s'\n", input_path);
        return 1;
    }

    /* ---- Test 1: bit_find_payload parses header correctly ---------- */
    size_t payload_off = 0, payload_len = 0;
    if (bit_find_payload(in_buf, in_len, &payload_off, &payload_len) != 0)
    {
        fprintf(stderr, "FAIL [test1]: bit_find_payload failed\n");
        pass = 0;
    }
    else if (payload_off == 0 || payload_off >= in_len)
    {
        fprintf(stderr, "FAIL [test1]: payload offset %zu is out of range\n",
                payload_off);
        pass = 0;
    }
    else if (payload_len % 4 != 0)
    {
        fprintf(stderr, "FAIL [test1]: payload length %zu is not word-aligned\n",
                payload_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test1]: payload at offset %zu, length %zu bytes ✓\n",
               payload_off, payload_len);
    }
    free(in_buf);

    /* Early exit: Test 2–4 depend on a readable input file, but not on
     * Test 1's payload_len — each test re-derives what it needs. */
    if (!pass)
        return 1;

    /* ---- Test 2: convert_bit_to_bin output size matches TLV ------- */
    if (convert_bit_to_bin(input_path, OUTPUT_PATH) != 0)
    {
        fprintf(stderr, "FAIL [test2]: convert_bit_to_bin failed\n");
        return 1;
    }

    /* Re-parse TLV independently to get the expected payload length.
     * This avoids a dependency on payload_len set in Test 1. */
    size_t in_len2 = 0;
    uint8_t *in_buf2 = read_file(input_path, &in_len2);
    if (!in_buf2)
    {
        fprintf(stderr, "FAIL [test2]: could not re-read input for TLV check\n");
        return 1;
    }
    size_t expected_off = 0, expected_len = 0;
    int tlv_ok = bit_find_payload(in_buf2, in_len2, &expected_off, &expected_len);
    free(in_buf2);
    if (tlv_ok != 0)
    {
        fprintf(stderr, "FAIL [test2]: TLV re-parse failed\n");
        return 1;
    }

    size_t out_len = 0;
    uint8_t *out_buf = read_file(OUTPUT_PATH, &out_len);
    if (!out_buf)
    {
        fprintf(stderr, "FAIL [test2]: cannot read output file\n");
        return 1;
    }

    if (out_len != expected_len)
    {
        fprintf(stderr, "FAIL [test2]: output size %zu != TLV payload length %zu\n",
                out_len, expected_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test2]: output size %zu matches TLV length ✓\n", out_len);
    }

    /* ---- Test 3: output is word-aligned --------------------------- */
    if (out_len % 4 != 0)
    {
        fprintf(stderr, "FAIL [test3]: output size %zu is not a multiple of 4\n",
                out_len);
        pass = 0;
    }
    else
    {
        printf("PASS [test3]: output is word-aligned ✓\n");
    }

    /* ---- Test 4: swapped sync word present in output -------------- */
    /*
     * In .bit (big-endian stored): AA 99 55 66
     * After swap32:                66 55 99 AA  (bytes in .bin)
     * ARM reads as LE uint32:      0xAA995566   ✓
     *
     * The sync word is preceded by NOP words (FF FF FF FF → FF FF FF FF
     * after swap) and bus-width detection words. All words are
     * word-aligned, so scanning at i += 4 is correct and intentional —
     * the sync word is guaranteed to start on a 4-byte boundary because
     * the payload itself is word-aligned by TLV contract.
     */
    static const uint8_t SWAPPED_SYNC[4] = {0x66, 0x55, 0x99, 0xAA};
    int found = 0;
    size_t scan = out_len < 256 ? out_len : 256;
    for (size_t i = 0; i + 4 <= scan; i += 4)
    {
        if (out_buf[i] == SWAPPED_SYNC[0] && out_buf[i + 1] == SWAPPED_SYNC[1] &&
            out_buf[i + 2] == SWAPPED_SYNC[2] && out_buf[i + 3] == SWAPPED_SYNC[3])
        {
            printf("PASS [test4]: sync word 66 55 99 AA at output offset %zu "
                   "(= 0xAA995566 LE) ✓\n",
                   i);
            found = 1;
            break;
        }
    }
    if (!found)
    {
        fprintf(stderr, "FAIL [test4]: sync word 66 55 99 AA not found in "
                        "first 256 bytes of output\n");
        pass = 0;
    }

    free(out_buf);
    printf("\n%s\n", pass ? "All tests PASSED." : "Some tests FAILED.");
    return pass ? 0 : 1;
}