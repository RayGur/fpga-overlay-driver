# FPGA Overlay Driver

不依賴 PYNQ 的 Bitstream 載入工具。以 C Userspace Library 直接透過 Linux FPGA Manager 載入 bitstream、解析 HWH 取得 IP 位址、並以 `/dev/mem` 讀寫 PL 暫存器。

## 背景

PYNQ 框架在以下情境不適用：
- 無 PYNQ 環境的 Linux 系統
- 需要避免 Python overhead 直接操控硬體
- BCI 等即時性要求較高的應用

本工具重新實作 PYNQ overlay 的核心功能，完全不依賴 PYNQ。

## 硬體目標

| 板子 | 用途 | 晶片 | ARM |
|------|------|------|-----|
| PYNQ-Z2 | 開發驗證（Zynq-7000） | XC7Z020 | Cortex-A9 (32-bit) |
| Kria KD240 | ZynqMP 首次驗證（Phase 7） | XCK24 | Cortex-A53 (64-bit) |
| Kria KV260 | 最終部署目標（Phase 9） | XCK26 | Cortex-A53 (64-bit) |

## 模組架構

```
src/
├── bit2bin.c      將 Vivado .bit 轉為 FPGA Manager 可接受的 .bin（剝 header、byte-swap）
├── hwh_parser.c   解析 .hwh XML，取得各 IP Core 的 base/high address
├── fpga_load.c    操作 sysfs，觸發 FPGA Manager 載入 bitstream
├── mmio.c         /dev/mem + mmap，讀寫 PL 暫存器
└── main.c         CLI 入口，整合以上四個模組
```

## Build

**依賴**：`libexpat1-dev`（`sudo apt install libexpat1-dev`）

```bash
# PYNQ-Z2（32-bit address）
make PLATFORM=Z2

# KV260（64-bit address，Phase 7）
make

# 只建 unit tests
make tests

# 清除
make clean
```

## CLI 使用方式

```bash
# 轉換 .bit → .bin 並載入 FPGA
sudo ./overlay load  <design.bit> <design.hwh>

# 列出 .hwh 中所有 IP Core 與位址
     ./overlay list  <design.hwh>

# 讀寫指定 IP Core 的暫存器
sudo ./overlay read  <design.hwh> <ip_name> <offset_hex>
sudo ./overlay write <design.hwh> <ip_name> <offset_hex> <value_hex>
```

`load` / `read` / `write` 需要 root（存取 `/lib/firmware/` 與 `/dev/mem`）。

### 範例（cordic design）

```bash
$ sudo ./overlay load design_1.bit design_1.hwh
[✓] Converted design_1.bit → design_1.bin
[✓] Parsed 1 IP cores from design_1.hwh
[✓] Bitstream loaded, FPGA state: operating

$ ./overlay list design_1.hwh
IP Cores (1):
  cordic_1    base=0x40000000  high=0x4000FFFF

$ sudo ./overlay read design_1.hwh cordic_1 0x00
0x00000004

$ sudo ./overlay write design_1.hwh cordic_1 0x00 0x00000001
[✓] Wrote 0x00000001 to cordic_1 offset 0x0
```

## 開發指南

本專案在 Windows 上編輯，透過 SCP 或 git clone 同步到板子上以 native gcc 編譯與測試。

```bash
# 板子上 clone（建議）
git clone https://github.com/RayGur/fpga-overlay-driver.git
cd fpga-overlay-driver
make              # KD240 / KV260（64-bit）
make PLATFORM=Z2  # PYNQ-Z2（32-bit）
```

**開發規則：**
- 位址型別一律用 `fpga_addr_t`，不直接用 `uint32_t` / `uint64_t`（定義在 `include/config.h`）
- 每個模組獨立驗證後才整合進 `main.c`
- 操作 `/lib/firmware/` 或寫 sysfs 前確認你知道這是不可逆操作

## 測試指南

每個模組有對應的獨立測試程式，放在 `test/`：

```bash
# 建置所有 unit tests
make tests PLATFORM=Z2

# bit2bin：驗證 .bit → .bin 轉換輸出正確
sudo ./test/test_bit2bin <design.bit>

# hwh_parser：驗證 IP Core 地址解析
./test/test_hwh_parser <design.hwh>
```

`main.c` 的 end-to-end 測試需要實際 bitstream，測試資料放在 `testing_data/`：

```bash
# 完整流程測試
sudo ./overlay load  testing_data/cordic/design_1.bit testing_data/cordic/design_1.hwh
     ./overlay list  testing_data/cordic/design_1.hwh
sudo ./overlay read  testing_data/cordic/design_1.hwh cordic_1 0x00
sudo ./overlay write testing_data/cordic/design_1.hwh cordic_1 0x00 0x1
```

## 平台抽象

所有地址型別使用 `fpga_addr_t`，定義於 `include/config.h`：

```c
#ifdef ZYNQ7000
    typedef uint32_t fpga_addr_t;   // PYNQ-Z2
#else
    typedef uint64_t fpga_addr_t;   // KV260
#endif
```

移植 KV260 時只需改 Makefile 的 `PLATFORM` 參數，其餘 `.c` 不需動。

## 注意事項

- FPGA Manager 寫入順序：先寫 `flags`，再寫 `firmware`；順序錯誤不報錯但載入會失敗
- `.bin` 必須先複製到 `/lib/firmware/`，FPGA Manager 才能讀取
- mmap offset 需 page-align（4096 bytes），`mmio.c` 內部處理
- KV260 移植須注意 TrustZone 記憶體限制與 64-bit `off_t`，詳見 `docs/phase7.md`

## 已知限制

- `bit2bin` 只以 Vivado 2022.2 產出的 `.bit` 驗證過；不同版本的 header 格式未驗證
- `bit_find_payload()` 遇到無法解析的格式會明確報錯（回傳 `-1`），不會靜默產生錯誤輸出
- 目前不支援 partial reconfiguration（partial bitstream）
- DMA 與中斷不在此工具範疇內

## 當前進度

| Phase | 內容 | 狀態 |
|-------|------|------|
| 1 | 環境確認（FPGA Manager、/lib/firmware、/dev/mem） | ✅ 完成 |
| 2 | `bit2bin.c` — .bit → .bin 轉換 | ✅ 完成，板子驗證通過 |
| 3 | `fpga_load.c` — FPGA Manager sysfs 載入 | ✅ 完成，板子驗證通過 |
| 4 | `hwh_parser.c` — HWH XML 解析 | ✅ 完成，板子驗證通過 |
| 5 | `mmio.c` — /dev/mem mmap 讀寫 | ✅ 完成，板子驗證通過 |
| 6 | `main.c` — CLI 整合 | ✅ 完成，板子驗證通過 |
| 7 | KD240 移植（ZynqMP 64-bit 首次驗證） | 🔄 進行中（環境確認✅ 編譯✅ 端對端驗證中） |
| 8 | DMA 支援 | 🔲 待開發 |
| 9 | KV260 移植（最終目標板） | 🔲 待開發 |

## 文件索引

| 主題 | 文件 |
|------|------|
| 整體架構與 Phase 清單 | `docs/plan.md` |
| 設計決策紀錄 | `docs/decisions.md` |
| KV260 移植注意事項 | `docs/phase7.md` |
