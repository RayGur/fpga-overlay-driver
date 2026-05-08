/*
 * test_hwh_parser.c — 獨立測試 hwh_parser 模組
 *
 * 編譯：gcc -Wall -I../include -o test_hwh_parser test_hwh_parser.c ../src/hwh_parser.c
 * 執行：./test_hwh_parser <your_design.hwh>
 */
#include <stdio.h>
#include "../src/hwh_parser.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <design.hwh>\n", argv[0]);
        return 1;
    }

    ip_map_t map;
    int ret = parse_hwh(argv[1], &map);

    if (ret != 0) {
        printf("[FAIL] parse_hwh returned %d\n", ret);
        return 1;
    }

    printf("[PASS] Parsed %d IP cores:\n", map.count);
    print_ip_map(&map);

    /* bonus: test find_ip_by_name with first entry */
    if (map.count > 0) {
        const ip_core_t *found = find_ip_by_name(&map, map.cores[0].name);
        if (found)
            printf("[PASS] find_ip_by_name('%s') succeeded\n", map.cores[0].name);
        else
            printf("[FAIL] find_ip_by_name('%s') returned NULL\n", map.cores[0].name);

        const ip_core_t *notfound = find_ip_by_name(&map, "nonexistent_ip");
        if (!notfound)
            printf("[PASS] find_ip_by_name('nonexistent_ip') correctly returned NULL\n");
    }

    return 0;
}
