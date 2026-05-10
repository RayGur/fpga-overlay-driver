#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
    /* 1. 取出 basename */
    const char *base = strrchr(bin_path, '/');
    base = base ? base + 1 : bin_path;

    /* 2. 拼出 dest 路徑 */
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/%s", FIRMWARE_DIR, base);

    /* 3. 路徑相同則跳過複製 */
    if (strcmp(bin_path, dest) == 0) {
        fprintf(stderr, "[fpga_load] source is already in firmware dir, skipping copy\n");
        snprintf(fw_name, fw_name_len, "%s", base);
        return 0;
    }

    fprintf(stderr, "[fpga_load] Copying %s → %s\n", bin_path, dest);

    FILE *src_f = fopen(bin_path, "rb");
    if (!src_f) {
        fprintf(stderr, "[fpga_load] cannot open %s: %s\n", bin_path, strerror(errno));
        return -1;
    }

    FILE *dst_f = fopen(dest, "wb");
    if (!dst_f) {
        fprintf(stderr, "[fpga_load] cannot open %s: %s\n", dest, strerror(errno));
        fclose(src_f);
        return -1;
    }

    uint8_t buf[4096];
    size_t n;
    int ret = 0;
    while ((n = fread(buf, 1, sizeof(buf), src_f)) > 0) {
        if (fwrite(buf, 1, n, dst_f) != n) {
            fprintf(stderr, "[fpga_load] write error to %s: %s\n", dest, strerror(errno));
            ret = -1;
            break;
        }
    }
    if (ferror(src_f)) {
        fprintf(stderr, "[fpga_load] read error from %s: %s\n", bin_path, strerror(errno));
        ret = -1;
    }

    fclose(src_f);
    fclose(dst_f);

    if (ret == 0)
        snprintf(fw_name, fw_name_len, "%s", base);
    return ret;
}

/* ─────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────── */

int fpga_load_bitstream(const char *bin_path)
{
#define POLL_MAX    50       /* 50 × 100ms = 5 seconds */
#define POLL_SLEEP  100000   /* 100ms in microseconds  */

    char fw_name[MAX_BIN_FILENAME];

    /* 1. 複製到 /lib/firmware/ */
    if (copy_to_firmware(bin_path, fw_name, sizeof(fw_name)) < 0)
        return -1;

    /* 2. flags = 0 → full bitstream mode */
    fprintf(stderr, "[fpga_load] Writing flags = 0\n");
    if (write_sysfs(FPGA_MANAGER_FLAGS, "0\n") < 0)
        return -1;

    /* 3. 寫入 firmware 檔名觸發載入 */
    fprintf(stderr, "[fpga_load] Triggering firmware load: %s\n", fw_name);
    if (write_sysfs(FPGA_MANAGER_FIRMWARE, fw_name) < 0)
        return -1;

    /* 4. Poll state 直到 operating 或逾時 */
    char state[64] = {0};
    for (int i = 0; i < POLL_MAX; i++) {
        usleep(POLL_SLEEP);
        if (fpga_get_state(state, sizeof(state)) == 0 &&
            strcmp(state, "operating") == 0) {
            fprintf(stderr, "[fpga_load] State: operating ✓\n");
            return 0;
        }
    }

    fprintf(stderr, "[fpga_load] timeout waiting for 'operating', last state: %s\n", state);
    return -1;

#undef POLL_MAX
#undef POLL_SLEEP
}

int fpga_get_state(char *buf, int buflen)
{
    return read_sysfs(FPGA_MANAGER_STATE, buf, buflen);
}
