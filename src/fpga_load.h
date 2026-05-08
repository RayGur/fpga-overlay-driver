#ifndef FPGA_LOAD_H
#define FPGA_LOAD_H

/**
 * fpga_load.h — Linux FPGA Manager sysfs interface
 *
 * 載入流程：
 *   1. cp <bin_file> /lib/firmware/<name>.bin
 *   2. echo 0 > /sys/class/fpga_manager/fpga0/flags   (full bitstream)
 *   3. echo <name>.bin > /sys/class/fpga_manager/fpga0/firmware
 *   4. poll /sys/class/fpga_manager/fpga0/state until "operating"
 */

/**
 * fpga_load_bitstream()
 *   複製 .bin 到 /lib/firmware 並觸發 FPGA Manager 載入
 *
 * @param bin_path  本地 .bin 檔路徑
 * @return          0 on success, -1 on error
 */
int fpga_load_bitstream(const char *bin_path);

/**
 * fpga_get_state()
 *   讀取目前 FPGA Manager state 字串
 *
 * @param buf     輸出 buffer
 * @param buflen  buffer 大小
 * @return        0 on success, -1 on error
 */
int fpga_get_state(char *buf, int buflen);

#endif /* FPGA_LOAD_H */
