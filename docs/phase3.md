# Phase 3 — FPGA 載入（fpga_load）

> 狀態：✅ 完成（實作 + 板子驗證通過）

---

## 目標

以 C 程式操作 Linux FPGA Manager sysfs 介面，讓轉換好的 `.bin` 實際載入到 FPGA PL。

### Linux FPGA Manager 載入流程

```
cp <bin_file> /lib/firmware/<name>.bin
  ↓
echo 0 > /sys/class/fpga_manager/fpga0/flags       (full bitstream mode)
  ↓
echo <name>.bin > /sys/class/fpga_manager/fpga0/firmware  (觸發載入)
  ↓
poll /sys/class/fpga_manager/fpga0/state until "operating"
```

---

## Step 3.1 — Skeleton 已存在

✅ 完成（`src/fpga_load.c` / `fpga_load.h` 已建立）

現有 skeleton 包含：
- `write_sysfs()` — 內部 helper，寫字串到 sysfs file ✅
- `read_sysfs()` — 內部 helper，從 sysfs file 讀一行 ✅
- `copy_to_firmware()` — ✅ 完成
- `fpga_load_bitstream()` — ✅ 完成
- `fpga_get_state()` — 已實作（呼叫 `read_sysfs`）✅

---

## Step 3.2 — 實作 copy_to_firmware()

🔲 待實作

### 功能

將 `.bin` 複製到 `/lib/firmware/`，讓 FPGA Manager kernel driver 能找到它。

### 介面

```c
static int copy_to_firmware(const char *bin_path, char *fw_name, int fw_name_len);
```

### 實作要點

```c
// 1. 從 bin_path 取出 basename
const char *base = strrchr(bin_path, '/');
base = base ? base + 1 : bin_path;

// 2. 拼出 dest 路徑
char dest[512];
snprintf(dest, sizeof(dest), "%s/%s", FIRMWARE_DIR, base);

// 3. 用 fopen/fread/fwrite 複製（比 system("cp") 更嚴謹）
FILE *src_f = fopen(bin_path, "rb");
FILE *dst_f = fopen(dest, "wb");
// ... fread/fwrite loop ...

// 4. 回存 basename
snprintf(fw_name, fw_name_len, "%s", base);
```

### 注意事項

- ⚠️ **需要 root 權限**（`/lib/firmware/` 通常只有 root 可寫）
- 若 `bin_path` 與 `dest` 相同（使用者直接把 .bin 放進 `/lib/firmware`），應正常處理（skip copy 或 overwrite）
- 緩衝區大小：`MAX_BIN_FILENAME 256`（來自 `config.h`）

---

## Step 3.3 — 實作 fpga_load_bitstream()

🔲 待實作

### 介面

```c
int fpga_load_bitstream(const char *bin_path);
```

### 完整流程

```c
int fpga_load_bitstream(const char *bin_path)
{
    char fw_name[MAX_BIN_FILENAME];

    // 1. 複製到 /lib/firmware/
    if (copy_to_firmware(bin_path, fw_name, sizeof(fw_name)) < 0)
        return -1;

    // 2. 設定 flags = 0（full bitstream）
    if (write_sysfs(FPGA_MANAGER_FLAGS, "0\n") < 0)
        return -1;

    // 3. 寫入 firmware 檔名，觸發載入
    if (write_sysfs(FPGA_MANAGER_FIRMWARE, fw_name) < 0)
        return -1;

    // 4. Poll state，最多等 N 次 × 100ms
    #define POLL_MAX    50     // 5 秒
    #define POLL_SLEEP  100000 // 100ms in microseconds
    char state[64];
    for (int i = 0; i < POLL_MAX; i++) {
        usleep(POLL_SLEEP);
        if (fpga_get_state(state, sizeof(state)) == 0 &&
            strcmp(state, "operating") == 0)
            return 0;
    }

    fprintf(stderr, "[fpga_load] timeout waiting for 'operating', last state: %s\n", state);
    return -1;
}
```

### Poll 策略

| 參數 | 值 | 說明 |
|------|-----|------|
| 間隔 | 100ms | `usleep(100000)` |
| 最大次數 | 50 | 總等待上限 5 秒 |
| 成功條件 | `state == "operating"` | |
| 逾時行為 | return -1 + 印最後 state | |

---

## Step 3.4 — 端對端驗證

✅ 完成（cordic.bit 在 PYNQ-Z2 驗證通過，state → operating）

### 操作步驟

```bash
# 編譯（包含 bit2bin + fpga_load）
gcc -Wall -o fpga_load_test \
    test_fpga_load.c \
    bit2bin.c fpga_load.c

# 執行（需 sudo）
sudo ./fpga_load_test design.bit

# 確認
cat /sys/class/fpga_manager/fpga0/state
# 預期：operating
```

### 預期輸出

```
[fpga_load] Copying design.bin → /lib/firmware/design.bin
[fpga_load] Writing flags = 0
[fpga_load] Triggering firmware load: design.bin
[fpga_load] State: operating  ✓
```

### ⚠️ 警告：不可逆操作

> **載入新 bitstream 會立即覆蓋 PL 上的現有設計。**
> 若板子上有正在運行的應用程式依賴目前的 PL 功能，請先確認安全再執行。

---

## 相關文件

- `include/config.h` — `FPGA_MANAGER_*` 路徑、`FIRMWARE_DIR`、`MAX_BIN_FILENAME`
- `docs/phase2.md` — bit2bin 輸出的 `.bin` 即為本 Phase 的輸入
- `docs/decisions.md` — poll 策略決策
