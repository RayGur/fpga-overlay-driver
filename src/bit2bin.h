/*
 * bit2bin.h — Xilinx .bit → Linux FPGA Manager .bin conversion
 */

#ifndef BIT2BIN_H
#define BIT2BIN_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h> /* ssize_t */

/*
 * find_sync_offset()
 *   Scan buf[0..len-1] for the .bit sync word FF FF FF AA.
 *   Returns byte offset of sync word, or -1 if not found.
 */
ssize_t find_sync_offset(const uint8_t *buf, size_t len);

/*
 * convert_bit_to_bin()
 *   Read input_path (.bit), strip header, byte-swap payload, write output_path (.bin).
 *   Returns 0 on success, -1 on error.
 *
 *   ⚠️  Overwrites output_path without prompting.
 */
int convert_bit_to_bin(const char *input_path, const char *output_path);

#endif /* BIT2BIN_H */
