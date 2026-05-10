# Phase 6 — CLI 整合

> 狀態：✅ 完成（板子驗證通過）

---

## 目標

將 bit2bin、hwh_parser、fpga_load、mmio 整合進 `main.c`，提供統一的命令列介面。

---

## CLI 介面設計

```bash
./overlay load  <design.bit> <design.hwh>    # 轉換並載入 bitstream
./overlay list  <design.hwh>                 # 列出所有 IP Core 與位址
./overlay read  <design.hwh> <ip_name> <offset_hex>
./overlay write <design.hwh> <ip_name> <offset_hex> <value_hex>
```

---

## Step 6.1 — 設計 CLI 介面

✅ 完成（`main.c` skeleton 已含 `parse_command` 邏輯與 `print_usage`）

### 引數解析結構

```c
typedef enum { CMD_LOAD, CMD_LIST, CMD_READ, CMD_WRITE, CMD_UNKNOWN } cmd_t;

cmd_t parse_command(const char *s);
void  print_usage(const char *prog);
```

---

## Step 6.2 — 整合模組到 main.c

✅ 完成

### load 流程

```
parse argv → bit2bin (convert_bit_to_bin) → hwh_parse
           → fpga_load_bitstream → 印結果
```

### read/write 流程

```
hwh_parse → hwh_find_core(ip_name) → mmio_open(base_addr)
          → mmio_read/write(offset) → 印結果 → mmio_close
```

---

## Step 6.3 — 錯誤處理規格

✅ 完成

| 錯誤 | 回傳碼 | 訊息格式 |
|------|--------|---------|
| 錯誤引數數量 | 1 | print_usage() |
| 檔案不存在 | 2 | `Error: cannot open <path>` |
| bit2bin 失敗 | 3 | `Error: bitstream conversion failed` |
| FPGA 載入失敗 | 4 | `Error: FPGA load failed (state: <state>)` |
| IP Core 不存在 | 5 | `Error: IP core '<name>' not found` |
| mmap 失敗 | 6 | `Error: mmio_open failed: <strerror>` |

---

## Step 6.4 — 完整流程 end-to-end 測試

✅ 完成（PYNQ-Z2，cordic design，2026-05-11）

```bash
$ sudo ./overlay load testing_data/cordic/design_1.bit testing_data/cordic/design_1.hwh
[✓] Converted testing_data/cordic/design_1.bit → testing_data/cordic/design_1.bin
[✓] Parsed 1 IP cores from testing_data/cordic/design_1.hwh
[✓] Bitstream loaded, FPGA state: operating

$ ./overlay list testing_data/cordic/design_1.hwh
IP Cores (1):
  cordic_1                         base=0x40000000  high=0x4000FFFF

$ sudo ./overlay read testing_data/cordic/design_1.hwh cordic_1 0x00
0x00000004

$ sudo ./overlay write testing_data/cordic/design_1.hwh cordic_1 0x00 0x00000001
[✓] Wrote 0x00000001 to cordic_1 offset 0x0

$ sudo ./overlay read testing_data/cordic/design_1.hwh cordic_1 0x00
0x0000000E
```

**readback 說明**：`0x00` 是 HLS IP 的 `AP_CTRL` register。寫 `0x1`（ap_start）
後 CORDIC 執行完成，回報 `0xE`（bits 3:1 = ap_done + ap_idle + ap_ready 全高），
代表 write+read 均正常，IP 有真實回應。

---

## 相關文件

- `docs/phase2.md` ~ `phase5.md` — 各模組實作細節
