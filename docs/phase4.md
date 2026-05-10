# Phase 4 — HWH 解析（hwh_parser）

> 狀態：✅ 完成，板子驗證通過

---

## 目標

解析 Vivado 輸出的 `.hwh` XML 檔，取得各 IP Core 的 base address 與 high address，供後續 MMIO 操作使用。

---

## 實際 .hwh 格式

`docs/plan.md` 描述的格式（`<PROPERTY NAME="BASEADDR">`）與實際 Vivado 輸出不符。  
實際格式中地址資訊在 **`<MEMRANGE>`** 元素，位於 PS MODULE 的 `<MEMRANGES>` 區塊內：

```xml
<MEMRANGE ADDRESSBLOCK="Reg"
          INSTANCE="cordic_1"
          BASEVALUE="0x40000000"
          HIGHVALUE="0x4000FFFF"
          MEMTYPE="REGISTER"
          MASTERBUSINTERFACE="M_AXI_GP0"
          SLAVEBUSINTERFACE="s_axi_CTRL"/>
```

### 兩種 MEMTYPE

| MEMTYPE | 說明 | 是否採用 |
|---------|------|----------|
| `REGISTER` | PL IP 的 AXI-Lite 暫存器空間 | ✅ 採用 |
| `MEMORY` | DDR / QSPI 記憶體 | ❌ 略過 |

---

## 解析策略

使用 **libexpat SAX**（事件驅動），不建構 DOM 樹，記憶體用量固定。

### 過濾規則（參考 PYNQ 做法）

1. 元素名稱必須是 `MEMRANGE`
2. `MEMTYPE` 必須是 `REGISTER`
3. 同一 `INSTANCE` 只取第一筆（一個 IP 可能有多個 slave interface）

### 狀態機

SAX handler 無需狀態機——`MEMRANGE` 是 self-closing 元素，所有資訊在 start handler 的屬性中一次取得。

---

## Step 4.1 — API 設計 ✅

```c
// src/hwh_parser.h

typedef struct {
    char        name[IP_NAME_LEN];   /* e.g. "cordic_1"      */
    fpga_addr_t base_addr;           /* BASEVALUE from .hwh   */
    fpga_addr_t high_addr;           /* HIGHVALUE from .hwh   */
} ip_core_t;

typedef struct {
    ip_core_t   cores[MAX_IP_CORES];
    int         count;
} ip_map_t;

int              parse_hwh       (const char *hwh_path, ip_map_t *map);
const ip_core_t *find_ip_by_name (const ip_map_t *map, const char *name);
void             print_ip_map    (const ip_map_t *map);
```

---

## Step 4.2 — 實作 ✅

```c
// src/hwh_parser.c（核心邏輯）
static void start_handler(void *user_data, const XML_Char *el,
                          const XML_Char **attr)
{
    if (strcmp(el, "MEMRANGE") != 0) return;

    // 取 INSTANCE / BASEVALUE / HIGHVALUE / MEMTYPE
    // 過濾 MEMTYPE != "REGISTER"
    // 跳過已存在的 INSTANCE（dedup）
    // 寫入 map->cores[]
}
```

編譯：`gcc ... -lexpat`

---

## Step 4.3 — 驗證 ✅

```bash
make PLATFORM=Z2 test/test_hwh_parser
./test/test_hwh_parser testing_data/cordic/design_1.hwh
./test/test_hwh_parser testing_data/hls_tiling/hls_tiling.hwh
./test/test_hwh_parser testing_data/sa/sa.hwh
```

### 驗證結果

```
=== cordic ===
[PASS] Parsed 1 IP cores:
  cordic_1                         base=0x40000000  high=0x4000FFFF
[PASS] find_ip_by_name('cordic_1') succeeded
[PASS] find_ip_by_name('nonexistent_ip') correctly returned NULL

=== hls_tiling ===
[PASS] Parsed 1 IP cores:
  gemm_tiling_0                    base=0x40000000  high=0x4000FFFF

=== sa ===
[PASS] Parsed 4 IP cores:
  MM2S_0                           base=0x40000000  high=0x4000FFFF
  MM2S_1                           base=0x40010000  high=0x4001FFFF
  S2MM_0                           base=0x40020000  high=0x4002FFFF
  SystolicArray_v1_0_0             base=0x40030000  high=0x40030FFF
```

---

## 相關文件

- `include/config.h` — `MAX_IP_CORES`、`IP_NAME_LEN`、`fpga_addr_t`
- `docs/phase5.md` — MMIO 使用 hwh_parser 的輸出
- `docs/decisions.md` — DEC-06（libexpat vs 手刻）
