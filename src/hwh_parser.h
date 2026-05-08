#ifndef HWH_PARSER_H
#define HWH_PARSER_H

#include <stddef.h>
#include "../include/config.h"

/**
 * hwh_parser.h — Vivado .hwh (XML) IP Core address map parser
 *
 * .hwh 範例片段：
 *   <MODULE FULLNAME="design_1/axi_gpio_0" VLNV="xilinx.com:ip:axi_gpio:2.0">
 *     <REGISTERS>
 *       <PROPERTY NAME="BASEADDR" VALUE="0x41200000"/>
 *       <PROPERTY NAME="HIGHADDR" VALUE="0x4120FFFF"/>
 *     </REGISTERS>
 *   </MODULE>
 */

/* ─────────────────────────────────────────────
 * Data structures
 * ───────────────────────────────────────────── */

typedef struct {
    char        name[IP_NAME_LEN];   /* e.g. "axi_gpio_0"          */
    fpga_addr_t base_addr;           /* BASEADDR from .hwh          */
    fpga_addr_t high_addr;           /* HIGHADDR from .hwh          */
} ip_core_t;

typedef struct {
    ip_core_t   cores[MAX_IP_CORES];
    int         count;
} ip_map_t;

/* ─────────────────────────────────────────────
 * API
 * ───────────────────────────────────────────── */

/**
 * parse_hwh()
 *   解析 .hwh 檔，填入 ip_map_t
 *
 * @param hwh_path  .hwh 檔路徑
 * @param map       輸出的 IP map（呼叫者分配）
 * @return          0 on success, -1 on error
 */
int parse_hwh(const char *hwh_path, ip_map_t *map);

/**
 * find_ip_by_name()
 *   在 ip_map_t 中查找指定名稱的 IP Core
 *
 * @param map   已解析的 IP map
 * @param name  IP Core 名稱（e.g. "axi_gpio_0"）
 * @return      指向 ip_core_t 的指標，找不到回傳 NULL
 */
const ip_core_t *find_ip_by_name(const ip_map_t *map, const char *name);

/**
 * print_ip_map()
 *   列印所有 IP Core 的名稱與位址（供 CLI `list` 指令使用）
 */
void print_ip_map(const ip_map_t *map);

#endif /* HWH_PARSER_H */
