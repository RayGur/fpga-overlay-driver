#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bit2bin.h"
#include "hwh_parser.h"
#include "fpga_load.h"
#include "mmio.h"
#include "../include/config.h"

/* ─────────────────────────────────────────────
 * Usage
 * ───────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s load  <design.bit> <design.hwh>  — convert & load bitstream\n"
        "  %s list  <design.hwh>                — list IP cores and addresses\n"
        "  %s read  <design.hwh> <ip_name> <offset_hex>\n"
        "  %s write <design.hwh> <ip_name> <offset_hex> <value_hex>\n"
        "\nExamples:\n"
        "  %s load  design.bit design.hwh\n"
        "  %s list  design.hwh\n"
        "  %s read  design.hwh axi_gpio_0 0x00\n"
        "  %s write design.hwh axi_gpio_0 0x00 0xFF\n",
        prog, prog, prog, prog, prog, prog, prog, prog);
}

/* ─────────────────────────────────────────────
 * Command handlers (Step 6: 填入實作)
 * ───────────────────────────────────────────── */

static int cmd_load(const char *bit_path, const char *hwh_path)
{
    char bin_path[MAX_BIN_FILENAME];

    /* derive .bin path: replace .bit extension */
    snprintf(bin_path, sizeof(bin_path), "%s", bit_path);
    char *dot = strrchr(bin_path, '.');
    if (dot) strcpy(dot, ".bin");
    else      strncat(bin_path, ".bin", sizeof(bin_path) - strlen(bin_path) - 1);

    /* step 1: convert .bit → .bin */
    if (convert_bit_to_bin(bit_path, bin_path) != 0) {
        fprintf(stderr, "[✗] bit2bin failed\n");
        return -1;
    }
    printf("[✓] Converted %s → %s\n", bit_path, bin_path);

    /* step 2: parse .hwh */
    ip_map_t map;
    if (parse_hwh(hwh_path, &map) != 0) {
        fprintf(stderr, "[✗] hwh parse failed\n");
        return -1;
    }
    printf("[✓] Parsed %d IP cores from %s\n", map.count, hwh_path);

    /* step 3: load bitstream */
    if (fpga_load_bitstream(bin_path) != 0) {
        fprintf(stderr, "[✗] FPGA load failed\n");
        return -1;
    }

    char state[64];
    fpga_get_state(state, sizeof(state));
    printf("[✓] Bitstream loaded, FPGA state: %s\n", state);

    return 0;
}

static int cmd_list(const char *hwh_path)
{
    ip_map_t map;
    if (parse_hwh(hwh_path, &map) != 0) {
        fprintf(stderr, "[✗] hwh parse failed\n");
        return -1;
    }
    print_ip_map(&map);
    return 0;
}

static int cmd_read(const char *hwh_path, const char *ip_name, const char *offset_str)
{
    ip_map_t map;
    if (parse_hwh(hwh_path, &map) != 0) return -1;

    const ip_core_t *ip = find_ip_by_name(&map, ip_name);
    if (!ip) {
        fprintf(stderr, "[✗] IP core '%s' not found\n", ip_name);
        return -1;
    }

    uint32_t offset = (uint32_t)strtoul(offset_str, NULL, 16);
    size_t size = (size_t)(ip->high_addr - ip->base_addr + 1);

    mmio_region_t *region = mmio_open(ip->base_addr, size);
    if (!region) return -1;

    uint32_t val = mmio_read32(region, offset);
    printf("0x%08X\n", val);

    mmio_close(region);
    return 0;
}

static int cmd_write(const char *hwh_path, const char *ip_name,
                     const char *offset_str, const char *value_str)
{
    ip_map_t map;
    if (parse_hwh(hwh_path, &map) != 0) return -1;

    const ip_core_t *ip = find_ip_by_name(&map, ip_name);
    if (!ip) {
        fprintf(stderr, "[✗] IP core '%s' not found\n", ip_name);
        return -1;
    }

    uint32_t offset = (uint32_t)strtoul(offset_str, NULL, 16);
    uint32_t value  = (uint32_t)strtoul(value_str,  NULL, 16);
    size_t   size   = (size_t)(ip->high_addr - ip->base_addr + 1);

    mmio_region_t *region = mmio_open(ip->base_addr, size);
    if (!region) return -1;

    mmio_write32(region, offset, value);
    printf("[✓] Wrote 0x%08X to %s offset 0x%X\n", value, ip_name, offset);

    mmio_close(region);
    return 0;
}

/* ─────────────────────────────────────────────
 * Entry point
 * ───────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) { print_usage(argv[0]); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "load") == 0 && argc == 4)
        return cmd_load(argv[2], argv[3]);

    if (strcmp(cmd, "list") == 0 && argc == 3)
        return cmd_list(argv[2]);

    if (strcmp(cmd, "read") == 0 && argc == 5)
        return cmd_read(argv[2], argv[3], argv[4]);

    if (strcmp(cmd, "write") == 0 && argc == 6)
        return cmd_write(argv[2], argv[3], argv[4], argv[5]);

    print_usage(argv[0]);
    return 1;
}
