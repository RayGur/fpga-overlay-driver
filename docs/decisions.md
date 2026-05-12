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

---

## DEC-08：DMA userspace 機制選用 u-dma-buf

**決策內容**
Phase 8 DMA buffer 分配採用 `u-dma-buf`（ikwzm/u-dma-buf），不用 dma-proxy 或 `/dev/mem` 直接存取。

**原因**
- KD240 確認 CMA 512MB 可用、kernel headers 完整，u-dma-buf 可在板子上 build
- `/dev/udmabuf`（Linux 標準）是 display buffer driver，用途不同，不可混用
- dma-proxy 需確認 kernel config，相依性較高
- u-dma-buf 透過 sysfs 直接提供實體位址，mmap 給 userspace 用，最乾淨

**替代方案**
- dma-proxy（Xilinx 官方）：功能相近，但需要額外確認 kernel 支援
- `/dev/mem` 直接 mmap CMA 區段：不需 module，但需手動指定實體位址，脆弱

**影響範圍**
- `src/dma.c`：buffer 分配與實體位址取得邏輯
- 板子部署：需先 build & insmod u-dma-buf.ko

---

## DEC-09：AXI DMA 採用 Scatter-Gather 模式

**決策內容**
Phase 8 DMA 實作採用 SG（Scatter-Gather）模式，不用 Simple 模式。

**原因**
- 學長建議，其 SPI 設計（16ch × 32-bit × 30kHz ≈ 1.92 MB/s）也採用 SG
- BCI 應用為連續串流，SG descriptor chain 更適合 pipeline 資料移動
- Simple 模式每次傳輸需 CPU 介入重設，無法高效處理連續資料流

**替代方案**
- Simple 模式：實作簡單，但不適合連續串流場景

**影響範圍**
- `src/dma.c`：需實作 BD（Buffer Descriptor）ring 管理
- Vivado block design：AXI DMA IP 需啟用 SG 選項

---

## DEC-10：DMA 等待策略：直接實作 interrupt（UIO）

**決策內容**
直接以 UIO interrupt 實作等待完成，不先做 polling 版本。

**原因**
- UIO interrupt 實作與 polling 複雜度相近（`read(uio_fd)` block 等通知，`write(uio_fd)` re-enable）
- KD240 確認 `uio_pdrv_genirq` 已載入，`/dev/uio*` 機制完整可用
- 從一開始就做好 error handling，比兩階段開發更省時

**替代方案**
- 先 polling 再改寫：多一個驗證迴圈，但 debug 較容易

**影響範圍**
- `src/dma.c`：等待邏輯用 `read(uio_fd)` / `write(uio_fd)`，不用輪詢 DMASR
- Vivado block design：AXI DMA interrupt 需接出並對應到 UIO device

---

## DEC-11：dma.h API 設計（高階，參考 PYNQ + libaxidma）

**決策內容**
採用高階 API，BD ring 管理完全封裝在 `dma.c` 內部，呼叫者只操作資料和 handle。

```c
// lifecycle
dma_t *dma_open(fpga_addr_t base, const char *uio_dev,
                size_t buf_size, int ring_size);
void   dma_close(dma_t *h);

// 雙向（loopback / BCI 主要用途）
int dma_transfer(dma_t *h, const void *tx, void *rx, size_t len);

// 單向（未來彈性）
int dma_send(dma_t *h, const void *data, size_t len);  // MM2S
int dma_recv(dma_t *h, void *buf,        size_t len);  // S2MM
```

**暫定參數**（待實測後可能調整）
- `buf_size` 預設 4096 bytes（≈ 2ms BCI 資料，page-aligned）
- `ring_size` 預設 4

**原因**
- 高階 API 隱藏 BD 細節，與 PYNQ `sendchannel.transfer()` 設計一致
- MM2S / S2MM 邏輯分開但不暴露給呼叫者，`dma_transfer()` 成對呼叫
- wait 封裝在函式內部，切換 interrupt 機制時 API 不變

**替代方案**
- 低階 API（Xilinx xaxidma 風格）：呼叫者管 BD，彈性高但複雜

**影響範圍**
- `include/dma.h`：公開 API 定義
- `src/dma.c`：BD ring、udmabuf、UIO interrupt 全部封裝於此
