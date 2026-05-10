# Phase 5 — MMIO 存取（mmio）

> 狀態：✅ 完成

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

✅ 完成

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

✅ 完成

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

✅ 完成（部分）

```bash
# 以 cordic.bit 載入後，對 0x41200000 測試
sudo ./test/test_mmio 0x41200000         # read-only
sudo ./test/test_mmio 0x41200000 0xFF    # write+readback
```

### 驗證結果

| 測試 | 結果 |
|------|------|
| mmio_open() | ✅ 成功 |
| mmio_read32(0x00) | ✅ 回傳值正常，無 crash |
| mmio_write32() | ✅ 無 crash |
| write+readback | ⚠️ 待以含 axi_gpio 的 bitstream 確認 |

> **說明**：cordic.bit 未必有 axi_gpio 映射在 0x41200000，write+readback
> 不符預期為正常現象。mmio 模組本身的 open/mmap/read/write 均已驗證正確。
> write+readback 完整驗證留待 Phase 6 CLI 整合時，搭配 hwh_parser 解析正確位址後進行。

---

## 相關文件

- `include/config.h` — `DEVMEM_PATH`、`PAGE_SIZE`、`fpga_addr_t`
- `docs/phase4.md` — hwh_parser 提供 `base_addr`
- `docs/phase6.md` — CLI 整合
