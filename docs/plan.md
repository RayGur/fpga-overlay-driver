# FPGA Overlay Driver — 完整計畫書

> 版本：v1.0 · 更新至 Phase 2（完成）

---

## 1. 專案目標與背景

### 動機

PYNQ 提供了方便的 overlay 機制，但框架層次偏厚、相依性重，在以下情境造成不便：

- 需要在無 PYNQ 環境的 Linux 系統上部署
- 希望避免 Python overhead，以 C 直接操控硬體
- BCI 等即時性要求高的應用中，Python 框架延遲不可接受

### 目標

以 **C Userspace Library** 重新實作 PYNQ overlay 核心功能：

1. 將 Vivado 產出的 `.bit` 轉換為 Linux FPGA Manager 可接受的 `.bin`
2. 解析 `.hwh` 取得各 IP Core 記憶體映射位址
3. 透過 Linux sysfs 介面載入 bitstream
4. 以 `/dev/mem` + `mmap` 讀寫 PL 暫存器
5. 提供乾淨的 CLI 介面

### 非目標（此階段不實作）

- Kernel Module
- Device Tree Overlay 自動化
- DMA 或中斷處理

---

## 2. 硬體與軟體環境

| 項目 | 說明 |
|------|------|
| 開發驗證板 | PYNQ-Z2（XC7Z020，Cortex-A9，32-bit，Ubuntu PYNQ） |
| 臨時 ZynqMP 測試板 | KD240（XCK24，Cortex-A53，64-bit，Ubuntu 22.04） |
| 最終目標板 | KV260（XCK26，Cortex-A53，64-bit，Ubuntu 22.04） |
| 開發方式 | Windows 寫 code → SCP 傳板子 → SSH native gcc 編譯 |

---

## 3. 整體架構

### 模組拆分

| 模組 | 檔案 | 功能 |
|------|------|------|
| Bitstream 轉換 | `bit2bin.c` | 剝除 `.bit` header，byte-swap 產生 `.bin` |
| HWH 解析 | `hwh_parser.c` | 解析 XML，取得各 IP 的 base/high address |
| FPGA 載入 | `fpga_load.c` | 操作 sysfs，觸發 FPGA Manager 載入 |
| MMIO 存取 | `mmio.c` | open `/dev/mem` + mmap，讀寫 PL 暫存器 |
| CLI 入口 | `main.c` | 整合以上模組，提供命令列介面 |

### 目錄結構

```
fpga-overlay-driver/
├── src/
│   ├── main.c
│   ├── bit2bin.c / bit2bin.h
│   ├── hwh_parser.c / hwh_parser.h
│   ├── fpga_load.c / fpga_load.h
│   └── mmio.c / mmio.h
├── include/
│   └── config.h
├── test/
│   ├── test_bit2bin.c
│   └── test_hwh_parser.c
└── docs/
    ├── plan.md          ← 本文件
    ├── phase1.md
    ├── phase2.md
    ├── phase3.md
    ├── phase4.md
    ├── phase5.md
    ├── phase6.md
    ├── phase7.md
    ├── decisions.md
    └── design.md
```

### 平台抽象（config.h）

```c
#ifdef ZYNQ7000
    typedef uint32_t fpga_addr_t;   // PYNQ-Z2
#else
    typedef uint64_t fpga_addr_t;   // KV260
#endif
```

---

## 4. 所有 Phase 與 Step 清單

### Phase 1 — 環境確認

| Step | 說明 | 狀態 |
|------|------|------|
| 1.1 | 手動 shell 確認 FPGA Manager sysfs 路徑存在 | ✅ 完成 |
| 1.2 | 手動載入測試 bitstream，確認 state → operating | ✅ 完成 |
| 1.3 | 確認 `/dev/mem` 可存取（權限） | ✅ 完成 |

### Phase 2 — Bitstream 轉換（bit2bin）

| Step | 說明 | 狀態 |
|------|------|------|
| 2.1 | 設計 `bit2bin.h` API，建立 skeleton | ✅ 完成 |
| 2.2 | 實作 `bit_find_payload()` 與 `convert_bit_to_bin()` | ✅ 完成 |
| 2.3 | 以 `test_bit2bin.c` 在板子上驗證輸出 `.bin` 正確 | ✅ 完成 |

### Phase 3 — FPGA 載入（fpga_load）

