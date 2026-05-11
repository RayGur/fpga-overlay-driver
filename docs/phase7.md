# Phase 7 — 移植至 KV260

> 狀態：🔲 待開始

---

## 目標

將在 PYNQ-Z2 驗證完畢的工具移植至 Kria KV260（Zynq Ultrascale+，64-bit，PetaLinux）。

---

## 主要差異

| 項目 | PYNQ-Z2 | KV260 |
|------|---------|-------|
| ARM 架構 | 32-bit Cortex-A9 | 64-bit Cortex-A53 |
| AXI Address 寬度 | `uint32_t` | `uint64_t` |
| OS | Ubuntu | PetaLinux（Yocto-based） |
| 套件管理 | apt | 需在 PetaLinux build 中加入 |
| Native gcc | 有 | 未確認 |
| libexpat | 有 | 未確認 |
| FPGA Manager sysfs | 路徑相同 | 預期相同，待確認 |
| `.hwh` 格式 | 相同 | 相同 |
| Bitstream | XC7Z020 專用 | XCK26 專用，需重新合成 |
| TrustZone | 無 | 有，部分記憶體受限 |
| `off_t` 大小 | 32-bit（需 `_FILE_OFFSET_BITS=64`） | 原生 64-bit |

---

## Step 7.0 — KV260 環境確認（前提條件）

🔲 待執行

**在開始任何實作前，先跑環境確認腳本：**

```bash
# 本機：scp 到 KV260
scp check_kv260_env.sh <user>@<kv260-ip>:~

# KV260 上執行
bash check_kv260_env.sh
```

腳本位置：`check_kv260_env.sh`（專案根目錄）

### 確認項目與可能問題

#### 2. 編譯工具（gcc / make）

| 結果 | 應對方式 |
|------|---------|
| `[PASS]` gcc 存在 | 維持原本 native compile 方式 |
| `[FAIL]` gcc 不存在 | 改為 cross-compile：主機安裝 `aarch64-linux-gnu-gcc`，Makefile 加 `CROSS_COMPILE` 變數，編好再 scp |

#### 3. libexpat

| 結果 | 應對方式 |
|------|---------|
| `[PASS]` library + header 都有 | 不需改動 |
| `[PASS]` library 有、header 無 | 只需動態連結，但 host cross-compile 需要另外取得 header |
| `[FAIL]` library 不存在 | **選項 A**：請學長在 PetaLinux build 裡加入 `expat`（`meta-petalinux` 有 recipe）；**選項 B**：將 `hwh_parser.c` 改寫為不依賴 libexpat 的 minimal XML scanner（只掃 MEMRANGE，不需完整 parser） |

#### 4. FPGA Manager sysfs

| 結果 | 應對方式 |
|------|---------|
| `[PASS]` 路徑存在、state 可讀 | 不需改動 |
| `[FAIL]` 路徑不存在 | 確認 KV260 kernel config 有啟用 `CONFIG_FPGA_MGR_ZYNQMP`；路徑可能為 `/sys/class/fpga_manager/fpga0` 以外的名字，查 `ls /sys/class/fpga_manager/` |

#### 5. /dev/mem

| 結果 | 應對方式 |
|------|---------|
| `[PASS]` 可讀 | 不需改動 |
| `[FAIL]` 讀取失敗 | TrustZone 限制，改用 UIO（Userspace I/O）driver 存取 PL registers；需確認目標 IP 的位址範圍是否在 non-secure 區域 |

#### 6. TrustZone / iomem

- AXI Peripheral 位址（通常 `0xA000_0000` 以上）一般屬於 non-secure，`/dev/mem` 可直接存取
- 若目標 IP 的 base address 落在 secure 區域，會收到 `EPERM`，需改用 UIO

---

## Step 7.1 — 重新編譯（不加 ZYNQ7000 定義）

🔲 待移植（依 Step 7.0 結果決定 native 或 cross-compile）

```bash
# PYNQ-Z2
make PLATFORM=Z2    # 定義 ZYNQ7000 → fpga_addr_t = uint32_t

# KV260（預設）
make                # 不定義 ZYNQ7000 → fpga_addr_t = uint64_t
```

### config.h 切換點

```c
#ifdef ZYNQ7000
    typedef uint32_t fpga_addr_t;
    #define FPGA_ADDR_FMT   "0x%08X"
#else
    typedef uint64_t fpga_addr_t;
    #define FPGA_ADDR_FMT   "0x%016lX"
#endif
```

**預期改動：僅 `config.h` 的 define，其餘 .c 不需修改。**

---

## Step 7.2 — TrustZone 確認

🔲 待確認（依 Step 7.0 結果）

### 應對策略

- AXI Peripheral 位址（通常 `0xA000_0000` 以上）一般屬於 non-secure，可直接存取
- 若受限，改用 UIO（Userspace I/O）driver

---

## Step 7.3 — 重新合成 bitstream 並端對端驗證

🔲 待驗證

1. 取得學長提供的 KV260 `.bit` + `.hwh`
2. SCP 到 KV260
3. 執行完整 `./overlay load` → `list` → `read` / `write` 流程
4. 確認結果與 PYNQ-Z2 一致

---

## ⚠️ 移植注意事項

1. **`off_t` 問題**：若 KV260 上的 glibc 為 32-bit `off_t`（罕見，但需確認），mmio_open 中的 `mmap(..., (off_t)m->phys_base)` 可能截斷位址。編譯時加 `-D_FILE_OFFSET_BITS=64` 保險。

2. **printf 格式字串**：`fpga_addr_t` 在 64-bit 為 `uint64_t`，使用 `PRIx64`（`<inttypes.h>`）而非直接 `%lX`，確保跨平台正確。`config.h` 的 `FPGA_ADDR_FMT` 已考慮此點。

3. **Bitstream 不可共用**：XC7Z020 與 XCK26 的 bitstream 完全不兼容，務必重新合成。

4. **PetaLinux 套件**：沒有 `apt`，所有需要的函式庫必須在 PetaLinux build 階段加入，或改寫程式碼移除該依賴。

---

## 相關文件

- `check_kv260_env.sh` — 環境確認腳本
- `include/config.h` — 平台切換的唯一入口
- `docs/plan.md` — 整體移植計畫
