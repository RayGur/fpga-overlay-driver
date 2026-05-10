# CLAUDE.md

C userspace 工具，將 Vivado `.bit` 載入 Zynq FPGA，不依賴 PYNQ。

---

## 硬體環境

- **開發板**：PYNQ-Z2（XC7Z020，ARM Cortex-A9，32-bit）｜SSH：`pynq-z2`（192.168.2.99，user: xilinx）
- **目標板**：Kria KV260（XCK26，ARM Cortex-A53，64-bit）
- **編譯方式**：板子上 native gcc，不 cross-compile
- **本機路徑**：`C:\Users\ray93\EE\fpga-overlay-driver`
- **PYNQ 路徑**：`/home/xilinx/fpga-overlay-driver`
- **同步方式**：寫完檔案用 scp 同步到 PYNQ，測試和執行都在 PYNQ 上跑

## 開發規則

1. **位址型別永遠用 `fpga_addr_t`**，不要直接用 `uint32_t` / `uint64_t`。定義在 `include/config.h`。
2. **操作 `/lib/firmware/` 或寫 sysfs 之前要警告**，這些是不可逆操作（會覆蓋 FPGA bitstream）。
3. **每個模組獨立可測**，不要在模組未驗證前就整合進 `main.c`。
4. **不要用 `system("cp ...")`** 做 firmware 複製，用 fopen/fread/fwrite。
5. KV260 移植時注意 TrustZone 與 `off_t` 大小問題（詳見 `docs/phase7.md`）。
6. 每個 Step 開始前先說實作計畫，確認後再動手。
7. 遇到設計決策列出選項問我，不要自己決定。
8. 不可以自動 reboot；硬體不可逆操作前一定要問我確認。
9. token 快用完時主動提醒，先更新 CLAUDE.md 和 decisions.md。

## 版本控制

- Repo：https://github.com/RayGur/fpga-overlay-driver.git
- 功能開新 branch，完成一小塊就 commit
- merge 一律開 PR，push 前告訴我

## 目前進度

- ✅ Phase 1：環境確認（手動 shell 驗證 FPGA Manager 正常）
- ✅ Phase 2：`bit2bin.c` 實作完成，板子驗證通過
- ✅ Phase 3：`fpga_load.c` 實作完成，板子驗證通過（cordic.bit → operating）
- ✅ Phase 4：`hwh_parser.c` 實作完成，板子驗證通過（libexpat SAX，掃描 MEMRANGE）
- ✅ Phase 5：`mmio.c` 實作完成，板子驗證通過（open/mmap/read/write，write+readback 留待 Phase 6 完整驗證）
- 🔲 **下一步**：Phase 6 — CLI 整合（`main.c`，`load` / `list` / `read` / `write`）

## 關鍵文件索引

| 需要了解… | 看這裡 |
|-----------|--------|
| 整體架構與所有 Phase 清單 | `docs/plan.md` |
| bit2bin 模組 | `docs/phase2.md` |
| fpga_load 模組 | `docs/phase3.md` |
| hwh_parser | `docs/phase4.md` |
| mmio | `docs/phase5.md` |
| CLI 整合（下一步）| `docs/phase6.md` |
| CLI 整合 | `docs/phase6.md` |
| KV260 移植 | `docs/phase7.md` |
| 為什麼這樣設計 | `docs/decisions.md` |
| 平台常數、sysfs 路徑 | `include/config.h` |
