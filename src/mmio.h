#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>
#include "../include/config.h"

/**
 * mmio.h — /dev/mem + mmap PL register access
 *
 * 使用方式：
 *   mmio_region_t *r = mmio_open(0x41200000, 0x10000);
 *   uint32_t val = mmio_read32(r, 0x00);
 *   mmio_write32(r, 0x00, 0xFF);
 *   mmio_close(r);
 *
 * 注意事項：
 *   - 需要 root 權限（或加入 kmem group）
 *   - mmap offset 必須對齊 PAGE_SIZE (4096)
 *   - 對不存在的位址讀寫會造成 SIGBUS，需確認 .hwh 位址正確
 */

typedef struct {
    void        *base;      /* mmap 回傳的虛擬位址起點       */
    size_t       size;      /* mmap 的映射大小（已對齊 page） */
    fpga_addr_t  phys_base; /* 實際的物理位址（對齊前）       */
    int          fd;        /* /dev/mem 的 fd                 */
} mmio_region_t;

/**
 * mmio_open()
 *   對指定物理位址範圍做 mmap
 *
 * @param phys_addr  IP Core 的 BASEADDR（from .hwh）
 * @param size       映射大小（通常用 high_addr - base_addr + 1）
 * @return           mmio_region_t*，失敗回傳 NULL
 */
mmio_region_t *mmio_open(fpga_addr_t phys_addr, size_t size);

/**
 * mmio_close()
 *   munmap 並關閉 fd
 */
void mmio_close(mmio_region_t *region);

/**
 * mmio_read32() / mmio_write32()
 *   以 32-bit 存取暫存器
 *
 * @param region  由 mmio_open() 取得
 * @param offset  相對於 BASEADDR 的 byte offset
 */
uint32_t mmio_read32(const mmio_region_t *region, uint32_t offset);
void     mmio_write32(mmio_region_t *region, uint32_t offset, uint32_t value);

#endif /* MMIO_H */
