/*
 * bit2bin.h — Xilinx .bit → Linux FPGA Manager .bin conversion
 */

#ifndef BIT2BIN_H
#define BIT2BIN_H

#include <stdint.h>
#include <stddef.h>

/*
 * bit_find_payload()
 *   Parse the TLV header of a .bit file in memory and return the
 *   byte offset and length of the bitstream payload (tag 0x65 field).
 *
 *   buf        — pointer to full file contents
 *   file_len   — total file size in bytes
 *   out_offset — set to payload start offset on success
 *   out_length — set to payload length in bytes on success
 *
 *   Returns 0 on success, -1 on error.
 */
int bit_find_payload(const uint8_t *buf, size_t file_len,
                     size_t *out_offset, size_t *out_length);

/*
 * convert_bit_to_bin()
 *   Read input_path (.bit), strip TLV header, byte-swap payload,
 *   write output_path (.bin).
 *
 *   Returns 0 on success, -1 on error.
 *   WARNING: output_path is overwritten without prompting.
 */
int convert_bit_to_bin(const char *input_path, const char *output_path);

#endif /* BIT2BIN_H */