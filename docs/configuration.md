# Athrill configuration

Athrill has three configuration surfaces:

1. command-line options select the run mode and input files;
2. the memory configuration passed with `-m` describes the guest address map;
3. the device configuration passed with `-d` supplies core, target, and external-device parameters.

This page describes the CPU-independent Athrill core. Target-specific parameters are documented by the
[STM32 target](https://github.com/hakoniwalab/athrill-target-stm32/blob/main/docs/configuration.md),
[RISC-V target](https://github.com/hakoniwalab/athrill-target-riscv/blob/main/docs/configuration.md), and the other target repositories.
Values below are the current behavior of the source, including behavior retained for older targets.

## Command line

Syntax (options must precede the load file):

```text
athrill3 [options] <load-file>
```

Short-option arguments may be joined to the option or supplied as the next argument. Long-option arguments may be
joined with `=` or supplied as the next argument. `--` ends option parsing. The first non-option is loaded; a load
file is always required.

| Option | Meaning, values, default, and dependencies | Example |
| --- | --- | --- |
| `-c <cores>` | Decimal number of guest cores. If absent or zero, uses the target's compiled `CPU_CONFIG_CORE_NUM`. Do not exceed the target's compiled core capacity. | `-c 1` |
| `-i` | Interactive debugger mode. Without it, Athrill runs in background mode. It cannot be combined with `--hakoniwa`. | `-i` |
| `-r` | In `-i` mode, use the UDP remote debugger instead of standard input/output. It has no effect without `-i`. The two ports are configurable below. | `-i -r` |
| `-g <port>` | Start the GDB Remote Serial Protocol server on localhost at the given TCP port. Requires a target built with `ATHRILL_ENABLE_GDB_RSP=ON` (RH850F1x, STM32 and RISC-V on POSIX hosts; Windows is unsupported). Without the feature, `-g` is rejected. Implies interactive mode: cannot be combined with `--hakoniwa`; `-t` has no effect and `-r` is ignored. Supports one core only; `-c` values above 1 are rejected. | `-g 1234` |
| `-t <clocks>` | Stop after this many virtual CPU clocks in background mode. The stored default is zero: an ordinary background run therefore exits at clock zero, while Hakoniwa converts zero to an unlimited run. `-1` is accepted and becomes the maximum unsigned value, which is effectively unlimited. Interactive mode does not install this limit. | `-t 100000` |
| `-b` | Treat the load file as raw binary and copy it at guest address zero. Without it, the file must be ELF and symbols are loaded. The memory map must cover the bytes either way. | `-b` |
| `-m <file>` | Read the memory configuration described below. The parser permits omission, but an ELF or binary still needs mapped memory that covers what is loaded. | `-m memory.txt` |
| `-d <file>` | Read the device configuration described below. If absent, all parameters take their code defaults. | `-d device_config.txt` |
| `-p <file>` | Read the legacy inter-emulator FIFO configuration. The file must contain exactly one `TX <fifo-name>` line and one `RX <fifo-name>` line; both named FIFOs must already exist beside the configuration file. | `-p fifo_config.txt` |
| `--hakoniwa <asset>` | Register this background run as a Hakoniwa polling asset. Requires a build with `ATHRILL_ENABLE_HAKONIWA=ON`; incompatible with `-i`. With no positive `-t`, the run is unbounded. | `--hakoniwa athrill` |
| `--pdudef <file>` | Hakoniwa PDU definition. Used only with `--hakoniwa`; default is `config/hakoniwa/athrill-pdudef.json` as resolved from the process working directory. | `--pdudef config/hakoniwa/athrill-pdudef.json` |
| `--delta-usec <usec>` | Positive decimal conductor step in microseconds; default `1000`. Used when Athrill owns the conductor. | `--delta-usec 1000` |
| `--max-delay-usec <usec>` | Positive decimal conductor maximum delay in microseconds; default `20000`. Used when Athrill owns the conductor. | `--max-delay-usec 20000` |
| `--disable-conductor` | Do not create or advance a conductor; an external Hakoniwa conductor must do so. | `--disable-conductor` |
| `--real-sleep-msec <msec>` | In Hakoniwa mode, after each interval of this many milliseconds of simulated time, sleep for the same wall-clock duration. Default `0` disables it. | `--real-sleep-msec 10` |

`-t` accepts a signed decimal string and stores it as an unsigned 64-bit value; apart from the useful `-1` case,
use non-negative values. The three Hakoniwa time options use the same parser; `--delta-usec` and
`--max-delay-usec` additionally reject zero.

### GDB remote debugging

Connect to a running server with `gdb-multiarch`:

```text
gdb-multiarch
(gdb) set osabi none
(gdb) file <ELF>
(gdb) target remote :1234
```

The RSP server only binds to localhost. GDB `kill` exits Athrill; `detach` leaves it paused and waiting for another
connection. A new connection clears breakpoints from the previous session. Memory reads use Athrill's normal bus
read path, so reading memory-mapped peripheral registers can have device-specific side effects. Multi-core and
Windows hosts are not supported.

Targets that implement GDB register access must enable `ATHRILL_ENABLE_GDB_RSP` and provide the adapter functions
declared in `src/debugger/rsp/gdb_rsp_target.h`. A target may also provide `dbg_target_gdb_target_xml()`, a
self-contained GDB target description listing its registers in the order of its register image. The server then
advertises `qXfer:features:read+` and sends it, so GDB and LLDB show every described register (FPU, system
registers, CSRs) without a client-side file. Without it the server behaves as before and the debugger uses its
default layout. Addresses (memory, breakpoints, `c`/`s` with an address and `dbg_target_gdb_set_pc`) have the
target's width (`AthrillAddrType`): 64 bits on a 64-bit target.

## Memory configuration (`-m`)

Each non-empty line is comma- or whitespace-separated. A blank line ends the file, so keep all entries contiguous.
Addresses must be hexadecimal tokens and sizes are decimal KiB. The syntax is:

```text
ROM, <hex-address>, <size-kib>[, <flags>]
RAM, <hex-address>, <size-kib>[, <flags>]
MMAP, <hex-address>, <host-file>[, <flags>]
MALLOC, <hex-address>, <size-kib>[, <flags>]
DEV, <hex-address>, <shared-library>
```

`X` in the optional flags marks a ROM, RAM, MMAP, or MALLOC region executable. `V` makes ELF segments use their
virtual addresses instead of their physical addresses. The parser examines the first two flag characters, so use
`X`, `V`, `XV`, or `VX`.

| Type | Current behavior | Example line |
| --- | --- | --- |
| `ROM` | Allocates read-only guest memory of the given size. ELF loading may initialize it. | `ROM, 0x08000000, 2048, X` |
| `RAM` | Allocates writable guest memory of the given size. | `RAM, 0x20000000, 512` |
| `MMAP` | On POSIX hosts, maps a writable host file as RAM. The region is the file length rounded up to 8 KiB, with an 8 KiB minimum. Athrill does not extend the file: pre-size it to that rounded length before the guest accesses it. Unsupported hosts warn and skip the line. | `MMAP, 0x60000000, state.bin` |
| `MALLOC` | POSIX-host RAM backed by 1 MiB allocation units. The configured size must be a multiple of 10240 KiB (10 MiB); unsupported hosts warn and skip it. | `MALLOC, 0x70000000, 10240` |
| `DEV` | With `ATHRILL_ENABLE_EXDEV=ON`, opens an Athrill external-device library, validates its exported header, and maps the size declared by that device. If external devices are disabled, the type is skipped as unknown. | `DEV, 0x60000000, libhako_sensor_device.so` |

The file and library tokens cannot contain whitespace, commas, or colons. An MMAP string beginning with an
environment reference such as `{PWD}/state.bin` is expanded to the variable value plus the suffix; a DEV library
path is used literally. The same expansion applies to values retrieved as strings from the device configuration.

### `athrill_device_func_call`

An external-device build looks up the ELF symbol `athrill_device_func_call`; it is no longer tied to a hard-coded
address. Map the section containing that 32-bit variable as RAM. For the traditional linker placement, the memory
line is `RAM, 0xDEAD0000, 1`, and the C declaration is
`static unsigned int athrill_device_func_call __attribute__((section(".athrill_device_section")));`.
Writing a nonzero value makes the core process the request and then clear the variable. A value equal to an MMAP
base toggles that mapping's file lock; other values go to the Athrill syscall device on POSIX hosts.

## Device configuration (`-d`)

The ordinary syntax is one key and one value per line:

```text
<key> <value>
```

`DEVICE_CONFIG_HOST_FUNCTION` is the one three-token form. Decimal integers, lowercase `0x` hexadecimal integers,
and strings are distinct token types: a key read with `cpuemu_get_devcfg_value` needs decimal, and a key read with
`cpuemu_get_devcfg_value_hex` needs hexadecimal. Strings cannot contain whitespace, commas, or colons. There are no
comments, and the first blank line ends the file. At most 128 entries are stored. Repeated keys are preserved; the
ordinary getters return the first compatible entry.

### Execution and timing

| Key | Meaning and accepted values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEVICE_CPU_FREQ` | Virtual CPU frequency in MHz. It converts clocks to simulated microseconds; use a positive decimal value. | `100`. Targets may also have peripheral-specific fallback frequencies, so normally set this explicitly to the target frequency. | `DEVICE_CPU_FREQ 216` |
| `DEBUG_FUNC_ENABLE_SKIP_CLOCK` | `1` lets a halted CPU jump to the next scheduled device event; any other value disables clock skipping. | `0`. Idle-function detection also requires exactly `1`. | `DEBUG_FUNC_ENABLE_SKIP_CLOCK 1` |
| `DEVICE_CONFIG_IDLE_FUNCTION` | Guest function symbol treated as idle: at its entry, a target-specific safety check can halt the core as if it waited for an interrupt. `none` disables it. | Target-defined function or none. STM32 defaults to `up_idle`; RISC-V has no symbol default. Requires clock skipping, ELF symbols, and target support. | `DEVICE_CONFIG_IDLE_FUNCTION up_idle` |
| `DEVICE_CONFIG_SPIN_HALT` | Nonzero treats a target-recognized branch to itself as an interruptible halt. | `0`; requires target spin-halt support. | `DEVICE_CONFIG_SPIN_HALT 1` |
| `DEBUG_FUNC_RESET_PC` | Hexadecimal initial PC installed on every configured core before execution. | ELF entry address. A target reset sequence may subsequently replace it, as STM32 does from its vector table. | `DEBUG_FUNC_RESET_PC 0x40000200` |
| `DEBUG_FUNC_MAX_BATCH_CLOCKS` | Maximum instructions/clocks executed between device ticks in the fast background loop. Valid useful range is `1` through the target cap. | Target cap; `0` or a value above the cap also selects the cap. Current STM32 and RISC-V caps are 1024; the core fallback is 1. | `DEBUG_FUNC_MAX_BATCH_CLOCKS 64` |
| `DEBUG_FUNC_REAL_TIME` | Nonzero prevents simulated time from running ahead of wall time, checked every simulated millisecond and after a skip. | `0`. Uses `DEVICE_CPU_FREQ`; external input can then arrive at different virtual times. | `DEBUG_FUNC_REAL_TIME 1` |
| `DEBUG_FUNC_ENABLE_SYNC_TIME` | On POSIX, keep clock skipping at most this many microseconds ahead: a skip longer than the value sleeps off the excess. | `0`; only used when a clock skip occurs. Windows ignores it. | `DEBUG_FUNC_ENABLE_SYNC_TIME 1000` |
| `DEBUG_FUNC_SHOW_SKIP_TIME` | Nonzero prints each skip interval and elapsed wall time. | `0`; only used when a clock skip occurs. Windows ignores it. | `DEBUG_FUNC_SHOW_SKIP_TIME 1` |

### Debugger, symbols, and logging

| Key | Meaning and accepted values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEBUG_FUNC_ENABLE_BT` | `1` enables stack-frame collection for backtraces; every other value disables it. | `1`; meaningful when debugger callbacks run. | `DEBUG_FUNC_ENABLE_BT 0` |
| `DEBUG_FUNC_ENABLE_FT` | `1` enables the per-core function-entry ring; every other value disables function tracing. | `1`. | `DEBUG_FUNC_ENABLE_FT 0` |
| `DEBUG_FUNC_ENABLE_PROF` | `1` enables function call/time profiling; every other value disables it. | `1`. | `DEBUG_FUNC_ENABLE_PROF 0` |
| `DEBUG_FUNC_ENABLE_WATCH` | `1` enables data-watch and access-context collection; every other value disables it. | `1`. | `DEBUG_FUNC_ENABLE_WATCH 0` |
| `DEBUG_FUNC_FT_LOG_SIZE` | Decimal entries allocated for each core's function trace ring. Use a positive value. | `1024`. | `DEBUG_FUNC_FT_LOG_SIZE 4096` |
| `DEBUG_FUNC_DISABLE_DEBUG_DATA_TYPE` | Nonzero skips building DWARF variable-type information. | `0`; applies only to usable DWARF in ELF32 files. | `DEBUG_FUNC_DISABLE_DEBUG_DATA_TYPE 1` |
| `DISABLE_SYMBOL_UNDERSCORE` | Nonzero keeps ELF symbol names unchanged. Zero drops the first character, for toolchains that prefix symbols with `_`. | `0`. Modern ELF targets generally set `1`. | `DISABLE_SYMBOL_UNDERSCORE 1` |
| `DEBUG_FUNC_OPLOG` | File containing debugger commands to replay at interactive startup. The file is opened `a+`; successful later commands are appended. | Absent: no replay or recording. Requires `-i`. | `DEBUG_FUNC_OPLOG debugger.ops` |
| `DEBUG_FUNC_REMOTE_ATHRILL_LISTEN_PORT_NO` | Decimal UDP port on which remote interactive Athrill listens. | Target constant, `49152` in current targets; requires `-i -r`. | `DEBUG_FUNC_REMOTE_ATHRILL_LISTEN_PORT_NO 49152` |
| `DEBUG_FUNC_REMOTE_CLIENT_LISTEN_PORT_NO` | Decimal UDP port to which remote interactive responses are sent. | Target constant, `49153` in current targets; requires `-i -r`. | `DEBUG_FUNC_REMOTE_CLIENT_LISTEN_PORT_NO 49153` |

### Host functions

Host functions replace selected guest function calls with functions exported by one host library. The target must
implement the host-call ABI. A handled host call counts as one guest instruction; a refused call falls back to the
guest implementation.

| Key | Meaning and accepted values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEVICE_CONFIG_HOST_FUNCTION_LIBRARY` | Path of the host-function shared library. | Absent: host functions disabled. Required when any mapping is present. | `DEVICE_CONFIG_HOST_FUNCTION_LIBRARY libathrill_host_functions.so` |
| `DEVICE_CONFIG_HOST_FUNCTION` | Repeatable three-token mapping: guest ELF function symbol, then exported host symbol. Up to 64 mappings. | No mappings. Requires the library, ELF symbols, and target support. | `DEVICE_CONFIG_HOST_FUNCTION memcpy athrill_host_memcpy` |

## Legacy parameters still read by the core

These names remain functional for older V850/EV3, debugger, mROS, and serial-FIFO configurations. New targets need
not implement the corresponding devices. The misspelling `FILFO` is part of the serial-FIFO interface and must be
written exactly as shown.

### Compatibility alias

| Key | Legacy use and values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEVICE_CONFIG_ARM_SPIN_HALT` | Older Arm-only name for `DEVICE_CONFIG_SPIN_HALT`. It is read only when the generic key is absent. | `0`; requires target spin-halt support. Prefer the generic key in new configurations. | `DEVICE_CONFIG_ARM_SPIN_HALT 1` |

### ROM overrides and editor paths

| Key | Legacy use and values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEBUG_ROM_DEFINE_NUM` | Number of guest global variables to override after ELF loading. | Absent: no overrides. Requires an ELF symbol table. | `DEBUG_ROM_DEFINE_NUM 1` |
| `DEBUG_ROM_DEFINE_<n>` | For zero-based `n`, `symbol=decimal-value`. Only 1-, 2-, and 4-byte globals are written. | Required for every index below the count. | `DEBUG_ROM_DEFINE_0 test_mode=2` |
| `EDITOR_SEARCH_PATH_NUM` | Number of fallback source roots used by the legacy source viewer. | Absent: no fallback roots. | `EDITOR_SEARCH_PATH_NUM 1` |
| `EDITOR_SEARCH_PATH_<n>` | Zero-based source root appended before the DWARF directory and filename. | Required for every index below the count. | `EDITOR_SEARCH_PATH_0 /work/firmware` |

### Serial FIFO

The serial FIFO is used by legacy targets and by the EV3 syscall bridge.

| Key | Legacy use and values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEVICE_CONFIG_SERIAL_FILFO_BASE_ADDR` | Hex base of the memory-mapped serial-FIFO registers. | Absent: the entire serial-FIFO device is disabled. | `DEVICE_CONFIG_SERIAL_FILFO_BASE_ADDR 0x09000000` |
| `DEVICE_CONFIG_SERIAL_FIFO_FD` | Decimal interrupt assertion delay in device ticks. | `1024`; used only when the base is configured. | `DEVICE_CONFIG_SERIAL_FIFO_FD 32` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_SIZE` | Decimal byte capacity of channel `n`; channels are zero based and target-limited. | Absent: that channel is disabled. | `DEVICE_CONFIG_SERIAL_FILFO_0_SIZE 1024` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_RD_INTNO` | Guest interrupt number for receive. | Required when the channel size is present. | `DEVICE_CONFIG_SERIAL_FILFO_0_RD_INTNO 10` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_WR_INTNO` | Guest interrupt number for transmit. | Required when the channel size is present. | `DEVICE_CONFIG_SERIAL_FILFO_0_WR_INTNO 11` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_RD_INTOFF` | Receive occupancy threshold that raises the interrupt. | Required when the channel size is present. | `DEVICE_CONFIG_SERIAL_FILFO_0_RD_INTOFF 1` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_WR_INTOFF` | Transmit free/occupancy threshold used by the device. | Required when the channel size is present. | `DEVICE_CONFIG_SERIAL_FILFO_0_WR_INTOFF 1` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_DISABLE_CPUIO` | Nonzero disables guest-side reads and writes for the channel. | `0`. | `DEVICE_CONFIG_SERIAL_FILFO_0_DISABLE_CPUIO 0` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_EXDEV_ENABLE` | Nonzero assigns transport to an external device instead of the built-in FIFO threads. | `0`. | `DEVICE_CONFIG_SERIAL_FILFO_0_EXDEV_ENABLE 1` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_RD_FIFO` | Receive named-FIFO path opened by the built-in channel thread. | Required when the channel is enabled and EXDEV is zero. | `DEVICE_CONFIG_SERIAL_FILFO_0_RD_FIFO serial-rx.fifo` |
| `DEVICE_CONFIG_SERIAL_FILFO_<n>_WR_FIFO` | Transmit named-FIFO path opened by the built-in channel thread. | Required when the channel is enabled and EXDEV is zero. | `DEVICE_CONFIG_SERIAL_FILFO_0_WR_FIFO serial-tx.fifo` |

### EV3 syscall device and virtual filesystem

| Key | Legacy use and values | Default and dependency | Example line |
| --- | --- | --- | --- |
| `DEVICE_CONFIG_EV3_CH0_SERIAL_ID` | Serial-FIFO channel used for the EV3 UART port. | Absent: unavailable; requires a build with `ENABLE_EXTERNAL_BT_SERIAL`. | `DEVICE_CONFIG_EV3_CH0_SERIAL_ID 0` |
| `DEVICE_CONFIG_EV3_CH1_SERIAL_ID` | Serial-FIFO channel used for the EV3 Bluetooth port. | Absent: unavailable; requires `ENABLE_EXTERNAL_BT_SERIAL`. | `DEVICE_CONFIG_EV3_CH1_SERIAL_ID 1` |
| `DEVICE_CONFIG_VIRTFS_TOP` | Host directory used as the EV3 virtual-filesystem root. | `_ev3rtfs`; created on first use. | `DEVICE_CONFIG_VIRTFS_TOP ev3-files` |
| `DEVICE_CONFIG_UART_BASENAME` | Base name for the EV3 UART pipe pair. | `__ev3rt_uart`. | `DEVICE_CONFIG_UART_BASENAME ev3-uart` |
| `DEVICE_CONFIG_BT_BASENAME` | Base name for the EV3 Bluetooth pipe pair. | `__ev3rt_bt`. | `DEVICE_CONFIG_BT_BASENAME ev3-bt` |

### mROS device

These are read only when the legacy mROS peripheral is built and started.

| Key | Legacy use and values | Default | Example line |
| --- | --- | --- | --- |
| `DEBUG_FUNC_MROS_MASTER_IPADDR` | ROS master IPv4 address or host string accepted by the mROS socket layer. | `0.0.0.0` | `DEBUG_FUNC_MROS_MASTER_IPADDR 192.0.2.10` |
| `DEBUG_FUNC_MROS_NODE_IPADDR` | Address advertised by the Athrill node. | `127.0.0.1` | `DEBUG_FUNC_MROS_NODE_IPADDR 127.0.0.1` |
| `DEBUG_FUNC_MROS_SLAVE_PORT_NO` | Decimal XML-RPC slave port. | `11411` | `DEBUG_FUNC_MROS_SLAVE_PORT_NO 11411` |
| `DEBUG_FUNC_MROS_PUBLISHER_PORT_NO` | Decimal publisher TCP port. | `11511` | `DEBUG_FUNC_MROS_PUBLISHER_PORT_NO 11511` |
| `DEBUG_FUNC_MROS_NODE_NAME` | ROS node name. | `athrill_node` | `DEBUG_FUNC_MROS_NODE_NAME athrill_node` |
