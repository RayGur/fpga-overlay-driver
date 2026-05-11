# Phase 8 — DMA 支援

> 狀態：🔲 待開始（Phase 7 KD240 驗證完成後進行）

---

## 目標

為工具加入 DMA 資料搬運能力，支援 AXI DMA IP 的 MM2S / S2MM 傳輸，
讓工具能處理需要大量資料移動的 IP（如 BCI 訊號處理）。

---

## 背景

AXI-Lite 只適合控制暫存器（頻寬低），實際資料搬運需要 DMA（AXI-Stream / AXI HP port）。
目前工具是「控制平面」，Phase 8 加入「資料平面」。

---

## Step 8.0 — 確認 userspace DMA 機制

🔲 待確認

KD240 上 userspace DMA 的可行方案，需確認何者可用：

| 機制 | 說明 | 需要 |
|------|------|------|
| `udmabuf` | kernel module，分配 CMA 連續記憶體，userspace 直接 mmap | 板子上需載入 module |
| `dma-proxy` | Xilinx 官方 kernel driver | 需確認 kernel config |
| `/dev/mem` + CMA | 直接 mmap 保留記憶體區段 | 需 device tree 保留 CMA |

確認指令：
```bash
find /lib/modules -name "udmabuf*" 2>/dev/null
ls /dev/dma*
cat /proc/iomem | grep -i cma
```

---

## Step 8.1 — Vivado：建立 AXI DMA loopback design（xck24）

🔲 待建立

### Block Design 組成

```
zynq_ultra_ps_e ──HPM0──→ SmartConnect ──→ AXI DMA (s_axi_lite)
                └──HP0──→ AXI Interconnect ──→ AXI DMA (m_axi_mm2s / m_axi_s2mm)

AXI DMA M_AXIS_MM2S ──→ AXI4-Stream Data FIFO ──→ AXI DMA S_AXIS_S2MM
```

MM2S（memory→stream）→ FIFO loopback → S2MM（stream→memory），
不需外部 IP 即可驗證完整 DMA 資料路徑。

---

## Step 8.2 — 實作 DMA 模組

🔲 待實作

新增 `src/dma.c` / `include/dma.h`，實作：
- 透過選定機制分配 physically contiguous buffer
- 設定 AXI DMA control registers（MM2S / S2MM descriptor）
- 觸發傳輸、poll 完成

---

## Step 8.3 — CLI 加入 DMA 指令

🔲 待實作

```bash
./overlay dma_write <design.hwh> <ip_name> <data_file>
./overlay dma_read  <design.hwh> <ip_name> <length> <output_file>
```

---

## Step 8.4 — KD240 端對端 DMA 驗證

🔲 待驗證

寫入資料 → MM2S → FIFO loopback → S2MM → 讀回比對，確認資料一致。

---

## 相關文件

- `docs/phase7.md` — 前置（AXI-Lite 移植）
- `docs/phase9.md` — 後續（KV260 最終移植）
