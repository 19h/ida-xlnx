# Xilinx Boot Image Loader for IDA Pro

This is a native IDA Pro loader module for the **Xilinx Zynq / Versal / Spartan Boot Image (BOOT.BIN / .pdi)** formats. It dynamically detects and parses Boot Headers, Image Header Tables, and Partition Headers across multiple generations of AMD/Xilinx System-on-Chips, cleanly mapping out Bootloaders (FSBL/PLM) and subsequent executable partitions into their intended memory locations.

Written in modern **C++23**, the loader relies on the [idax](https://github.com/19h/idax) wrapper to safely interface with the IDA SDK. The parsing engine is completely decoupled from the IDA SDK, allowing high-performance and extensive headless unit testing.

## Supported Architectures

* **Zynq 7000 SoC** (`BOOT.BIN`)
* **Zynq UltraScale+ MPSoC** (`BOOT.BIN`)
* **Versal Adaptive SoC** (`.pdi`)
* **Spartan UltraScale+** (`.pdi`)
* **Versal AI Edge Series Gen 2 / Versal Prime Series Gen 2** (`.pdi`)

## Features

- **Format Recognition:** Automatically detects Xilinx image families via the `XNLX` (`0x584C4E58`) magic signature and specific width detection checks (at offsets `0x10` and `0x20` respectively).
- **Boot Header Parsing:** Extracts and prints detailed logging about the Boot Header, PLM (Platform Loader and Manager), PMC Data, and FSBL execution and load addresses.
- **Partition Layout & Mapping (Zynq):** Safely walks the nested Image Header Table to dynamically extract, map, and size all active partitions (such as PMUFW, FSBL, ATF, U-Boot, and baremetal applications).
- **PLM & Data Extraction (Versal/Spartan):** Maps the Platform Loader and Manager (PLM) directly to execution RAM (e.g. `0xF0280000`) and unpacks embedded Programmable Logic / PMC CDO data.
- **Dynamic Segment Naming:** Automatically unpacks `Image Name` strings encoded within the header to assign correct names to mapped IDA segments (`Zynq7007_miner.bit`, `u_boot.elf`, `FSBL.elf`, etc).
- **Entry Points Registration:** Automatically resolves and registers partition entry points, allowing IDA's auto-analysis to efficiently kick off on multiple firmware stages simultaneously.
- **Fallback Mechanism:** Implements intelligent fallback to direct FSBL extraction if the Image Header Table is stripped or corrupt.

## Requirements

- **IDA Pro 9.x**
- **idax** (C++23 IDA wrapper library - fetched automatically via CMake)
- **CMake 3.27+**
- **C++23** compatible compiler (Apple Clang 17+, GCC 13+, MSVC 19.38+)

## Build Instructions

This project includes a convenient `Makefile` wrapping the native CMake preset architecture.

### Quick Start

```bash
# Build the loader module in release mode (optimized)
make loader
```

The compiled loader module will be generated inside the `build-release-optimized` directory.

### Available Make Targets

- `make loader`: Build the loader in Release mode with aggressive optimizations (recommended).
- `make loader-debug`: Build the loader in Debug mode (preserves debug symbols for loader development).
- `make test`: Run headless C++ unit tests to verify parser structures and boundaries.
- `make clean`: Clean intermediate build artifacts, keeping CMake config caches.
- `make purge`: Wipe all build directories and reset the CMake environment.

## Tests

The underlying format parser is completely abstracted from the IDA runtime and thoroughly tested against raw buffer mocks, validating padding layouts (like the Zynq 7000 continuous arrays vs. MPSoC linked lists), boundary constraints, and string extraction schemes without booting IDA.

```bash
make test
```

## Installation & Usage

1. Move the compiled loader binary (e.g., `zynqmp_boot_image_loader.dylib` on macOS, `.dll` on Windows, or `.so` on Linux) into your IDA `loaders/` directory.
2. Open IDA Pro and load your `BOOT.BIN` or `.pdi` firmware image.
3. In the initial "Load a new file" dialog, choose the appropriate **Xilinx** option (e.g., *Xilinx Zynq 7000 Boot Image*, *Xilinx Zynq UltraScale+ Boot Image*, or *Xilinx Versal/Spartan PDI*).
4. Let the loader automatically orchestrate the segments and entry points, and begin your analysis!

## License

Available under the MIT License.
