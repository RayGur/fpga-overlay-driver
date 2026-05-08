#ifndef BIT2BIN_H
#define BIT2BIN_H

/**
 * bit2bin.h — Vivado .bit → Linux FPGA Manager .bin converter
 *
 * .bit 格式：
 *   [variable-length header] + [bitstream payload]
 *
 * 轉換步驟：
 *   1. 掃描 header，找到 sync word (0xFFFFFFAA) 定位 payload 起點
 *   2. 對 payload 做 32-bit byte-swap（每 4 bytes 內部反轉）
 *   3. 輸出 .bin 供 Linux FPGA Manager 使用
 */

/**
 * convert_bit_to_bin()
 *
 * @param bit_path   輸入的 .bit 檔路徑
 * @param bin_path   輸出的 .bin 檔路徑
 * @return           0 on success, -1 on error
 */
int convert_bit_to_bin(const char *bit_path, const char *bin_path);

#endif /* BIT2BIN_H */
