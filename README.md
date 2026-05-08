# fpga-overlay-driver

PYNQ に依存しない Zynq FPGA bitstream ローダー / 不依賴 PYNQ 的 Bitstream 載入工具

A C userspace library to load Vivado `.bit` files and access PL registers on Zynq devices,
without requiring the PYNQ framework.

---

## Features

- Convert Vivado `.bit` → Linux FPGA Manager `.bin`
- Parse `.hwh` hardware description for IP Core address maps
- Load bitstream via Linux sysfs (`/sys/class/fpga_manager`)
- Read/write PL registers via `/dev/mem` + `mmap`
- Clean CLI interface

## Target Hardware

| Board     | SoC                     | ARM Core       | Status      |
|-----------|-------------------------|----------------|-------------|
| PYNQ-Z2   | Zynq-7000 XC7Z020       | Cortex-A9 32b  | Development |
| Kria KV260| Zynq Ultrascale+ XCK26  | Cortex-A53 64b | Planned     |

## Build

```bash
# On PYNQ-Z2 (native gcc)
make PLATFORM=Z2

# Default (Ultrascale+ / 64-bit)
make
```

## Usage

```bash
./overlay load  design.bit design.hwh        # convert & load bitstream
./overlay list  design.hwh                   # list IP cores and addresses
./overlay read  design.hwh axi_gpio_0 0x00   # read register
./overlay write design.hwh axi_gpio_0 0x00 0xFF  # write register
```

> **Note:** `fpga_load` and `mmio` require root privileges (or `kmem` group).

## Project Structure

```
src/
  main.c          CLI entry point
  bit2bin.c/h     .bit → .bin conversion
  hwh_parser.c/h  .hwh XML IP address map parser
  fpga_load.c/h   sysfs FPGA Manager interface
  mmio.c/h        /dev/mem mmap register access
include/
  config.h        Platform abstraction (32/64-bit address types)
test/
  test_bit2bin.c
  test_hwh_parser.c
docs/
  design.md       Design document
```

## Implementation Status

| Module        | Status         |
|---------------|---------------|
| bit2bin       | 🔲 Skeleton   |
| hwh_parser    | 🔲 Skeleton   |
| fpga_load     | 🔲 Skeleton   |
| mmio          | 🔲 Skeleton   |
| CLI           | 🔲 Skeleton   |
