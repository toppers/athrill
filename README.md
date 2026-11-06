athrill
=======

Athrill is a CPU emulator. 
Athrill was developed to easily execute and debug embedded programs on virtual microcomputer. 
You can evaluate bare metal programs or embedded control programs running on real-time OS. 

Athrill started in the TOPPERS project with the V850. Version 3
(`athrill3`, from v3.0.0) is a CPU-independent core in C++, with one
repository per target; the C generation ends with v1.2.1.
Athrill supports four instruction sets:

| Target repository | Instruction set | What runs on it |
| --- | --- | --- |
| [athrill-target-v850e2m](https://github.com/toppers/athrill-target-v850e2m) | V850E2M | TOPPERS real-time OS programs |
| [athrill-target-rh850f1x](https://github.com/toppers/athrill-target-rh850f1x) | RH850 (F1x) | programs for the RH850/F1x microcontrollers |
| [athrill-target-riscv](https://github.com/hakoniwalab/athrill-target-riscv) | RISC-V (RV64IMFC) | PX4 on the AIFoundry Erbium board, a board target in upstream PX4 |
| [athrill-target-stm32](https://github.com/hakoniwalab/athrill-target-stm32) | Armv7-M (Cortex-M7) | the unmodified PX4 firmware of Pixhawk 4 (`px4_fmu-v5`), flown from QGroundControl |

Athrill runs on a virtual clock (one clock per instruction, idle time
skipped), so a run repeats exactly from the same inputs, and it can run in
lockstep with the [Hakoniwa](https://github.com/toppers/hakoniwa)
co-simulation environment (`--hakoniwa`).

Command-line options, memory maps, and core device parameters are described in the
[configuration reference](docs/configuration.md).

# Table of Contents
-----------------
  * [Requirements](#requirements)
  * [Install](#install)
  * [GDB remote debugging](#gdb-remote-debugging)
  * [License](#license)

# Requirements
------------
Athrill requires the following to run:

  * OS
    * macOS (arm64)
    * Linux (x64, arm64)
    * Windows 11 (x64, Visual Studio 2022)
  * Build tools
    * CMake 3.16 or later
    * A C11 and C++17 compiler (AppleClang, GCC, Clang, or MSVC)

Athrill uses the following editors for source debugging:

  * Editor
    * [Linux]
      * geany
    * [Windows]
      * Sakura Editor

# Install
-----
This repository holds the CPU-independent core. The `athrill3` executable is built with CMake from a target repository, which pulls in this core through `cmake/AthrillCore.cmake`. For the V850E2M target:

```
git clone https://github.com/toppers/athrill.git
git clone https://github.com/toppers/athrill-target-v850e2m.git
cmake -S athrill-target-v850e2m -B athrill-target-v850e2m/build -DCMAKE_BUILD_TYPE=Release
cmake --build athrill-target-v850e2m/build
```

The target finds this repository as the sibling `../athrill` checkout. To use a checkout somewhere else, add `-DATHRILL_SOURCE_DIR=<path to athrill>` to the first `cmake` command.

## GDB remote debugging

The `-g<port>` option starts a GDB Remote Serial Protocol server. It is available only when the target enables `ATHRILL_ENABLE_GDB_RSP` at configure time and provides the register access functions declared in `src/debugger/rsp/gdb_rsp_target.h`. Currently, the RH850F1x target enables this option; other targets leave it disabled by default. A build without this support rejects `-g` with an explanatory error and does not show it in the usage output.

The server listens on localhost and supports one core (`-c1`). Start Athrill with the target's memory configuration and ELF file, then connect with `gdb-multiarch`:

```sh
athrill3 -c1 -m memory.txt -g1234 app.elf
gdb-multiarch
(gdb) set osabi none
(gdb) file app.elf
(gdb) target remote :1234
```

GDB `kill` exits Athrill. GDB `detach` leaves Athrill paused while it waits for another connection. A new connection clears breakpoints from the previous session. Memory reads use the normal bus read path, so reading memory-mapped peripheral registers can have device-specific side effects.

Running `athrill3` without arguments displays its usage:

	Usage:athrill -m<memory config file> [OPTION]... <load_file>
	-i                             : execute on the interaction mode. if -i is not set, execute on the background mode.
	-r                             : execute on the remote mode. this option is valid on the interaction mode.
	-t<timeout>                    : set program end time using <timeout> clocks. this option is valid on the background mode.
	-m<memory config file>         : set athrill memory configuration. rom, ram region is configured on your system.


# License
-------
Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).
