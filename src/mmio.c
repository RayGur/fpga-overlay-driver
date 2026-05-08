#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include "mmio.h"
#include "../include/config.h"

mmio_region_t *mmio_open(fpga_addr_t phys_addr, size_t size)
{
    /* TODO: Step 5 實作
     *
     * 1. malloc mmio_region_t
     * 2. fd = open(DEVMEM_PATH, O_RDWR | O_SYNC)
     * 3. 計算 page-aligned offset：
     *      fpga_addr_t aligned = phys_addr & ~(PAGE_SIZE - 1);
     *      size_t offset_in_page = phys_addr - aligned;
     *      size_t mmap_size = size + offset_in_page;  // 確保涵蓋完整範圍
     * 4. base = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE,
     *               MAP_SHARED, fd, aligned)
     * 5. region->base = base + offset_in_page  ← 指向實際的 phys_addr
     * 6. 存好 fd、size、phys_base 供 close 使用
     *
     * 回傳 NULL on error
     */
    (void)phys_addr;
    (void)size;
    fprintf(stderr, "[mmio] not yet implemented\n");
    return NULL;
}

void mmio_close(mmio_region_t *region)
{
    /* TODO: Step 5 實作
     *
     * 1. munmap(region->base, region->size)
     * 2. close(region->fd)
     * 3. free(region)
     */
    (void)region;
}

uint32_t mmio_read32(const mmio_region_t *region, uint32_t offset)
{
    /* TODO: Step 5 實作
     * volatile uint32_t *ptr = (volatile uint32_t *)((uint8_t *)region->base + offset);
     * return *ptr;
     */
    (void)region;
    (void)offset;
    return 0xDEADBEEF;
}

void mmio_write32(mmio_region_t *region, uint32_t offset, uint32_t value)
{
    /* TODO: Step 5 實作
     * volatile uint32_t *ptr = (volatile uint32_t *)((uint8_t *)region->base + offset);
     * *ptr = value;
     */
    (void)region;
    (void)offset;
    (void)value;
}