| Step | 說明 | 狀態 |
|------|------|------|
| 3.1 | 設計 `fpga_load.h` API，建立 skeleton | ✅ 完成 |
| 3.2 | 實作 `copy_to_firmware()` | ✅ 完成 |
| 3.3 | 實作 `fpga_load_bitstream()`（寫 sysfs + poll state） | ✅ 完成 |
| 3.4 | 端對端驗證：.bit → .bin → FPGA Manager → operating | ✅ 完成 |

### Phase 4 — HWH 解析（hwh_parser）

| Step | 說明 | 狀態 |
|------|------|------|
| 4.1 | 設計 `hwh_parser.h` API（IP core struct） | ✅ 完成 |
| 4.2 | 實作 XML 解析（libexpat SAX，掃描 MEMRANGE） | ✅ 完成 |
| 4.3 | 以 `test_hwh_parser.c` 驗證 address map 正確 | ✅ 完成 |

### Phase 5 — MMIO 存取（mmio）

| Step | 說明 | 狀態 |
|------|------|------|
| 5.1 | 設計 `mmio.h` API | ✅ 完成 |
| 5.2 | 實作 `mmio_open()` / `mmio_read32()` / `mmio_write32()` / `mmio_close()` | ✅ 完成 |
| 5.3 | 驗證讀寫 axi_gpio 等已知 IP 暫存器 | ✅ 完成（部分，write+readback 留待 Phase 6） |

### Phase 6 — CLI 整合

| Step | 說明 | 狀態 |
|------|------|------|
| 6.1 | 設計 CLI 介面（`load` / `list` / `read` / `write`） | 🔲 待實作 |
| 6.2 | 整合所有模組到 `main.c` | 🔲 待實作 |
| 6.3 | 錯誤處理與 usage message | 🔲 待實作 |
| 6.4 | 完整流程 end-to-end 測試 | 🔲 待驗證 |

### Phase 7 — 移植至 KD240（ZynqMP 64-bit 首次驗證）

> 練習板：KD240（xck24，Ubuntu 22.04），架構與 KV260 相同

| Step | 說明 | 狀態 |
|------|------|------|
| 7.0 | KD240 環境確認（`check_kv260_env.sh`） | 🔲 待執行 |
| 7.1 | Vivado：建立 AXI GPIO test design（xck24） | 🔲 待建立 |
| 7.2 | 程式碼調整：64-bit addr、off_t、printf 格式 | 🔲 待調整 |
| 7.3 | KD240 端對端驗證（load / list / read / write） | 🔲 待驗證 |

### Phase 8 — DMA 支援

| Step | 說明 | 狀態 |
|------|------|------|
| 8.0 | 確認 userspace DMA 機制（udmabuf / dma-proxy）可用 | 🔲 待確認 |
| 8.1 | Vivado：建立 AXI DMA loopback design（xck24） | 🔲 待建立 |
| 8.2 | 實作 DMA 模組（分配 CMA buffer、設定 descriptor、觸發傳輸） | 🔲 待實作 |
| 8.3 | CLI 加入 `dma_write` / `dma_read` 指令 | 🔲 待實作 |
| 8.4 | KD240 端對端 DMA 驗證（MM2S → FIFO loopback → S2MM） | 🔲 待驗證 |

### Phase 9 — 移植至 KV260（最終目標板）

| Step | 說明 | 狀態 |
|------|------|------|
| 9.1 | KV260 環境確認（`check_kv260_env.sh`） | 🔲 待執行 |
| 9.2 | 取得 KV260 專用 bitstream（xck26 重新合成） | 🔲 待取得 |
| 9.3 | KV260 端對端驗證（GPIO + DMA） | 🔲 待驗證 |

---

## 5. 目前進度摘要

```
✅ Phase 1 — 環境確認         完成
✅ Phase 2 — bit2bin          實作完成，板子驗證通過
✅ Phase 3 — fpga_load        實作完成，板子驗證通過
✅ Phase 4 — hwh_parser       實作完成，板子驗證通過
✅ Phase 5 — mmio             實作完成，板子驗證通過（open/mmap/read/write）
🔲 Phase 6 — CLI 整合         下一步
```

---

## 6. CLI 預期輸出（目標樣貌）

```bash
$ ./overlay load design.bit design.hwh
[✓] Converted design.bit → design.bin
[✓] Parsed 3 IP cores from design.hwh
[✓] Bitstream loaded, FPGA state: operating

$ ./overlay list
IP Cores:
  axi_gpio_0  @ 0x41200000
  axi_timer_0 @ 0x42800000
  my_dsp_core @ 0x43C00000

$ ./overlay read  design.hwh axi_gpio_0 0x00
0x00000001

$ ./overlay write design.hwh axi_gpio_0 0x00 0xFF
[✓] Written
```
