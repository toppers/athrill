athrill
=======

Athrill is a CPU emulator. 
Athrill was developed to easily execute and debug embedded programs on virtual microcomputer. 
You can evaluate bare metal programs or embedded control programs running on real-time OS. 
Athrill at the moment supports V850 CPU instructions.

# Table of Contents
-----------------
  * [Requirements](#requirements)
  * [Install](#install)
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
This repository holds the CPU-independent core. The `athrill2` executable is built with CMake from a target repository, which pulls in this core through `cmake/AthrillCore.cmake`. For the V850E2M target:

```
git clone https://github.com/toppers/athrill.git
git clone https://github.com/toppers/athrill-target-v850e2m.git
cmake -S athrill-target-v850e2m -B athrill-target-v850e2m/build -DCMAKE_BUILD_TYPE=Release
cmake --build athrill-target-v850e2m/build
```

The target finds this repository as the sibling `../athrill` checkout. To use a checkout somewhere else, add `-DATHRILL_SOURCE_DIR=<path to athrill>` to the first `cmake` command.

Running `athrill2` without arguments displays its usage:

	Usage:athrill -m<memory config file> [OPTION]... <load_file>
	-i                             : execute on the interaction mode. if -i is not set, execute on the background mode.
	-r                             : execute on the remote mode. this option is valid on the interaction mode.
	-t<timeout>                    : set program end time using <timeout> clocks. this option is valid on the background mode.
	-m<memory config file>         : set athrill memory configuration. rom, ram region is configured on your system.


# License
-------
Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).
