# Phase 9 — 移植至 KV260（最終目標板）

> 狀態：🔲 待開始（Phase 8 DMA 驗證完成後進行）
> 目標板：KV260（xck26，Cortex-A53，64-bit，Ubuntu 22.04）

---

## 目標

將在 KD240 驗證完畢的工具（含 DMA 支援）移植至 KV260，進行最終部署驗證。

---

## KD240 vs KV260 差異

| 項目 | KD240 | KV260 |
|------|-------|-------|
| 晶片 | XCK24 | XCK26 |
| 架構 | Cortex-A53，64-bit | Cortex-A53，64-bit |
| OS | Ubuntu 22.04 | Ubuntu 22.04 |
| Bitstream | xck24 專用 | xck26 專用，需重新合成 |
| 工具 code | 不需改動 | 不需改動（同架構） |

---

## Step 9.1 — KV260 環境確認

🔲 待執行

```bash
bash check_kv260_env.sh
```

預期全 PASS（與 KD240 相同環境）。

---

## Step 9.2 — 取得 KV260 專用 bitstream

🔲 待取得

- Vivado 中將 KD240 設計的 Board 換成 KV260（xck26），重新合成
- 或取得學長提供的 KV260 `.bit` + `.hwh`

---

## Step 9.3 — KV260 端對端驗證

🔲 待驗證

重跑 Phase 7.3 + Phase 8.4 的完整驗證流程，確認與 KD240 結果一致。

---

## 相關文件

- `docs/phase7.md` — AXI-Lite 移植
- `docs/phase8.md` — DMA 支援
