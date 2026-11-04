athrill
=======

Athrill は CPU エミュレータです. 
Athrill を使用すると，仮想マイコン上で組込みプログラムを簡単に実行/デバッグすることができます．
ベアメタルプログラムやリアルタイムOS上で動作する組込み制御プログラムを評価できます．

Athrill は TOPPERS プロジェクトで V850 向けに始まりました．バージョン3（`athrill3`，v3.0.0 から）は
C++ で書かれた CPU に依存しないコアと，ターゲットごとのリポジトリに分かれています．C 版は v1.2.1 が最後です．
Athrill は次の4つの命令セットをサポートしています．

| ターゲットのリポジトリ | 命令セット | 動くもの |
| --- | --- | --- |
| [athrill-target-v850e2m](https://github.com/toppers/athrill-target-v850e2m) | V850E2M | TOPPERS のリアルタイムOS上のプログラム |
| [athrill-target-rh850f1x](https://github.com/toppers/athrill-target-rh850f1x) | RH850 (F1x) | RH850/F1x マイコン向けのプログラム |
| [athrill-target-riscv](https://github.com/hakoniwalab/athrill-target-riscv) | RISC-V (RV64IMFC) | AIFoundry Erbium ボード（PX4 本家のボード）上の PX4 |
| [athrill-target-stm32](https://github.com/hakoniwalab/athrill-target-stm32) | Armv7-M (Cortex-M7) | Pixhawk 4（`px4_fmu-v5`）向けの無改造の PX4 ファームウェア．QGroundControl から飛行 |

Athrill は仮想クロック（1命令1クロック，アイドル時間は飛ばす）で動くため，同じ入力からの実行は完全に再現します．
また，[箱庭](https://github.com/toppers/hakoniwa)と同期して動かせます（`--hakoniwa`）．

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
このリポジトリは CPU に依存しないコア部分です．`athrill3` の実行ファイルは，ターゲットのリポジトリから CMake でビルドします．ターゲット側が `cmake/AthrillCore.cmake` を通してこのコアを取り込みます．V850E2M の場合：

```
git clone https://github.com/toppers/athrill.git
git clone https://github.com/toppers/athrill-target-v850e2m.git
cmake -S athrill-target-v850e2m -B athrill-target-v850e2m/build -DCMAKE_BUILD_TYPE=Release
cmake --build athrill-target-v850e2m/build
```

ターゲットは，隣に置いた `../athrill` をこのリポジトリとして使います．別の場所にあるチェックアウトを使う場合は，最初の `cmake` コマンドに `-DATHRILL_SOURCE_DIR=<athrill のパス>` を追加します．

引数なしで `athrill3` を実行すると，usage が表示されます：

	Usage:athrill -m<memory config file> [OPTION]... <load_file>
	-i                             : execute on the interaction mode. if -i is not set, execute on the background mode.
	-r                             : execute on the remote mode. this option is valid on the interaction mode.
	-t<timeout>                    : set program end time using <timeout> clocks. this option is valid on the background mode.
	-m<memory config file>         : set athrill memory configuration. rom, ram region is configured on your system.
	-d<device config file>         : set device parameter.


# License
-------
Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).
