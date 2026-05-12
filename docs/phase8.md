# Phase 8 — DMA 支援

> 狀態：🔲 待開始（Phase 7 KD240 驗證完成後進行）

---

## 目標

為工具加入 DMA 資料搬運能力，支援 AXI DMA IP 的 SG（Scatter-Gather）模式傳輸，讓工具能處理 BCI 等高速串流應用。

---

## 背景

- AXI-Lite 只做控制暫存器，資料移動用 AXI-Stream + AXI DMA
- DMA 需要實體連續記憶體（physical address），用 `u-dma-buf` kernel module 解決
- 資料量參考：16ch × 32-bit × 30kHz ≈ 1.92 MB/s（學長 SPI 設計規格）
- DMA 模式：Scatter-Gather（學長建議）
- 等待策略：直接實作 UIO interrupt，不先做 polling

---

## KD240 環境（已確認）

| 項目 | 狀態 | 說明 |
|---|---|---|
| CMA | ✅ | 512 MB 保留（`cma=512M`），~210 MB 空閒 |
| kernel headers | ✅ | `/lib/modules/5.15.0-1061-xilinx-zynqmp/build` 完整 |
| gcc | ✅ | 11.4.0 |
| UIO | ✅ | `uio_pdrv_genirq` 已載入，`/dev/uio0–4` 存在，cmdline 有 `uio_pdrv_genirq.of_id=generic-uio` |
| u-dma-buf | 🔲 | 需從 source build（`ikwzm/u-dma-buf`），build 條件已具備 |

> `/dev/udmabuf`（無數字）是 Linux 標準 display buffer driver，非此處使用的 u-dma-buf。
> u-dma-buf 正確載入後會出現 `/dev/udmabuf0`。
> AXI DMA interrupt 載入後會自動出現新的 `/dev/uioX`（因 `uio_pdrv_genirq.of_id=generic-uio`）。

---

## Step 8.0 — 設計確認（實作前置）

✅ 討論完成

### API 設計（`include/dma.h`）

```c
// lifecycle
dma_t *dma_open(fpga_addr_t base, const char *uio_dev,
                size_t buf_size, int ring_size);
void   dma_close(dma_t *h);

// 雙向（loopback / BCI 主要用途）
int dma_transfer(dma_t *h, const void *tx, void *rx, size_t len);

// 單向（未來彈性）
int dma_send(dma_t *h, const void *data, size_t len);  // MM2S only
int dma_recv(dma_t *h, void *buf,        size_t len);  // S2MM only
```

- BD ring 管理完全封裝在 `dma.c` 內部
- 等待完成用 UIO interrupt（`read(uio_fd)` block，`write(uio_fd)` re-enable）
- MM2S / S2MM 邏輯分開但不暴露給呼叫者

### 暫定參數（可能依實測調整）

| 參數 | 暫定值 | 說明 |
|---|---|---|
| `buf_size` | 4096 bytes | ≈ 2ms BCI 資料，page-aligned |
| `ring_size` | 4 | 夠展示環形機制，不過於複雜 |

---

## Step 8.1 — build & install u-dma-buf

🔲 待完成

```bash
git clone https://github.com/ikwzm/u-dma-buf.git
cd u-dma-buf
make
sudo insmod u-dma-buf.ko udmabuf0=4096
ls /dev/udmabuf0
cat /sys/class/u-dma-buf/udmabuf0/phys_addr
```

---

## Step 8.2 — Vivado：建立 AXI DMA Loopback Design（xck24）

🔲 待建立

### 目的

不依賴外部 ADC/DAC，用 FIFO loopback 驗證 SG DMA 資料通路正確性。驗證通過後換上真實 BCI IP 只需替換 bitstream，軟體不動。

### Block Design 組成

| IP | 關鍵設定 |
|---|---|
| `zynq_ultra_ps_e` | HPM0_FPD on（32-bit），HP0_FPD on（128-bit），PL→PS interrupt 接入 |
| `axi_dma` | SG on，Buffer Length 23-bit，MM width 128-bit，Stream width 32-bit，Burst 256 |
| `axis_data_fifo` | TDATA 32-bit，depth 512 |
| `smartconnect` | 1 master（HPM0）→ 1 slave（axi_dma s_axi_lite） |
| `axi_interconnect` | 1 master（HP0）← 3 slaves（m_axi_sg, m_axi_mm2s, m_axi_s2mm） |

### 連線架構

```
PS HPM0_FPD → smartconnect → axi_dma s_axi_lite
PS HP0_FPD  ← axi_interconnect ← m_axi_sg
                                ← m_axi_mm2s
                                ← m_axi_s2mm

axi_dma M_AXIS_MM2S → axis_data_fifo → axi_dma S_AXIS_S2MM
axi_dma mm2s_introut ─┐
axi_dma s2mm_introut ─┴→ PS PL→PS interrupt
```

### 產出

```
kd240_dma_loopback.bit
kd240_dma_loopback.hwh
```

---

## Step 8.3 — 實作 dma.c（SG + UIO interrupt）

🔲 待實作

新增 `src/dma.c` / `include/dma.h`，實作：

- mmap `/dev/udmabuf0` 拿虛擬位址，從 sysfs 讀實體位址
- 建立 SG descriptor chain（BD ring，ring_size 個 BD）
- MMIO 控制 AXI DMA 暫存器（MM2S / S2MM）
- `read(uio_fd)` block 等 interrupt，`write(uio_fd)` re-enable
- 完整 error handling（每個步驟明確回傳錯誤碼）
- 獨立可測，不整合進 main.c 直到驗證通過

---

## Step 8.4 — 端對端驗證

🔲 待驗證

```
軟體產生測試資料 → dma_transfer() → FIFO loopback → 讀回比對
```

驗證通過條件：讀回資料與寫入資料完全一致。

---

## Step 8.5 — CLI 加入 DMA 指令

🔲 待實作

```bash
./overlay dma_write <design.hwh> <ip_name> <data_file>
./overlay dma_read  <design.hwh> <ip_name> <length> <output_file>
```

---

## 不在此 Phase 的項目

- Kernel DMA driver（u-dma-buf + UIO interrupt 不夠用再升級）
- 真實 BCI IP 接入（Phase 9 之後）

---

## 相關文件

- `docs/phase7.md` — 前置（KD240 AXI-Lite 驗證）
- `docs/decisions.md` — DEC-08、DEC-09、DEC-10、DEC-11
- `docs/phase9.md` — 後續（KV260 最終移植）
