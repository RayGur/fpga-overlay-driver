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
 * The 0x65 tag length field is authoritative: Vivado writes the exact
 * byte count of the bitstream payload, guaranteed word-aligned. No sync
 * word scanning is needed to locate the payload boundary.
 *
 * Sync word note:
 *   The real Xilinx sync word is 0xAA995566 (stored big-endian in .bit
 *   as bytes AA 99 55 66). After swap32 it becomes 0x665599AA, stored
 *   in .bin as bytes 66 55 99 AA. ARM reads those 4 bytes as little-endian
 *   uint32 = 0xAA995566. All words go through the same swap; sync word
 *   needs no special treatment.
 */

#include "bit2bin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

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

/*
 * file_size() — return size of a regular file via ftell/fseek.
 * POSIX guarantees this works on regular files opened in binary mode.
 *
 * Side-effect note: if the final fseek(SEEK_SET) fails, the file position
 * is left at EOF and -1 is returned. The only caller (convert_bit_to_bin)
 * immediately follows with fread(), which will read 0 bytes and trigger
 * its own error check — so this side effect is harmless in practice.
 *
 * Returns file size on success, -1 on error.
 */
static long file_size(FILE *fp)
{
    long cur = ftell(fp);
    if (cur < 0)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0)
        return -1;
    long size = ftell(fp);
    if (size < 0)
        return -1; /* ftell at SEEK_END failed */
    if (fseek(fp, cur, SEEK_SET) != 0)
        return -1;
    return size;
}

/*
 * read_be16() — read a 2-byte big-endian value from buf[off].
 * Returns -1 if out of bounds.
 */
static int read_be16(const uint8_t *buf, size_t len, size_t off)
{
    if (off + 2 > len)
        return -1;
    return ((unsigned)buf[off] << 8) | buf[off + 1];
}

/*
 * read_be32() — read a 4-byte big-endian uint32 from buf[off].
 *
 * Uses output parameter to avoid ambiguity between a legitimate value
 * that happens to be 0xFFFFFFFF and the error sentinel -1.
 * (A payload length with high bit set would have caused a false error
 * if returned as a signed long.)
 *
 * Returns 0 on success, -1 if out of bounds.
 */
static int read_be32(const uint8_t *buf, size_t len, size_t off,
                     uint32_t *out)
{
    if (off + 4 > len)
        return -1;
    *out = ((uint32_t)buf[off] << 24) |
           ((uint32_t)buf[off + 1] << 16) |
           ((uint32_t)buf[off + 2] << 8) |
           ((uint32_t)buf[off + 3]);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/*
 * bit_find_payload() — parse the .bit TLV header and return the
 * offset and length of the bitstream payload.
 *
 * Implements the same header-walking logic as PYNQ's parse_bit_header():
 *   1. Skip the initial 2+n byte field and the 2-byte unknown field.
 *   2. Walk TLV fields: 1-byte tag, 2-byte length, n-byte data.
 *   3. On tag 0x65: read 4-byte payload length, validate, return.
 *
 * Parameters:
 *   buf         — full file contents
 *   file_len    — total file size in bytes
 *   out_offset  — set to byte offset of payload start on success
 *   out_length  — set to payload length in bytes on success
 *
 * Returns 0 on success, -1 on parse error (message printed to stderr).
 */
int bit_find_payload(const uint8_t *buf, size_t file_len,
                     size_t *out_offset, size_t *out_length)
{
    size_t off = 0;

    /* Skip initial field: 2-byte length + data */
    int init_len = read_be16(buf, file_len, off);
    if (init_len < 0)
    {
        fprintf(stderr, "[bit2bin] File too short for initial field\n");
        return -1;
    }
    off += 2 + (size_t)init_len;

    /* Skip 2-byte unknown field */
    if (off + 2 > file_len)
    {
        fprintf(stderr, "[bit2bin] File too short for unknown field\n");
        return -1;
    }
    off += 2;

    /* Walk TLV fields until tag 0x65 */
    while (off < file_len)
    {
        uint8_t tag = buf[off++];

        if (tag == 0x65)
        {
            /* 4-byte big-endian payload length */
            uint32_t payload_len = 0;
            if (read_be32(buf, file_len, off, &payload_len) != 0)
            {
                fprintf(stderr, "[bit2bin] File too short for 0x65 length field\n");
                return -1;
            }
            off += 4;

            /* Validate: payload fills the file exactly */
            if (off + (size_t)payload_len != file_len)
            {
                fprintf(stderr,
                        "[bit2bin] Length mismatch: 0x65 says %" PRIu32 " bytes payload, "
                        "but %zu bytes remain in file\n",
                        payload_len, file_len - off);
                return -1;
            }

            /* Validate: Vivado guarantees word-aligned payload */
            if (payload_len % 4 != 0)
            {
                fprintf(stderr,
                        "[bit2bin] Payload length %" PRIu32 " is not a multiple of 4 "
                        "(file may be corrupt)\n",
                        payload_len);
                return -1;
            }

            *out_offset = off;
            *out_length = (size_t)payload_len;
            return 0;
        }

        /* Non-terminal field: skip 2-byte length + data */
        int field_len = read_be16(buf, file_len, off);
        if (field_len < 0)
        {
            fprintf(stderr, "[bit2bin] File too short for tag 0x%02X length\n", tag);
            return -1;
        }
        off += 2 + (size_t)field_len;
    }

    fprintf(stderr, "[bit2bin] Tag 0x65 not found — is this a valid Xilinx .bit file?\n");
    return -1;
}

/*
 * convert_bit_to_bin() — convert a Vivado .bit file to FPGA Manager .bin.
 *
 * Reads input_path, strips the TLV header, byte-swaps every 32-bit word
 * in the payload, and writes the result to output_path.
 *
 * Returns 0 on success, -1 on error (message printed to stderr).
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
        fprintf(stderr, "[bit2bin] Cannot determine file size of '%s'\n",
                input_path);
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
        fprintf(stderr, "[bit2bin] Read error on '%s': %s\n",
                input_path, strerror(errno));
        goto cleanup;
    }
    fclose(fp_in);
    fp_in = NULL;

    /* ---- 2. Parse TLV header to locate payload --------------------- */
    size_t payload_off = 0;
    size_t payload_len = 0;

    if (bit_find_payload(buf, (size_t)fsize, &payload_off, &payload_len) != 0)
        goto cleanup;

    fprintf(stderr, "[bit2bin] Header: %zu bytes, payload: %zu bytes\n",
            payload_off, payload_len);

    /* ---- 3. Open output -------------------------------------------- */
    fp_out = fopen(output_path, "wb");
    if (!fp_out)
    {
        fprintf(stderr, "[bit2bin] Cannot open output '%s': %s\n",
                output_path, strerror(errno));
        goto cleanup;
    }

    /* ---- 4. Write byte-swapped words ------------------------------- */
    /*
     * memcpy() reads the 4 bytes into word without assuming host byte order.
     * swap32() then performs an explicit byte-level reversal regardless of
     * host endianness, making this code correct on both LE and BE hosts.
     * (In practice this targets ARM little-endian, but no assumption is
     * baked in.)
     */
    const uint8_t *payload = buf + payload_off;
    size_t words = payload_len / 4;

    for (size_t i = 0; i < words; i++)
    {
        uint32_t word;
        memcpy(&word, payload + i * 4, sizeof(word));
        uint32_t swapped = swap32(word);
        if (fwrite(&swapped, sizeof(swapped), 1, fp_out) != 1)
        {
            fprintf(stderr, "[bit2bin] Write error on '%s': %s\n",
                    output_path, strerror(errno));
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