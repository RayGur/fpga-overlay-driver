#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "hwh_parser.h"
#include "../include/config.h"

/*
 * 實作策略選擇（Step 4 實作時擇一）：
 *
 * Option A — libxml2（推薦，板子通常已有）
 *   #include <libxml/parser.h>
 *   #include <libxml/tree.h>
 *   編譯時加：$(xml2-config --cflags --libs)
 *
 * Option B — 純字串搜尋（無相依性，但容易踩邊界條件）
 *   用 strstr() 搜尋 "BASEADDR" / "HIGHADDR"
 *   適合先快速驗證，後續再換 Option A
 *
 * 建議先用 Option B 把流程跑通，確認位址正確後再改 Option A。
 */

int parse_hwh(const char *hwh_path, ip_map_t *map)
{
    /* TODO: Step 4 實作
     *
     * 1. fopen hwh_path
     * 2. 讀入全部內容到 buffer
     * 3. 搜尋每個 <MODULE> 區塊：
     *    a. 取出 FULLNAME 屬性的最後一段作為 IP 名稱
     *       e.g. "design_1/axi_gpio_0" → "axi_gpio_0"
     *    b. 在該 MODULE 區塊內找 BASEADDR / HIGHADDR 的 VALUE
     *    c. 用 strtoul(value, NULL, 16) 轉成數字
     * 4. 填入 map->cores[]，更新 map->count
     * 5. 錯誤處理：檔案不存在、XML 格式錯誤、超過 MAX_IP_CORES
     *
     * 回傳 0 on success, -1 on error
     */
    (void)hwh_path;

    memset(map, 0, sizeof(*map));
    fprintf(stderr, "[hwh_parser] not yet implemented\n");
    return -1;
}

const ip_core_t *find_ip_by_name(const ip_map_t *map, const char *name)
{
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->cores[i].name, name) == 0)
            return &map->cores[i];
    }
    return NULL;
}

void print_ip_map(const ip_map_t *map)
{
    printf("IP Cores (%d):\n", map->count);
    for (int i = 0; i < map->count; i++) {
        printf("  %-32s @ " FPGA_ADDR_FMT " – " FPGA_ADDR_FMT "\n",
               map->cores[i].name,
               map->cores[i].base_addr,
               map->cores[i].high_addr);
    }
}
