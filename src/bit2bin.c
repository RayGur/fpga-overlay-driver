/*
 * bit2bin.c — Strip .bit header and byte-swap bitstream to .bin
 *
 * .bit file structure (Xilinx TLV format):
 *
 *   [2 bytes]  initial field length (big-endian short)
 *   [n bytes]  initial field data
 *   [2 bytes]  unknown field (usually 0x0001)
 *   [repeated TLV fields]:
 *     [1 byte]   tag
 *     [2 bytes]  length (big-endian short)
 *     [n bytes]  data (ASCII string)
 *   [terminal field]:
 *     [1 byte]   tag = 0x65
 *     [4 bytes]  payload length (big-endian int, always multiple of 4)
 *     [n bytes]  bitstream payload (big-endian 32-bit words)
 *
 * The 0x65 tag is the key: Vivado writes the exact payload byte count
 * as a 4-byte big-endian integer, guaranteed to be word-aligned.
 * This is the authoritative boundary — no sync word scanning needed.
 *
 * Conversion:
 *   1. Parse TLV header until tag 0x65 is found
 *   2. Read 4-byte payload length → validate against file size
 *   3. swap32() every word in the payload → write .bin
 *
 * Sync word note:
 *   The real Xilinx sync word is 0xAA995566 (stored big-endian in .bit).
 *   swap32(0xAA995566) = 0x6655 99AA — this is what FPGA Manager sees
 *   as a little-endian uint32 on ARM, and it equals 0xAA995566. ✓
 *   All words go through the same swap; no special-casing needed.
 */

#include "bit2bin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static uint32_t swap32(uint32_t x)
{
    return ((x & 0xFF000000u) >> 24) |
           ((x & 0x00FF0000u) >> 8) |
           ((x & 0x0000FF00u) << 8) |
           ((x & 0x000000FFu) << 24);
}

static long file_size(FILE *fp)
{
    long cur = ftell(fp);
    if (cur < 0)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0)
        return -1;
    long size = ftell(fp);
    if (fseek(fp, cur, SEEK_SET) != 0)
        return -1;
    return size;
}

/*
 * read_be16() — read a 2-byte big-endian unsigned short from buf[off].
 * Returns -1 if out of bounds.
 */
static int read_be16(const uint8_t *buf, size_t len, size_t off)
{
    if (off + 2 > len)
        return -1;
    return (buf[off] << 8) | buf[off + 1];
}

/*
 * read_be32() — read a 4-byte big-endian unsigned int from buf[off].
 * Returns -1 if out of bounds.
 */
static long read_be32(const uint8_t *buf, size_t len, size_t off)
{
    if (off + 4 > len)
        return -1;
    return ((long)buf[off] << 24) |
           ((long)buf[off + 1] << 16) |
           ((long)buf[off + 2] << 8) |
           ((long)buf[off + 3]);
}

/* ------------------------------------------------------------------ */
/* TLV header parser                                                    */
/* ------------------------------------------------------------------ */

/*
 * find_payload_offset() — parse the .bit TLV header and return the
 * offset and length of the bitstream payload.
 *
 * Follows the same logic as PYNQ's parse_bit_header():
 *   - Skip the initial 2+n byte field and the 2-byte unknown field
 *   - Walk TLV fields (1-byte tag, 2-byte length, n-byte data)
 *   - Stop at tag 0x65, read 4-byte payload length
 *
 * Parameters:
 *   buf         — full file contents
 *   file_len    — total file size in bytes
 *   out_offset  — set to byte offset of payload start
 *   out_length  — set to payload length in bytes
 *
 * Returns 0 on success, -1 on parse error.
 */
