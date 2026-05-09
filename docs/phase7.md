# Phase 7 — 移植至 KV260

> 狀態：🔲 待開始（在 PYNQ-Z2 完整驗證後進行）

---

## 目標

將在 PYNQ-Z2 驗證完畢的工具移植至 Kria KV260（Zynq Ultrascale+，64-bit）。

---

## 主要差異

| 項目 | PYNQ-Z2 | KV260 |
|------|---------|-------|
| ARM 架構 | 32-bit Cortex-A9 | 64-bit Cortex-A53 |
| AXI Address 寬度 | `uint32_t` | `uint64_t` |
| FPGA Manager sysfs | 路徑相同 | 路徑相同 |
| `.hwh` 格式 | 相同 | 相同 |
| Bitstream | XC7Z020 專用 | XCK26 專用，需重新合成 |
| TrustZone | 無 | 有，部分記憶體受限 |
| `off_t` 大小 | 32-bit（需 `_FILE_OFFSET_BITS=64`） | 原生 64-bit |

---

## Step 7.1 — 重新編譯（不加 ZYNQ7000 定義）

🔲 待移植

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

🔲 待確認

KV260 上 TrustZone 將記憶體分為 secure/non-secure world。部分位址範圍（特別是 DDR 的某些區段）可能受限，`/dev/mem` mmap 會回傳 `EPERM`。

### 確認方式

```bash
# 查看 TrustZone 設定（若有 xil-trustzone 工具）
cat /proc/iomem | grep -i "reserved\|trust"

# 嘗試 mmap 目標 IP base addr，若失敗印 errno
```

### 應對策略

- AXI Peripheral 位址（通常 `0xA000_0000` 以上）一般屬於 non-secure，可直接存取
- 若受限，可考慮改用 UIO（Userspace I/O）driver

---

## Step 7.3 — 重新合成 bitstream 並端對端驗證

🔲 待驗證

1. 在 Vivado 中為 KV260（XCK26）重新合成相同設計
2. SCP `.bit` 與 `.hwh` 到 KV260
3. 執行完整 `./overlay load` → `read` / `write` 流程
4. 確認結果與 PYNQ-Z2 一致

---

## ⚠️ 移植注意事項

1. **`off_t` 問題**：若 KV260 上的 glibc 為 32-bit `off_t`（罕見，但需確認），mmio_open 中的 `mmap(..., (off_t)m->phys_base)` 可能截斷位址。編譯時加 `-D_FILE_OFFSET_BITS=64` 保險。

2. **printf 格式字串**：`fpga_addr_t` 在 64-bit 為 `uint64_t`，使用 `PRIx64`（`<inttypes.h>`）而非直接 `%lX`，確保跨平台正確。`config.h` 的 `FPGA_ADDR_FMT` 已考慮此點。

3. **Bitstream 不可共用**：XC7Z020 與 XCK26 的 bitstream 完全不兼容，務必重新合成。

---

## 相關文件

- `include/config.h` — 平台切換的唯一入口
- `docs/plan.md` — 整體移植計畫
