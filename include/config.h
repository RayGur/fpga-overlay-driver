#ifndef FPGA_OVERLAY_CONFIG_H
#define FPGA_OVERLAY_CONFIG_H

#include <stdint.h>

/* ─────────────────────────────────────────────
 * Platform selection
 *   Define ZYNQ7000 for PYNQ-Z2 (XC7Z020)
 *   Leave undefined for Ultrascale+ (KV260 XCK26)
 * ───────────────────────────────────────────── */
#ifdef ZYNQ7000
    typedef uint32_t fpga_addr_t;
    #define FPGA_ADDR_FMT   "0x%08X"
#else
    typedef uint64_t fpga_addr_t;
    #define FPGA_ADDR_FMT   "0x%016lX"
#endif

/* ─────────────────────────────────────────────
 * Linux FPGA Manager sysfs paths
 * ───────────────────────────────────────────── */
#define FPGA_MANAGER_FLAGS      "/sys/class/fpga_manager/fpga0/flags"
#define FPGA_MANAGER_FIRMWARE   "/sys/class/fpga_manager/fpga0/firmware"
#define FPGA_MANAGER_STATE      "/sys/class/fpga_manager/fpga0/state"
#define FIRMWARE_DIR            "/lib/firmware"

/* ─────────────────────────────────────────────
 * Bitstream constants
 * ───────────────────────────────────────────── */
#define BIT_SYNC_WORD           0xFFFFFFAAU   /* sync word to find bitstream start */
#define BIT_HEADER_MAX          512           /* max bytes to scan for sync word   */

/* ─────────────────────────────────────────────
 * MMIO
 * ───────────────────────────────────────────── */
#define DEVMEM_PATH             "/dev/mem"
#define PAGE_SIZE               4096

/* ─────────────────────────────────────────────
 * Misc
 * ───────────────────────────────────────────── */
#define MAX_IP_CORES            64
#define IP_NAME_LEN             128
#define MAX_BIN_FILENAME        256

#endif /* FPGA_OVERLAY_CONFIG_H */
