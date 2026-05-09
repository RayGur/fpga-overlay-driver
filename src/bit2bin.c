/*
 * bit2bin.c — Strip .bit header and byte-swap bitstream to .bin
 *
 * Vivado .bit format (big-endian):
 *   - Variable-length header with TLV fields (design name, part, date, etc.)
 *   - Sync word: FF FF FF AA  ← marks start of bitstream payload
 *   - Payload: raw bitstream, each 32-bit word stored big-endian
 *
 * Linux FPGA Manager requires:
 *   - No header (payload only)
 *   - Little-endian 32-bit words
 *
 * So we: find sync word → copy from there → swap every 4 bytes.
 *
 * The sync word itself is part of the bitstream and must be included
 * in the output (FPGA Manager needs it to lock onto the config stream).
 */

#include "bit2bin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * swap32() — reverse byte order of a 32-bit word.
 * e.g. 0xAABBCCDD → 0xDDCCBBAA
 */
static uint32_t swap32(uint32_t x)
{
    return ((x & 0xFF000000u) >> 24) |
           ((x & 0x00FF0000u) >> 8) |
           ((x & 0x0000FF00u) << 8) |
           ((x & 0x000000FFu) << 24);
}

/*
 * file_size() — return size of an open file, restore position.
 * Returns -1 on error.
 */
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

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/*
 * find_sync_offset() — scan buffer for the .bit sync word FF FF FF AA.
 *
 * The sync word marks the first byte of the FPGA configuration stream.
 * Everything before it is the Xilinx header (human-readable metadata).
 *
 * Parameters:
 *   buf  — pointer to file contents loaded into memory
 *   len  — total buffer length in bytes
 *
 * Returns:
 *   byte offset of the first 0xFF in the sync word sequence, or
 *   -1 if sync word not found (not a valid .bit file).
 *
 * Note: The header itself may contain 0xFF bytes (e.g. in length fields),
 * so we must find exactly the 4-byte sequence FF FF FF AA, not just 0xFF.
 * We scan until len-4 to avoid a partial match at the end.
 */
ssize_t find_sync_offset(const uint8_t *buf, size_t len)
{
    /* Need at least 4 bytes for a match */
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
 * convert_bit_to_bin() — read a .bit file, strip header, byte-swap, write .bin.
 *
 * Algorithm:
 *   1. Read entire .bit file into memory.
 *   2. Call find_sync_offset() to locate the payload start.
 *   3. Check payload length is a multiple of 4 (required for word swap).
 *      If not, the last partial word is written unswapped with a warning —
 *      this should not happen with a well-formed .bit file.
 *   4. For each 4-byte word in the payload: apply swap32() then write.
 *   5. Write result to output_path.
 *
 * Parameters:
 *   input_path  — path to source .bit file
 *   output_path — path to destination .bin file (created/overwritten)
 *
 * Returns:
 *   0  on success
 *   -1 on any error (message printed to stderr)
 *
 * ⚠️  WARNING: output_path will be overwritten without prompting.
 *     Caller should confirm if the file is in /lib/firmware/.
 */
int convert_bit_to_bin(const char *input_path, const char *output_path)
{
    int ret = -1;
    uint8_t *buf = NULL;
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;

    /* ---- 1. Open and read input file -------------------------------- */
    fp_in = fopen(input_path, "rb");
    if (!fp_in)
    {
        fprintf(stderr, "[bit2bin] Cannot open input '%s': %s\n",
                input_path, strerror(errno));
        goto cleanup;
    }

    long fsize = file_size(fp_in);
    if (fsize <= 0)
    {
        fprintf(stderr, "[bit2bin] Cannot determine file size or file is empty: %s\n",
                input_path);
        goto cleanup;
    }

    buf = (uint8_t *)malloc((size_t)fsize);
    if (!buf)
    {
        fprintf(stderr, "[bit2bin] Out of memory allocating %ld bytes\n", fsize);
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

    /* ---- 2. Find sync word ----------------------------------------- */
    ssize_t sync_off = find_sync_offset(buf, (size_t)fsize);
    if (sync_off < 0)
    {
        fprintf(stderr, "[bit2bin] Sync word (FF FF FF AA) not found in '%s'.\n"
                        "          Is this a valid Xilinx .bit file?\n",
                input_path);
        goto cleanup;
    }

    const uint8_t *payload = buf + sync_off;
    size_t payload_len = (size_t)fsize - (size_t)sync_off;

    fprintf(stderr, "[bit2bin] Header: %zd bytes, payload: %zu bytes\n",
            (size_t)sync_off, payload_len);

    if (payload_len == 0)
    {
        fprintf(stderr, "[bit2bin] Payload is empty after sync word.\n");
        goto cleanup;
    }

    /* ---- 3. Warn if payload is not word-aligned -------------------- */
    if (payload_len % 4 != 0)
    {
        fprintf(stderr, "[bit2bin] WARNING: payload length %zu is not a "
                        "multiple of 4. File may be truncated or corrupt.\n",
                payload_len);
    }

    /* ---- 4. Open output and write byte-swapped words --------------- */
    fp_out = fopen(output_path, "wb");
    if (!fp_out)
    {
        fprintf(stderr, "[bit2bin] Cannot open output '%s': %s\n",
                output_path, strerror(errno));
        goto cleanup;
    }

    size_t words = payload_len / 4;
    size_t remainder = payload_len % 4;

    for (size_t i = 0; i < words; i++)
    {
        /* Read 4 bytes as big-endian word */
        uint32_t word;
        memcpy(&word, payload + i * 4, sizeof(word));

        /* Swap to little-endian */
        uint32_t swapped = swap32(word);

        if (fwrite(&swapped, sizeof(swapped), 1, fp_out) != 1)
        {
            fprintf(stderr, "[bit2bin] Write error on '%s': %s\n",
                    output_path, strerror(errno));
            goto cleanup;
        }
    }

    /* Handle trailing partial word (should not happen with valid .bit) */
    if (remainder > 0)
    {
        fprintf(stderr, "[bit2bin] WARNING: writing %zu trailing byte(s) unswapped.\n",
                remainder);
        if (fwrite(payload + words * 4, 1, remainder, fp_out) != remainder)
        {
            fprintf(stderr, "[bit2bin] Write error (trailing bytes) on '%s': %s\n",
                    output_path, strerror(errno));
            goto cleanup;
        }
    }

    fprintf(stderr, "[bit2bin] Wrote %zu bytes to '%s'\n",
            payload_len, output_path);

    ret = 0; /* success */

cleanup:
    if (fp_in)
        fclose(fp_in);
    if (fp_out)
        fclose(fp_out);
    free(buf);
    return ret;
}
