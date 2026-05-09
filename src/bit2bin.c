/*
 * bit2bin.c — Strip .bit header and byte-swap bitstream to .bin
 *
 * Vivado .bit format:
 *   [TLV header]  variable length, human-readable metadata
 *   [0xFF...0xFF] dummy bytes at header tail (not word-aligned)
 *   [FF FF FF AA] sync word — marks start of FPGA config stream
 *   [payload]     big-endian 32-bit words, word-aligned
 *
 * Linux FPGA Manager requires:
 *   - Little-endian 32-bit words
 *   - Word-aligned length
 *
 * Conversion:
 *   1. Find sync word (FF FF FF AA) → locate payload
 *   2. Round payload start DOWN to 4-byte boundary
 *      (the extra bytes before sync are 0xFF dummies — FPGA ignores them)
 *   3. swap32() every word → write .bin
 *
 * Sync word note:
 *   swap32(0xFFFFFFAA) = 0xAAFFFFFF
 *   ARM reads those 4 bytes as little-endian uint32 = 0xFFFFFFAA ✓
 *   No special treatment needed; sync word goes through the same swap.
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

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/*
 * find_sync_offset() — find the .bit sync word FF FF FF AA in buf.
 *
 * Returns byte offset of the sync word, or -1 if not found.
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

    /* ---- 2. Find sync word ----------------------------------------- */
    ssize_t sync_off = find_sync_offset(buf, (size_t)fsize);
    if (sync_off < 0)
    {
        fprintf(stderr, "[bit2bin] Sync word FF FF FF AA not found in '%s'.\n"
                        "          Is this a valid Xilinx .bit file?\n",
                input_path);
        goto cleanup;
    }

    /* ---- 3. Align payload start to 4-byte boundary ---------------- */
    /*
     * The .bit header length is not word-aligned (e.g. 157 bytes).
     * The bytes immediately before the sync word are always 0xFF dummy
     * bytes — the FPGA config engine discards everything before the sync
     * word, so pulling a few extra 0xFF bytes into the payload is safe.
     * This avoids any need to pad or truncate the trailing end.
     */
    size_t aligned_off = (size_t)sync_off & ~(size_t)3; /* round down */
    const uint8_t *payload = buf + aligned_off;
    size_t payload_len = (size_t)fsize - aligned_off;

    fprintf(stderr, "[bit2bin] Sync at %zu, payload start aligned to %zu "
                    "(%zu bytes)\n",
            (size_t)sync_off, aligned_off, payload_len);

    /* Sanity: after alignment the payload must be word-aligned.
     * If not, the .bit file itself is malformed (truncated mid-word). */
    if (payload_len % 4 != 0)
    {
        fprintf(stderr, "[bit2bin] ERROR: payload %zu bytes is not a multiple "
                        "of 4 after alignment. File may be truncated.\n",
                payload_len);
        goto cleanup;
    }

    /* ---- 4. Open output -------------------------------------------- */
    fp_out = fopen(output_path, "wb");
    if (!fp_out)
    {
        fprintf(stderr, "[bit2bin] Cannot open output '%s': %s\n",
                output_path, strerror(errno));
        goto cleanup;
    }

    /* ---- 5. Write byte-swapped words ------------------------------- */
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