# Phase 5 — MMIO 存取（mmio）

> 狀態：🔲 待開始

---

## 目標

透過 `/dev/mem` + `mmap`，從 userspace 直接讀寫 PL 上的 AXI 暫存器。

---

## 背景

Zynq 的 PS（ARM）與 PL（FPGA）之間透過 AXI 匯流排連接。AXI 暫存器映射到 ARM 的實體位址空間，可透過 `/dev/mem` 存取。

```
ARM userspace → open("/dev/mem") → mmap(base_addr) → volatile uint32_t* → 讀寫
```

---

## Step 5.1 — 設計 API

🔲 待實作

```c
// mmio.h

typedef struct {
    int      fd;        // /dev/mem file descriptor
    void    *map_base;  // mmap 回傳的基底位址
    size_t   map_size;  // 實際 mmap 的大小（page-aligned）
    fpga_addr_t phys_base;  // 目標實體位址（page-aligned）
    fpga_addr_t offset;     // 在 map 內的 offset
} mmio_t;

int      mmio_open(mmio_t *m, fpga_addr_t phys_addr, size_t size);
uint32_t mmio_read(const mmio_t *m, size_t reg_offset);
void     mmio_write(mmio_t *m, size_t reg_offset, uint32_t value);
void     mmio_close(mmio_t *m);
```

---

## Step 5.2 — 實作

🔲 待實作

### mmio_open() 要點

```c
int mmio_open(mmio_t *m, fpga_addr_t phys_addr, size_t size)
{
    m->fd = open(DEVMEM_PATH, O_RDWR | O_SYNC);

    // 對齊到 page boundary
    m->phys_base = phys_addr & ~(PAGE_SIZE - 1);
    m->offset    = phys_addr & (PAGE_SIZE - 1);
    m->map_size  = size + m->offset;

    m->map_base = mmap(NULL, m->map_size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED,
                       m->fd, (off_t)m->phys_base);
    // 檢查 MAP_FAILED ...
}
```

### mmio_read() / mmio_write() 要點

```c
uint32_t mmio_read(const mmio_t *m, size_t reg_offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)
        ((char *)m->map_base + m->offset + reg_offset);
    return *reg;
}
```

### 注意事項

- `volatile` 修飾詞不可省略，防止編譯器最佳化掉記憶體存取
- `off_t` 在 32-bit 系統預設為 32-bit，若 phys_addr > 2GB 需 `_FILE_OFFSET_BITS=64`（PYNQ-Z2 上 AXI 位址通常 < 0x80000000，不成問題；KV260 需注意）
- ⚠️ **KV260 移植警告**：TrustZone 可能限制部分記憶體區域，若 mmap 失敗且 errno 為 EPERM，需確認目標 IP 的位址是否在 secure world

---

## Step 5.3 — 驗證

🔲 待驗證

```bash
# 載入有 axi_gpio 的 bitstream 後
sudo ./test_mmio 0x41200000   # axi_gpio_0 base addr

# 預期：讀取 DATA register (offset 0x00)
Read  [0x00] = 0x00000000
# 寫入後再讀回
Write [0x00] = 0x000000FF
Read  [0x00] = 0x000000FF
```

---

## 相關文件

- `include/config.h` — `DEVMEM_PATH`、`PAGE_SIZE`、`fpga_addr_t`
- `docs/phase4.md` — hwh_parser 提供 `base_addr`
- `docs/phase6.md` — CLI 整合