static int find_payload_offset(const uint8_t *buf, size_t file_len,
                               size_t *out_offset, size_t *out_length)
{
    size_t off = 0;

    /* --- skip initial field: 2-byte length + data --- */
    int init_len = read_be16(buf, file_len, off);
    if (init_len < 0)
    {
        fprintf(stderr, "[bit2bin] File too short for initial field\n");
        return -1;
    }
    off += 2 + (size_t)init_len;

    /* --- skip 2-byte unknown field --- */
    off += 2;

    /* --- walk TLV fields until tag 0x65 --- */
    while (off < file_len)
    {
        if (off + 1 > file_len)
        {
            fprintf(stderr, "[bit2bin] Unexpected end of file at tag byte\n");
            return -1;
        }
        uint8_t tag = buf[off++];

        if (tag == 0x65)
        {
            /* Terminal field: 4-byte payload length */
            long payload_len = read_be32(buf, file_len, off);
            if (payload_len < 0)
            {
                fprintf(stderr, "[bit2bin] File too short for 0x65 length field\n");
                return -1;
            }
            off += 4; /* advance past the 4-byte length field */

            /* Validate: payload must fit exactly to end of file */
            if (off + (size_t)payload_len != file_len)
            {
                fprintf(stderr,
                        "[bit2bin] Length mismatch: 0x65 says %ld bytes, "
                        "but file has %zu bytes after header\n",
                        payload_len, file_len - off);
                return -1;
            }

            /* Validate: Vivado always writes word-aligned payloads */
            if ((size_t)payload_len % 4 != 0)
            {
                fprintf(stderr,
                        "[bit2bin] Payload length %ld is not a multiple of 4. "
                        "File may be corrupt.\n",
                        payload_len);
                return -1;
            }

            *out_offset = off;
            *out_length = (size_t)payload_len;
            return 0; /* success */
        }

        /* Non-terminal field: 2-byte length + data, skip it */
        int field_len = read_be16(buf, file_len, off);
        if (field_len < 0)
        {
            fprintf(stderr, "[bit2bin] File too short for tag 0x%02X length\n", tag);
            return -1;
        }
        off += 2 + (size_t)field_len;
    }

    fprintf(stderr, "[bit2bin] Tag 0x65 (payload) not found. "
                    "Is this a valid Xilinx .bit file?\n");
    return -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/*
 * find_sync_offset() — legacy helper, kept for test compatibility.
 *
 * Scans for FF FF FF AA (the pre-sync NOP sequence in .bit format).
 * Note: the real Xilinx sync word is AA 99 55 66. The FF FF FF AA
 * sequence is bus-width detection / NOP preamble, not the config sync.
 * For production use, prefer find_payload_offset() via convert_bit_to_bin().
 */
ssize_t find_sync_offset(const uint8_t *buf, size_t len)
{
    if (len < 4)
        return -1;

    static const uint8_t SYNC[4] = {0xFF, 0xFF, 0xFF, 0xAA};

    for (size_t i = 0; i <= len - 4; i++)
    {
        if (buf[i] == SYNC[0] &&
            buf[i + 1] == SYNC[1] &&
            buf[i + 2] == SYNC[2] &&
            buf[i + 3] == SYNC[3])
        {
            return (ssize_t)i;
        }
    }
    return -1;
}

/*
 * convert_bit_to_bin() — convert a Vivado .bit file to FPGA Manager .bin.
 *
 * Returns 0 on success, -1 on error.
 * WARNING: output_path is overwritten without prompting.
 */
int convert_bit_to_bin(const char *input_path, const char *output_path)
{
    int ret = -1;
    uint8_t *buf = NULL;
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;

    /* ---- 1. Read entire .bit file into memory ---------------------- */
    fp_in = fopen(input_path, "rb");
    if (!fp_in)
    {
        fprintf(stderr, "[bit2bin] Cannot open '%s': %s\n",
                input_path, strerror(errno));
        goto cleanup;
    }

    long fsize = file_size(fp_in);
    if (fsize <= 0)
    {
        fprintf(stderr, "[bit2bin] Cannot get file size: %s\n", input_path);
        goto cleanup;
    }

    buf = malloc((size_t)fsize);
    if (!buf)
    {
        fprintf(stderr, "[bit2bin] Out of memory (%ld bytes)\n", fsize);
        goto cleanup;
    }

    if (fread(buf, 1, (size_t)fsize, fp_in) != (size_t)fsize)
    {
        fprintf(stderr, "[bit2bin] Read error: %s\n", strerror(errno));
        goto cleanup;
    }
    fclose(fp_in);
    fp_in = NULL;

    /* ---- 2. Parse TLV header to find payload ----------------------- */
    size_t payload_off = 0;
    size_t payload_len = 0;

    if (find_payload_offset(buf, (size_t)fsize, &payload_off, &payload_len) != 0)
        goto cleanup;

    fprintf(stderr, "[bit2bin] Header: %zu bytes, payload: %zu bytes "
                    "(offset 0x%zX)\n",
            payload_off, payload_len, payload_off);

    /* ---- 3. Open output -------------------------------------------- */
    fp_out = fopen(output_path, "wb");
    if (!fp_out)
    {
        fprintf(stderr, "[bit2bin] Cannot open output '%s': %s\n",
                output_path, strerror(errno));
        goto cleanup;
    }

    /* ---- 4. Write byte-swapped words ------------------------------- */
    const uint8_t *payload = buf + payload_off;
    size_t words = payload_len / 4;

    for (size_t i = 0; i < words; i++)
    {
        uint32_t word;
        memcpy(&word, payload + i * 4, sizeof(word));
        uint32_t swapped = swap32(word);
        if (fwrite(&swapped, sizeof(swapped), 1, fp_out) != 1)
        {
            fprintf(stderr, "[bit2bin] Write error: %s\n", strerror(errno));
            goto cleanup;
        }
    }

    fprintf(stderr, "[bit2bin] Wrote %zu bytes to '%s'\n",
            payload_len, output_path);

    ret = 0;

cleanup:
    if (fp_in)
        fclose(fp_in);
    if (fp_out)
        fclose(fp_out);
    free(buf);
    return ret;
}