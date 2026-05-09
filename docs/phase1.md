# Phase 1 — 環境確認

> 狀態：✅ 完成

---

## 目標

在開始寫任何 C code 之前，先用 shell 手動確認板子上的 Linux FPGA Manager 環境一切正常，避免後續花時間 debug 其實是環境問題而非程式問題。

---

## Step 1.1 — 確認 FPGA Manager sysfs 路徑存在

✅ 完成

### 操作

```bash
ls /sys/class/fpga_manager/
# 預期看到 fpga0/

ls /sys/class/fpga_manager/fpga0/
# 預期看到 flags  firmware  state  ...
```

### 預期輸出

```
fpga0
flags  firmware  name  state  ...
```

### 注意事項

- 若 `fpga_manager` 目錄不存在，表示 kernel 未載入 `xilinx-fpga-manager` driver
- PYNQ-Z2 的 Ubuntu image 預設應已包含此 driver

---

## Step 1.2 — 手動載入測試 bitstream，確認 state → operating

✅ 完成

### 操作

```bash
# 1. 準備一份已知好的 .bin（先用 PYNQ 產生，或用 Vivado 產生後手動 byte-swap）
cp design.bin /lib/firmware/

# 2. 設定 flags = 0（full bitstream 模式）
echo 0 | sudo tee /sys/class/fpga_manager/fpga0/flags

# 3. 觸發載入
echo "design.bin" | sudo tee /sys/class/fpga_manager/fpga0/firmware

# 4. 確認狀態
cat /sys/class/fpga_manager/fpga0/state
# 預期：operating
```

### 預期輸出

```
operating
```

### 注意事項

- `firmware` 寫入後 FPGA Manager 會立即觸發載入，**此操作會覆蓋目前 PL 上的 bitstream**
- 若 state 為 `fw_loaded` 或其他非 `operating` 值，表示 bitstream 格式有問題
- `.bin` 必須先放到 `/lib/firmware/`，FPGA Manager 不接受絕對路徑

---

## Step 1.3 — 確認 `/dev/mem` 可存取

✅ 完成

### 操作

```bash
# 確認設備存在且可存取
ls -la /dev/mem

# 以 root 或 kmem group 身份執行簡單讀取測試
sudo python3 -c "
f = open('/dev/mem', 'rb')
f.seek(0x41200000)   # axi_gpio_0 的預期位址
data = f.read(4)
print(hex(int.from_bytes(data, 'little')))
f.close()
"
```

### 預期輸出

```
crw-r----- 1 root kmem 1, 1 ... /dev/mem
0x0   (或其他暫存器初始值)
```

### 注意事項

- PYNQ-Z2 上通常需要 `sudo` 或加入 `kmem` group
- 此步驟確認的是 `/dev/mem` 的**權限**，實際 mmap 在 Phase 5 才實作
- KV260 移植時需注意 TrustZone 可能限制部分記憶體區域

---

## 完成標準

Phase 1 完成的判斷條件：

1. `cat /sys/class/fpga_manager/fpga0/state` 在手動載入後回傳 `operating`
2. `/dev/mem` 可以 root 身份開啟並讀取目標位址

---

## 相關文件

- `docs/decisions.md` — 為何選擇 Layer 2 C Userspace
- `include/config.h` — FPGA Manager sysfs 路徑定義
