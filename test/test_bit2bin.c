/*
 * test_bit2bin.c — 獨立測試 bit2bin 模組
 *
 * 編譯：gcc -Wall -I../include -o test_bit2bin test_bit2bin.c ../src/bit2bin.c
 * 執行：./test_bit2bin <your_design.bit>
 */
#include <stdio.h>
#include "../src/bit2bin.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <design.bit>\n", argv[0]);
        return 1;
    }

    const char *bit_path = argv[1];
    const char *bin_path = "/tmp/test_output.bin";

    printf("Testing bit2bin: %s → %s\n", bit_path, bin_path);

    int ret = convert_bit_to_bin(bit_path, bin_path);
    if (ret == 0)
        printf("[PASS] Conversion succeeded. Check %s\n", bin_path);
    else
        printf("[FAIL] Conversion returned %d\n", ret);

    return ret;
}
