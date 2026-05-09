# Phase 2 — Bitstream 轉換（bit2bin）

> 狀態：✅ 完成（含板子驗證）

---

## 目標

將 Vivado 產生的 `.bit` 檔轉換為 Linux FPGA Manager 可接受的 `.bin` 格式。

### 為什麼需要轉換？

`.bit` 檔包含一段 variable-length header（含設計名稱、日期、目標元件等資訊），FPGA Manager 只接受純 bitstream payload，且 byte order 必須對調。

### 轉換步驟

1. 解析 `.bit` TLV header，以 tag `0x65` 定位 payload 起點與長度
2. 對 payload 做 **32-bit byte-swap**（每 4 bytes 內部反轉）
3. 輸出 `.bin`

---

## Step 2.1 — 設計 API，建立 skeleton

✅ 完成

### 產出

**`src/bit2bin.h`**

```c
#ifndef BIT2BIN_H
#define BIT2BIN_H

int bit_find_payload(const uint8_t *buf, size_t file_len,
                     size_t *out_offset, size_t *out_length);
int convert_bit_to_bin(const char *input_path, const char *output_path);

#endif
```

**`src/bit2bin.c`** — skeleton 含 TODO 標註

---

## Step 2.2 — 實作 bit_find_payload() 與 convert_bit_to_bin()

✅ 完成

### 核心邏輯

#### `.bit` TLV 格式

```
[2B]  initial field length
[nB]  initial field data
[2B]  unknown field (通常 0x0001)
loop:
  [1B]  tag
  if tag == 0x65:
    [4B]  payload length (big-endian) ← 權威來源
    [nB]  bitstream payload
    break
  else:
    [2B]  field length
    [nB]  field data (ASCII，含設計名稱、日期等)
```

#### `bit_find_payload()`

TLV 走讀邏輯，找到 tag `0x65` 後：
- 讀 4-byte big-endian payload length
- 驗證 `off + payload_len == file_len`（長度吻合）
- 驗證 `payload_len % 4 == 0`（word-align）
- 回傳 `out_offset`、`out_length`

#### `swap32()`

```c
static uint32_t swap32(uint32_t x)
{
    return ((x & 0xFF000000u) >> 24) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x000000FFu) << 24);
}
```

以 `memcpy` 讀入 word（不假設 host byte order），swap 後 `fwrite` 輸出，對 LE 和 BE host 皆正確。

#### `convert_bit_to_bin()` 流程

```
fopen(input_path, "rb") → 讀入全部到 buf
  ↓
bit_find_payload(buf) → 取得 payload_off、payload_len
  ↓
fopen(output_path, "wb")
  ↓
for i in 0..words: memcpy → swap32 → fwrite
  ↓
return 0
```

### 錯誤處理

| 情況 | 回傳 | 行為 |
|------|------|------|
| `input_path` 不存在 | -1 | `strerror(errno)` |
| TLV 解析失敗（欄位截斷） | -1 | 具體位置的 stderr 訊息 |
| tag `0x65` 不存在 | -1 | 提示可能非 `.bit` 格式 |
| `off + payload_len != file_len` | -1 | 印出實際差異 byte 數 |
| payload 非 4 的倍數 | -1 | 視為 corrupt（不 truncate） |
| 寫入 `output_path` 失敗 | -1 | `strerror(errno)` |

### 關於 config.h 中的常數

`BIT_SYNC_WORD` 與 `BIT_HEADER_MAX` 在最終實作中未被使用（改為 TLV 走讀）。可移除或保留作文件用途。

---

## Step 2.3 — 板子驗證

✅ 完成

### 操作步驟

```bash
# 1. SCP 傳到板子
scp src/bit2bin.c src/bit2bin.h include/config.h test/test_bit2bin.c xilinx@192.168.2.99:~/bit2bin_test/

# 2. SSH 進板子編譯
ssh xilinx@192.168.2.99
cd ~/bit2bin_test
gcc -Wall -o test_bit2bin test_bit2bin.c bit2bin.c

# 3. 執行測試
./test_bit2bin design.bit

# 4. 確認輸出（選擇性）
# 用 PYNQ Python 產生一份 reference .bin，比對 md5
md5sum /tmp/test_output.bin reference.bin
```

### 預期輸出

```
Testing bit2bin: design.bit → /tmp/test_output.bin
[PASS] Conversion succeeded. Check /tmp/test_output.bin
```

若 md5 與 PYNQ 產生的 reference 一致，表示轉換完全正確。

### 注意事項

- 驗證完畢後才進入 Phase 3，避免帶著錯誤的 `.bin` 去 debug FPGA Manager
- `test_bit2bin.c` 目前寫死 `bin_path = "/tmp/test_output.bin"`，不會影響 `/lib/firmware`

---

## 相關文件

- `include/config.h` — `BIT_SYNC_WORD`、`BIT_HEADER_MAX`（實際未使用，可移除）
- `docs/decisions.md` — DEC-03：TLV 解析策略決策
- `docs/phase3.md` — 下一步：fpga_load
