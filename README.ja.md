athrill
=======

Athrill は CPU エミュレータです. 
Athrill を使用すると，仮想マイコン上で組込みプログラムを簡単に実行/デバッグすることができます．
ベアメタルプログラムやリアルタイムOS上で動作する組込み制御プログラムを評価できます．
現時点の Athrill は V850 のCPU命令をサポートしています．

# Table of Contents
-----------------
  * [Requirements](#requirements)
  * [Install](#install)
  * [License](#license)

# Requirements
------------
Athrill は， 以下の環境で動作します：

  * OS
    * macOS（arm64）
    * Linux（x64，arm64）
    * Windows 11（x64，Visual Studio 2022）
  * ビルドツール
    * CMake 3.16 以降
    * C11 と C++17 に対応したコンパイラ（AppleClang，GCC，Clang，MSVC）

Athrill は， ソースデバッグ用に以下のエディタを使用します:

  * エディタ
    * [Linux]
      * geany
    * [Windows]
      * Sakura Editor

# Install
-----
このリポジトリは CPU に依存しないコア部分です．`athrill2` の実行ファイルは，ターゲットのリポジトリから CMake でビルドします．ターゲット側が `cmake/AthrillCore.cmake` を通してこのコアを取り込みます．V850E2M の場合：

```
git clone --recursive https://github.com/toppers/athrill-target-v850e2m
cmake -S athrill-target-v850e2m -B athrill-target-v850e2m/build -DCMAKE_BUILD_TYPE=Release
cmake --build athrill-target-v850e2m/build
```

別の場所にあるこのリポジトリを使ってビルドする場合は，最初の `cmake` コマンドに `-DATHRILL_SOURCE_DIR=<athrill のパス>` を追加します．

引数なしで `athrill2` を実行すると，usage が表示されます：

	Usage:athrill -m<memory config file> [OPTION]... <load_file>
	-i                             : execute on the interaction mode. if -i is not set, execute on the background mode.
	-r                             : execute on the remote mode. this option is valid on the interaction mode.
	-t<timeout>                    : set program end time using <timeout> clocks. this option is valid on the background mode.
	-m<memory config file>         : set athrill memory configuration. rom, ram region is configured on your system.
	-d<device config file>         : set device parameter.


# License
-------
Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).
