# Phase 6 — CLI 整合

> 狀態：🔲 待開始

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

🔲 待實作

### 引數解析結構

```c
typedef enum { CMD_LOAD, CMD_LIST, CMD_READ, CMD_WRITE, CMD_UNKNOWN } cmd_t;

cmd_t parse_command(const char *s);
void  print_usage(const char *prog);
```

---

## Step 6.2 — 整合模組到 main.c

🔲 待實作

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

🔲 待實作

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

🔲 待驗證

```bash
sudo ./overlay load design.bit design.hwh
# [✓] Converted design.bit → design.bin
# [✓] Parsed 3 IP cores from design.hwh
# [✓] Bitstream loaded, FPGA state: operating

./overlay list design.hwh
# IP Cores:
#   axi_gpio_0  @ 0x41200000
#   axi_timer_0 @ 0x42800000

sudo ./overlay write design.hwh axi_gpio_0 0x00 0xFF
# [✓] Written 0xFF to axi_gpio_0 @ offset 0x00

sudo ./overlay read design.hwh axi_gpio_0 0x00
# 0x000000FF
```

---

## 相關文件

- `docs/phase2.md` ~ `phase5.md` — 各模組實作細節
