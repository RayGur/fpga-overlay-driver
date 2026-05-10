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
    mmio_region_t *region = malloc(sizeof(mmio_region_t));
    if (!region) {
        fprintf(stderr, "[mmio] malloc failed\n");
        return NULL;
    }

    region->fd = open(DEVMEM_PATH, O_RDWR | O_SYNC);
    if (region->fd < 0) {
        fprintf(stderr, "[mmio] open %s failed: %s\n", DEVMEM_PATH, strerror(errno));
        free(region);
        return NULL;
    }

    fpga_addr_t aligned      = phys_addr & ~((fpga_addr_t)(PAGE_SIZE - 1));
    size_t      offset_in_page = (size_t)(phys_addr - aligned);
    size_t      mmap_size    = size + offset_in_page;

    void *base = mmap(NULL, mmap_size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      region->fd, (off_t)aligned);
    if (base == MAP_FAILED) {
        fprintf(stderr, "[mmio] mmap failed: %s\n", strerror(errno));
        close(region->fd);
        free(region);
        return NULL;
    }

    region->base      = (uint8_t *)base + offset_in_page;
    region->size      = mmap_size;
    region->phys_base = phys_addr;

    return region;
}

void mmio_close(mmio_region_t *region)
{
    if (!region)
        return;

    size_t offset_in_page = (size_t)(region->phys_base & (PAGE_SIZE - 1));
    munmap((uint8_t *)region->base - offset_in_page, region->size);
    close(region->fd);
    free(region);
}

uint32_t mmio_read32(const mmio_region_t *region, uint32_t offset)
{
    volatile uint32_t *ptr = (volatile uint32_t *)((uint8_t *)region->base + offset);
    return *ptr;
}

void mmio_write32(mmio_region_t *region, uint32_t offset, uint32_t value)
{
    volatile uint32_t *ptr = (volatile uint32_t *)((uint8_t *)region->base + offset);
    *ptr = value;
}
