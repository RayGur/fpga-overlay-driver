# Phase 4 — HWH 解析（hwh_parser）

> 狀態：🔲 待開始

---

## 目標

解析 Vivado 輸出的 `.hwh` XML 檔，取得各 IP Core 的 base address 與 high address，供後續 MMIO 操作使用。

---

## .hwh 格式說明

```xml
<MODULE FULLNAME="design_1/axi_gpio_0"
        VLNV="xilinx.com:ip:axi_gpio:2.0">
  <REGISTERS>
    <PROPERTY NAME="BASEADDR" VALUE="0x41200000"/>
    <PROPERTY NAME="HIGHADDR" VALUE="0x4120FFFF"/>
  </REGISTERS>
</MODULE>
```

每個 `MODULE` 代表一個 IP Core，我們只需要：
- `FULLNAME` 最後一段作為 IP 名稱（`axi_gpio_0`）
- `BASEADDR` 與 `HIGHADDR`

---

## Step 4.1 — 設計 API

🔲 待實作

### 資料結構

```c
// hwh_parser.h
typedef struct {
    char        name[IP_NAME_LEN];   // e.g. "axi_gpio_0"
    fpga_addr_t base_addr;
    fpga_addr_t high_addr;
} ip_core_t;

typedef struct {
    ip_core_t cores[MAX_IP_CORES];
    int       count;
} hwh_design_t;
```

### 公開 API

```c
int  hwh_parse(const char *hwh_path, hwh_design_t *design);
int  hwh_find_core(const hwh_design_t *design, const char *name, ip_core_t *out);
void hwh_print(const hwh_design_t *design);
```

---

## Step 4.2 — 實作 XML 解析

🔲 待實作

### 解析策略選擇

| 方案 | 優點 | 缺點 |
|------|------|------|
| libexpat（SAX） | 標準、快 | 需確認板子上有安裝 |
| 手刻 strstr 掃描 | 零相依 | 脆弱，格式變動即壞 |
| mxml | 輕量 | 需額外安裝 |

**建議：先確認板子上 `libexpat` 是否可用（`dpkg -l libexpat1-dev`），可用則優先選擇。**

### 若使用 libexpat

```c
#include <expat.h>

// start_handler() 處理 <MODULE> 和 <PROPERTY>
// 用狀態機追蹤目前是否在感興趣的 MODULE 內
```

### 若手刻掃描

```c
// 逐行掃描，遇到 FULLNAME= 取名稱
// 遇到 BASEADDR= / HIGHADDR= 取位址
// 遇到 </MODULE> 結束一筆記錄
```

---

## Step 4.3 — 驗證

🔲 待驗證

```bash
gcc -Wall -o test_hwh test/test_hwh_parser.c src/hwh_parser.c
./test_hwh design.hwh
```

### 預期輸出

```
Parsed 3 IP cores:
  axi_gpio_0  base=0x41200000  high=0x4120FFFF
  axi_timer_0 base=0x42800000  high=0x4280FFFF
  my_dsp_core base=0x43C00000  high=0x43C0FFFF
```

---

## 相關文件

- `include/config.h` — `MAX_IP_CORES`、`IP_NAME_LEN`、`fpga_addr_t`
- `docs/phase5.md` — MMIO 使用 hwh_parser 的輸出
