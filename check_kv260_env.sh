#!/bin/bash
# KV260 環境確認腳本 — Phase 7 Step 7.0
# 用法：scp 到 KV260 後執行 bash check_kv260_env.sh

PASS="[PASS]"
FAIL="[FAIL]"
WARN="[WARN]"

echo "========================================"
echo " KV260 Environment Check — Phase 7"
echo "========================================"
echo ""

# ── 1. 系統資訊 ──────────────────────────────
echo "── 1. System Info ──"
uname -a
cat /etc/os-release 2>/dev/null || cat /etc/petalinux-version 2>/dev/null
echo ""

# ── 2. 編譯工具 ──────────────────────────────
echo "── 2. Toolchain ──"
if which gcc &>/dev/null; then
    echo "$PASS gcc: $(gcc --version | head -1)"
else
    echo "$FAIL gcc: not found — need cross-compile on host"
fi

if which make &>/dev/null; then
    echo "$PASS make: $(make --version | head -1)"
else
    echo "$FAIL make: not found"
fi
echo ""

# ── 3. libexpat ───────────────────────────────
echo "── 3. libexpat (required by hwh_parser) ──"
LIB=$(find /usr/lib /lib -name "libexpat*" 2>/dev/null)
HDR=$(find /usr/include -name "expat.h" 2>/dev/null)

if [ -n "$LIB" ]; then
    echo "$PASS libexpat library: $LIB"
else
    echo "$FAIL libexpat library: not found — hwh_parser needs rebuild or parser rewrite"
fi

if [ -n "$HDR" ]; then
    echo "$PASS expat.h header: $HDR"
else
    echo "$WARN expat.h header: not found — needed only for compilation"
fi
echo ""

# ── 4. FPGA Manager sysfs ─────────────────────
echo "── 4. FPGA Manager ──"
if [ -d /sys/class/fpga_manager/fpga0 ]; then
    echo "$PASS /sys/class/fpga_manager/fpga0 exists"
    echo "      state: $(cat /sys/class/fpga_manager/fpga0/state 2>/dev/null)"
else
    echo "$FAIL /sys/class/fpga_manager/fpga0: not found"
fi

if [ -d /lib/firmware ]; then
    echo "$PASS /lib/firmware exists"
    # 測試寫入權限
    if sudo touch /lib/firmware/.write_test 2>/dev/null; then
        sudo rm /lib/firmware/.write_test
        echo "$PASS /lib/firmware writable (sudo)"
    else
        echo "$WARN /lib/firmware not writable — fpga_load needs sudo"
    fi
else
    echo "$FAIL /lib/firmware: not found"
fi
echo ""

# ── 5. /dev/mem ───────────────────────────────
echo "── 5. /dev/mem (required by mmio) ──"
if [ -e /dev/mem ]; then
    echo "$PASS /dev/mem exists: $(ls -la /dev/mem)"
    if sudo dd if=/dev/mem of=/dev/null bs=4 count=1 &>/dev/null; then
        echo "$PASS /dev/mem readable (sudo)"
    else
        echo "$FAIL /dev/mem read failed — TrustZone or permission issue"
    fi
else
    echo "$FAIL /dev/mem: not found"
fi
echo ""

# ── 6. TrustZone / iomem ─────────────────────
echo "── 6. /proc/iomem (TrustZone check) ──"
grep -i "pl\|fpga\|reserved\|trust\|secure" /proc/iomem 2>/dev/null | head -20
echo ""

echo "========================================"
echo " Done. Paste output back to Claude."
echo "========================================"
