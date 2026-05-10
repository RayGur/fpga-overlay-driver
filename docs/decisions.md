# docs/decisions.md — 重要決策紀錄

> 格式：決策 · 原因 · 替代方案 · 影響範圍

---

## DEC-01：採用 Layer 2 C Userspace，不做 Kernel Module

**決策內容**
以 C userspace library 實作所有功能，透過 `/dev/mem` 和 sysfs 操作硬體，不撰寫 kernel module。

**原因**
- 此工具定位為 lab 內部研究工具，不需要 kernel 級即時性或 DMA 支援
- Userspace debug 容易（gdb / printf），kernel module crash 會死整台機器
- 開發速度優先；Layer 2 可覆蓋此專案所有需求

**替代方案**
- Layer 1 Shell Script：更快，但無法整合成函式庫，錯誤處理弱
- Layer 3 Kernel Module：完整支援 DMA/中斷，但開發成本高 10 倍以上

**影響範圍**
- 整個專案架構
- 需要 root 權限執行（`/dev/mem` 存取、`/lib/firmware/` 寫入）

---

## DEC-02：平台抽象集中在 config.h，單一切換點

**決策內容**
所有平台差異（32/64-bit address type、格式字串）集中到 `include/config.h`，以 `#ifdef ZYNQ7000` 切換。其餘 `.c` 檔一律使用 `fpga_addr_t`，不直接用 `uint32_t` 或 `uint64_t`。

**原因**
- 後續移植 KV260 時，改動量降到最小（理想上只改 Makefile 的 PLATFORM 參數）
- 減少移植時的漏改風險

**替代方案**
- 各 `.c` 直接用 `uintptr_t`：可行，但語意不夠清晰
- 用 cmake / autoconf 自動偵測：過度工程，此專案規模不需要

**影響範圍**
- `include/config.h`（定義）
- 所有使用位址的 `.c` 檔（使用 `fpga_addr_t`）
- Makefile（控制是否定義 `ZYNQ7000`）

---

## DEC-03：採用 TLV header 解析定位 payload，不做 sync word 掃描

**決策內容**
`bit_find_payload()` 按照 `.bit` 檔的實際 TLV 格式逐欄位走讀，以 tag `0x65` 的 4-byte length 欄位作為 payload 長度的權威來源，直接算出 payload offset。不做 sync word byte-by-byte 掃描。

**實作結構（對應 PYNQ `parse_bit_header()` 邏輯）**
```
[2B initial field length] + [n B data]
[2B unknown field]
while tag != 0x65:
    tag [1B] → length [2B] → skip data
tag 0x65:
    payload_length [4B big-endian]  ← 權威長度
    payload starts here
```

驗證：以 `off + payload_len == file_len` 確認長度吻合，以 `payload_len % 4 == 0` 確認 word-align。

**原因**
- Vivado 保證 tag `0x65` 的 length 欄位精確等於 bitstream payload 的 byte 數，且一定是 4 的倍數
- Sync word `0xAA995566` 在 payload 內部也可能出現，byte-by-byte 掃描有誤判風險
- TLV 走讀比掃描更嚴謹，同時能順帶驗證 header 完整性

**Sync word 的實際處理方式**
Sync word 不需要特殊處理：它和其他 word 一樣經過 `swap32()`，結果在 `.bin` 中正確呈現供 FPGA Manager 讀取。

**替代方案（未採用）**
Byte-by-byte 掃描 sync word `0xFFFFFFAA`：實作簡單，但有誤判 payload 內容的風險，且依賴 sync word 的 big-endian 表示形式而非格式規格。

**影響範圍**
- `src/bit2bin.c`：`bit_find_payload()`（取代原設計的 `find_sync_offset()`）
- `include/config.h`：`BIT_SYNC_WORD` 與 `BIT_HEADER_MAX` 常數實際上未被使用（可移除或保留作文件用途）

---

## DEC-04：fpga_load 使用 poll + sleep，不用 inotify

**決策內容**
載入 bitstream 後，以 100ms 間隔 poll `fpga_manager/state`，最多等 5 秒，而非用 `inotify` 監聽 sysfs 變化事件。

**原因**
- sysfs 的 `inotify` 行為在不同 kernel 版本不一致，不可靠
- Poll 實作簡單，5 秒上限對 Zynq bitstream 載入時間（通常 < 1 秒）綽綽有餘
- 工具類應用不在意那 100ms 的延遲精度

**替代方案**
- `inotify`：更即時，但在 sysfs 上行為不可靠
- `select()` on sysfs fd：同樣不可靠

**影響範圍**
- `src/fpga_load.c` 的 `fpga_load_bitstream()`
- 需要 `#include <unistd.h>` 以使用 `usleep()`

---

## DEC-05：採用逐步驗證策略，每個模組獨立可測

**決策內容**
每個模組（bit2bin、fpga_load、hwh_parser、mmio）都有對應的獨立測試程式，在整合進 `main.c` 之前先單獨在板子上驗證。

**原因**
- 減少「一次踩多個坑」的風險
- 可以更精確地定位問題（是 bit2bin 的 `.bin` 格式錯，還是 FPGA Manager 設定問題？）
- 符合嵌入式開發的最佳實踐

**替代方案**
- 一次寫完再整合測試：速度快，但出錯難以隔離

**影響範圍**
- 開發順序（Phase 1 → 2 → 3 → ... 順序執行，不跳過）
- `test/` 目錄下的獨立測試程式

---

## DEC-06：hwh_parser 的 XML 解析策略

**決策內容**
使用 **libexpat SAX**，不做手刻字串掃描。

**原因**
- 板子已安裝 `libexpat1-dev` 2.2.5-3（`dpkg -l` 確認）
- SAX 解析無需建構 DOM 樹，記憶體用量固定
- 標準函式庫，邊界條件由 expat 處理，不怕 XML 屬性順序或空白差異

**替代方案（未採用）**
- 手刻 strstr 掃描：零相依，但 XML 屬性順序不保證，維護性差

**影響範圍**
- `src/hwh_parser.c`：使用 `XML_ParserCreate` / `XML_SetElementHandler`
- Makefile：`-lexpat`

---

## DEC-07：hwh_parser 掃描 MEMRANGE 而非 MODULE PARAMETER

**決策內容**
解析地址映射時，掃描 `<MEMRANGE MEMTYPE="REGISTER">` 元素，而非掃描各 IP `<MODULE>` 內的 `<PARAMETER NAME="*_BASEADDR">` 屬性。

**原因**
- 實際 HWH 中地址參數名帶有 IP 特定前綴（如 `C_S_AXI_CTRL_BASEADDR`），無法用固定名稱匹配
- `MEMRANGE` 的 `INSTANCE` / `BASEVALUE` / `HIGHVALUE` 屬性名稱固定，不隨 IP 類型變化
- `MEMRANGE` 語意是「PS 可見的 slave 地址映射」，正好是 Linux MMIO 需要的視角
- PYNQ 的 `hwh_parser.py` 也採用此策略

**過濾規則**
只保留 `MEMTYPE="REGISTER"` 的 MEMRANGE，排除 `MEMTYPE="MEMORY"`（DDR/QSPI）。

**影響範圍**
- `src/hwh_parser.c`：`start_handler()` 只處理 `MEMRANGE` 元素
