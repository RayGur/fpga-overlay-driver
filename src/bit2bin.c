#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "bit2bin.h"
#include "../include/config.h"

/* ─────────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────────── */

/**
 * swap32() — reverse byte order within a 32-bit word
 * e.g. 0xAABBCCDD → 0xDDCCBBAA
 */
static inline uint32_t swap32(uint32_t x)
{
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) <<  8) |
           ((x & 0x00FF0000U) >>  8) |
           ((x & 0xFF000000U) >> 24);
}

/**
 * find_sync_offset() — scan buffer for the .bit sync word (0xFFFFFFAA)
 *
 * @param buf   raw bytes read from .bit file
 * @param len   number of bytes in buf
 * @return      byte offset of sync word, or -1 if not found
 */
static int find_sync_offset(const uint8_t *buf, size_t len)
{
    /* TODO: Step 2 實作
     *
     * 掃描 buf[0..len-4]，找到第一個符合 sync word 的位置。
     * 注意：.bit 檔的 sync word 以 big-endian 儲存，即
     *       bytes: FF FF FF AA
     */
    (void)buf;
    (void)len;
    return -1;
}

/* ─────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────── */

int convert_bit_to_bin(const char *bit_path, const char *bin_path)
{
    /* TODO: Step 2 實作
     *
     * 1. fopen bit_path, 讀入全部內容到 buffer
     * 2. 呼叫 find_sync_offset() 取得 payload 起點
     * 3. 對 payload 每 4 bytes 呼叫 swap32()
     * 4. 寫出到 bin_path
     * 5. 錯誤處理：檔案不存在、找不到 sync word、寫入失敗等
     *
     * 回傳 0 on success, -1 on error
     */
    (void)bit_path;
    (void)bin_path;
    fprintf(stderr, "[bit2bin] not yet implemented\n");
    return -1;
}
