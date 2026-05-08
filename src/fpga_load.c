#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "fpga_load.h"
#include "../include/config.h"

/* ─────────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────────── */

/**
 * write_sysfs() — write a string to a sysfs file
 */
static int write_sysfs(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[fpga_load] cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    int ret = (fputs(value, f) == EOF) ? -1 : 0;
    fclose(f);
    return ret;
}

/**
 * read_sysfs() — read a line from a sysfs file into buf
 */
static int read_sysfs(const char *path, char *buf, int buflen)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[fpga_load] cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    char *ret = fgets(buf, buflen, f);
    fclose(f);
    /* strip trailing newline */
    buf[strcspn(buf, "\n")] = '\0';
    return ret ? 0 : -1;
}

/**
 * copy_to_firmware() — cp bin_path → /lib/firmware/<basename>
 *
 * @param bin_path    source .bin path
 * @param fw_name     output: filename inside /lib/firmware (e.g. "design.bin")
 * @param fw_name_len size of fw_name buffer
 * @return            0 on success, -1 on error
 */
static int copy_to_firmware(const char *bin_path, char *fw_name, int fw_name_len)
{
    /* TODO: Step 3 實作
     *
     * 1. 從 bin_path 取出 basename（最後一段檔名）
     * 2. 拼出 dest = FIRMWARE_DIR "/" basename
     * 3. 用 fopen/fread/fwrite 複製檔案（或 system("cp ...")，後者較簡單但不夠嚴謹）
     * 4. 把 basename 存入 fw_name
     *
     * 回傳 0 on success, -1 on error
     */
    (void)bin_path;
    (void)fw_name;
    (void)fw_name_len;
    return -1;
}

/* ─────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────── */

int fpga_load_bitstream(const char *bin_path)
{
    /* TODO: Step 3 實作
     *
     * 1. copy_to_firmware()
     * 2. write_sysfs(FPGA_MANAGER_FLAGS, "0\n")       → full bitstream mode
     * 3. write_sysfs(FPGA_MANAGER_FIRMWARE, fw_name)  → trigger load
     * 4. poll fpga_get_state() 最多 N 次，sleep 100ms 間隔
     *    直到 state == "operating"，或逾時回傳 -1
     *
     * 回傳 0 on success, -1 on error
     */
    (void)bin_path;
    fprintf(stderr, "[fpga_load] not yet implemented\n");
    return -1;
}

int fpga_get_state(char *buf, int buflen)
{
    return read_sysfs(FPGA_MANAGER_STATE, buf, buflen);
}
