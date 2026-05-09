# FPGA Overlay Driver

不依賴 PYNQ 的 Bitstream 載入工具。以 C Userspace Library 直接透過 Linux FPGA Manager sysfs 介面載入 bitstream、解析 HWH 取得 IP 位址、並以 `/dev/mem` 讀寫 PL 暫存器。

## 背景

PYNQ 框架層次厚、Python overhead 重，在以下情境不適用：
- 無 PYNQ 環境的 Linux 系統
- 需要避免 Python overhead 直接操控硬體
- BCI 等即時性要求較高的應用

本工具重新實作 PYNQ overlay 的核心功能，完全不依賴 PYNQ。

## 硬體目標

| 板子 | 用途 | 晶片 | ARM |
|------|------|------|-----|
| PYNQ-Z2 | 開發驗證 | XC7Z020 | Cortex-A9 (32-bit) |
| Kria KV260 | 部署目標 | XCK26 | Cortex-A53 (64-bit) |

## 模組架構

```
src/
├── bit2bin.c      將 Vivado .bit 轉為 FPGA Manager 可接受的 .bin
├── hwh_parser.c   解析 .hwh XML，取得各 IP Core 的 base/high address
├── fpga_load.c    操作 sysfs，觸發 FPGA Manager 載入 bitstream
├── mmio.c         /dev/mem + mmap，讀寫 PL 暫存器
└── main.c         CLI 整合入口
```

## CLI 介面

```bash
./overlay load design.bit design.hwh   # 轉換並載入 bitstream
./overlay list                          # 列出所有 IP Core 與位址
./overlay write axi_gpio_0 0x00 0xFF   # 寫暫存器
./overlay read  axi_gpio_0 0x00        # 讀暫存器
```

## 開發流程

```bash
# Windows 上寫 code，SCP 傳板子，SSH 進去編譯
scp src/bit2bin.c xilinx@192.168.2.99:~/fpga-overlay-driver/src/
ssh xilinx@192.168.2.99
cd fpga-overlay-driver && make
```

## Build

```bash
# PYNQ-Z2（32-bit）
make PLATFORM=Z2

# KV260（64-bit，Step 7 之後）
make PLATFORM=KV260

# 只跑 unit tests
make tests

# 清除
make clean
```

## 當前進度

| Step | 內容 | 狀態 |
|------|------|------|
| Phase 0 | 環境確認（FPGA Manager、/lib/firmware、/dev/mem） | ✅ 完成 |
| Phase 1 | Repo 建立、目錄結構、骨架檔案 | ✅ 完成 |
| Step 2 | bit2bin.c | ✅ 完成，已驗證 |
| Step 3 | fpga_load.c | 🔲 進行中 |
| Step 4 | hwh_parser.c | 🔲 待開發 |
| Step 5 | mmio.c | 🔲 待開發 |
| Step 6 | CLI 整合 + 錯誤處理 | 🔲 待開發 |
| Step 7 | 移植至 KV260 | 🔲 待開發 |

## bit2bin 設計說明

Vivado 產出的 `.bit` 檔有兩個問題讓它無法直接送給 FPGA Manager：

1. **有 TLV header**：前面是設計名稱、晶片型號、日期等 metadata，FPGA Manager 不認識
2. **Big-endian**：payload 的每個 32-bit word 是 big-endian，ARM 是 little-endian

轉換方式：
- 解析 TLV header，找到 tag `0x65`，其 4-byte length 欄位給出 payload 的精確大小
- 對 payload 每個 word 做 `swap32()`，寫出 `.bin`

Xilinx 定義的 sync word 值為 `0xAA995566`（Xilinx UG470: 7 Series FPGAs Configuration User Guide）。在 `.bit` 裡以 big-endian 存成 bytes `AA 99 55 66`；經過 `swap32` 後在 `.bin` 裡變成 bytes `66 55 99 AA`；ARM 以 little-endian 讀這 4 個 bytes，得到 uint32 值 `0xAA995566`，正確。

> ⚠️ `FF FF FF AA` 不是 sync word，是 NOP preamble，不可用來定位 payload 起點。

## 平台移植

```c
// include/config.h
#ifdef ZYNQ7000
    typedef uint32_t fpga_addr_t;   // PYNQ-Z2
#else
    typedef uint64_t fpga_addr_t;   // KV260
#endif
```

KV260 主要差異：64-bit AXI address、TrustZone（部分記憶體區域受限）、bitstream 需重新合成。

## 注意事項

- `fpga_load` 和 `mmio` 需要 root（`sudo`）
- FPGA Manager 寫入順序：先寫 `flags`，再寫 `firmware`，順序錯了不報錯但載入失敗
- `.bin` 須先放到 `/lib/firmware/`，FPGA Manager 才能讀取
- mmap offset 需 page-align（4096 bytes），詳見 `mmio.c`

## 已知限制

- `bit2bin` 只用兩個同版本 Vivado（2022.2）的 `.bit` 測試過，不同版本 header 尚未驗證
- 遇到無法解析的格式，`bit_find_payload()` 會明確報錯（`-1`），不會靜默產生錯誤輸出