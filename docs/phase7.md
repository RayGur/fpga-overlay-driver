# Phase 7 — 移植至 KD240（ZynqMP 64-bit 首次驗證）

> 狀態：🔲 待開始
> 目標板：KD240（xck24，Cortex-A53，64-bit，Ubuntu 22.04）
> 備註：KD240 架構與最終目標板 KV260 相同，作為 ZynqMP 首次驗證用

---

## 主要差異（vs PYNQ-Z2）

| 項目 | PYNQ-Z2 | KD240 / KV260 |
|------|---------|---------------|
| ARM 架構 | 32-bit Cortex-A9 | 64-bit Cortex-A53 |
| AXI Address 寬度 | `uint32_t` | `uint64_t` |
| 晶片 | XC7Z020 | XCK24（KD240）/ XCK26（KV260） |
| OS | Ubuntu（PYNQ） | Ubuntu 22.04 LTS |
| gcc / libexpat | 有 | 有（已確認） |
| FPGA Manager sysfs | 路徑相同 | 預期相同，7.0 確認 |
| TrustZone | 無 | 有，AXI Peripheral 位址通常 non-secure |
| `off_t` 大小 | 32-bit | 原生 64-bit |

---

## Step 7.0 — KD240 環境確認

🔲 待執行

```bash
# 本機 scp 到 KD240
scp check_kv260_env.sh <user>@<kd240-ip>:~

# KD240 上執行
bash check_kv260_env.sh
```

腳本位置：`check_kv260_env.sh`（專案根目錄）

確認項目：gcc、libexpat、FPGA Manager sysfs、/dev/mem、TrustZone iomem。
各項 PASS/FAIL 對應應對方式見 `check_kv260_env.sh` 與之前討論。

---

## Step 7.1 — Vivado：建立 AXI GPIO test design（xck24）

🔲 待建立

需要一個有 AXI-Lite mapped IP 的 KD240 bitstream，用於驗證 load/list/read/write 完整流程。

### Block Design 組成

```
zynq_ultra_ps_e_0 ──M_AXI_HPM0_FPD──→ AXI SmartConnect ──→ axi_gpio_0
proc_sys_reset_0
```

### 操作步驟

1. **新建專案**：RTL Project，Part 選 `xck24-ubva530-2LV`（或 Board 選 KD240）
2. **建 Block Design**：IP Integrator → Create Block Design
3. **加 IP**：
   - `Zynq UltraScale+ MPSoC`，Run Block Automation
   - `AXI GPIO`，Width = 8，單 channel
   - `Processor System Reset`
4. **連線**：Run Connection Automation → 全勾
5. **產生**：Generate Block Design → Create HDL Wrapper
6. **合成**：Run Synthesis → Implementation → Generate Bitstream
7. **匯出**：
   - `.bit`：`<project>.runs/impl_1/<wrapper>.bit`
   - `.hwh`：`<project>.gen/sources_1/bd/<name>/hw_handoff/<name>.hwh`
8. 存到 `testing_data/kd240_gpio/`

### 預期 AXI GPIO 暫存器

| Offset | 名稱 | 說明 |
|--------|------|------|
| `0x00` | GPIO_DATA | 讀寫資料 |
| `0x04` | GPIO_TRI | 方向（0=output, 1=input） |

驗證方式：寫 `0x04` 設為 output（`0x00`），寫 `0x00` 任意值，readback 確認。

---

## Step 7.2 — 程式碼調整（64-bit 移植）

🔲 待調整

不加 `ZYNQ7000` 定義重新編譯，觸發 `config.h` 的 64-bit 路徑。

```bash
# KD240 上（不加 PLATFORM=Z2）
make
```

### 需確認的改動點

| 項目 | 現狀 | 是否需改 |
|------|------|---------|
| `fpga_addr_t` = `uint64_t` | `config.h` 已有 | 不需改，重新編譯即生效 |
| `FPGA_ADDR_FMT` 使用 `PRIx64` | `config.h` 已有 | 不需改 |
| `mmap` 的 `off_t` | 加 `-D_FILE_OFFSET_BITS=64` 保險 | Makefile 確認 |
| printf 格式字串 | 各模組使用 `FPGA_ADDR_FMT` | 確認無直接用 `%x` 印位址 |

---

## Step 7.3 — KD240 端對端驗證

🔲 待驗證

```bash
# scp 編譯好的 binary 和測試資料到 KD240
scp overlay testing_data/kd240_gpio/* <user>@<kd240-ip>:~/fpga-overlay-driver/

# KD240 上執行
sudo ./overlay load testing_data/kd240_gpio/<design>.bit testing_data/kd240_gpio/<design>.hwh
./overlay list testing_data/kd240_gpio/<design>.hwh
sudo ./overlay write testing_data/kd240_gpio/<design>.hwh axi_gpio_0 0x04 0x00
sudo ./overlay write testing_data/kd240_gpio/<design>.hwh axi_gpio_0 0x00 0xAB
sudo ./overlay read  testing_data/kd240_gpio/<design>.hwh axi_gpio_0 0x00
# 預期讀回 0x000000AB
```

---

## ⚠️ 移植注意事項

1. **`off_t`**：編譯加 `-D_FILE_OFFSET_BITS=64`，確保 `mmap` 不截斷 64-bit 位址。
2. **printf 格式**：使用 `config.h` 的 `FPGA_ADDR_FMT`，不直接用 `%x` 或 `%lx`。
3. **Bitstream 不共用**：xck24 與 xck26 bitstream 不兼容，Phase 9 需重新合成。
4. **TrustZone**：AXI Peripheral 位址（通常 `0xA000_0000` 以上）屬 non-secure，`/dev/mem` 可直接存取。

---

## 相關文件

- `check_kv260_env.sh` — 環境確認腳本
- `include/config.h` — 平台切換（fpga_addr_t）
- `docs/phase8.md` — DMA 支援（下一步）
- `docs/phase9.md` — KV260 最終移植
