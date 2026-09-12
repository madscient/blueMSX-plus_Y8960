# Y8960 カートリッジ ハードウェア仕様 調査ノート

## 本書の出所と役割（blueMSX+ 側で足した節）

**本書は madscient/openMSX_Y8960 の `doc/fork/y8960/hardware-notes.md` の写しである。**
写した日: 2026-09-12。本節以外は**一字も変えていない**。

- **役割**: 情報リソース。Y8960 のハードウェア仕様がどう読めるかの記録であって、
  blueMSX+ の成果物でも仕様書でもない
- **本文中の `implementation-plan.md` への参照は、写し元である openMSX_Y8960 側の
  文書を指す。** blueMSX+ 側の実装判断は本リポジトリの `implementation-plan.md` にある
- **本文が「本リポジトリ」「openMSX 側の階層」と書くとき、それは openMSX_Y8960 を
  指す。** `src/sound/Y8960OPL2.cc` のようなパスは blueMSX+ には存在しない
- 一次情報の調査は openMSX_Y8960 側で行われた。**blueMSX+ 側では一次情報に
  当たり直していない**（2026-09-12 時点）

**二重管理になっている。** 写し元が更新されても本書は追随しない。
食い違いに気づいたら、写し元を正として本書を写し直すこと。

## 本書より新しい情報がある（2026-09-12）

**本書を読む前に `implementation-plan.md` §3.3 を見ること。**

Y8960 を駆動するファームウェア（madscient/MsxSoundSuiteExtension）の
文書が、本書の調査（2026-09-08）より後の、より確かな記述を持っている。
**実際に動くソフトが前提にしている仕様**なので、食い違う場合はそちらが強い。

本書の記述のうち、少なくとも次は**古いか、決着が付いている**。

| 本書の記述 | 現状 |
|---|---|
| §7.3 ROM/RAM バンクの境界が「出典 4 つとも違う」 | **決着。`#0`-`#15` が ROM、`#16`-`#31` が RAM** |
| §7.5 ADPCM メモリの構成切替が未決 | **4 状態ある（共有 / 片寄せ ×2 / 分割）。設定方法は実機側で未実装。暫定で起動時は共有** |
| §6 SSG の LED が「1Fh の下位 4bit」 | **SSG2 の `2Fh` の下位 4bit** |
| §8「BIOS ROM イメージが存在しない」 | **`y8960bas.rom` (128KB) が実在する**（Y8960_Cartridge には無い、という意味では今も正しい） |
| クロックの記載が無い | **SSGS 1.7897725MHz、他 3.579545MHz** |
| §3 MMIO 窓の条件が `memory_io_en = ~rammode` | **BANK1 に ROM バンクが出ているときだけ表に出る。RAM バンクまたは SCC のときは隠れる** |

---

調査日: 2026-09-08 / 対象: hra1129/Y8960_Cartridge @ d6d16a3 (main)

**パスの読み方**: 本書が `src/...` と書くとき、断りが無ければ
hra1129/Y8960_Cartridge の `fpga/Y8960_Cartridge_TangPrimer25K/src/...` を指す。
`doc/spec/` や `doc/manual/` も同リポジトリのもの。
本リポジトリ（openMSX フォーク）のファイルは `src/sound/...` のように
openMSX 側の階層で書き、文脈で区別できるようにしている。

**確度について**: 本書の記述はすべて上記リポジトリの成果物を読んで得たもので、
実機での確認は行っていない（実機は未完成）。各項目の確度は個別に付記する。

- **確認済み(読解)** — 当該ファイルを読んで確認した。ファイル名と行を示す
- **未検証** — 読解はしたが、実機・シミュレーションで動作を確かめていない
- **推測** — 出典を示せない

一次情報は複数あり、**互いに食い違っている**。優先順位は次のとおり。

1. **hra1129 さんからの直接の回答**（Discord など）
2. `doc/spec/*.xlsx` / `doc/manual/*.docx`
3. `fpga/.../src/*.v` (RTL)

**RTL は最下位である。** 2026-09-09 に hra1129 さんから
「RTL は WIP とみなしてよい、SSGS も反映されていないのでプレースホルダ」との
確認があった。当初は「RTL が唯一動く形で書かれている」ことを理由に最優先に
置いていたが、**これは誤りだったので改めた**。実際 DCSG のアドレス、
OPLL の enabler ビット、SSG のブロック構成のいずれも RTL が古かった。

食い違いは §7 に列挙する。

## 1. 全体構成

MSX 用サウンドカートリッジ。Tang Primer 25K (Gowin FPGA) 上に実装。
搭載ブロック（**確認済み**: `src/y8960_cartridge_tangprimer25k.v`）:

| ブロック | 相当チップ | 回路数 | RTL の IP | ライセンス |
|---|---|---|---|---|
| SSG | YM2149 | 2 | 自作 (`src/ssg/ssg_core.v`) | MIT |
| OPLL | YM2413 + 音色バンク拡張 | 2 | IKAOPLL | BSD-2 |
| OPL2 | YM3812 | 2 | jtopl2 | GPL3 |
| ADPCM | YM2608 ADPCM-B | 2 | jt10_adpcmb | GPL3 |
| DCSG | SN76489 | 2 | sn76489_audio | BSD-3 |
| SCC | Konami SCC + 独自マッパー | 1 | IKASCC | BSD-2 |
| MSX-TIMER | 独自 | 1 | 自作 (`src/timer/`) | MIT |

出典: `doc/spec/Y8960_Specifications.xlsx` の `modules` シート、および上記 RTL。

音声は 24bit ステレオで I2S 出力。ただし現状の RTL のミキサーは
**L と R に完全に同じ式**を代入しており、パンポットは未実装（**確認済み**:
`y8960_cartridge_tangprimer25k.v` の `w_mix_l` / `w_mix_r`）。

## 2. I/O ポートマップ

**確認済み**: `src/y8960_address_decode/y8960_address_decode.v` の localparam 群。

| ポート | ブロック | リセット時の有効/無効 |
|---|---|---|
| A0h-A1h, A2h-A3h | SSG (Addr / Data-W / Data-R) | **有効** |
| 7Ah-7Bh | OPLL #1 (Addr/Data) | **有効** |
| 7Ch-7Dh | OPLL #2 (Addr/Data) | 無効 |
| 7Eh-7Fh | DCSG #1/#2 | 無効 |
| C0h-C1h | OPL2 #1 (Addr/Data) | 無効 |
| C2h-C3h | OPL2 #2 (Addr/Data) | 無効 |
| B0h-B3h | MSX-TIMER | 無効 |
| 40h-4Fh | System Controller (device_id = 61h) | 常時（enabler 無し） |

アドレス一致は enabler と独立に判定される。つまり無効なブロックのポートに
アクセスしても、System Controller のデフォルト腕には落ちない（**確認済み**:
`w_io_known_match` の使われ方）。

`doc/fpga/.../doc/ioport.txt` には `B6h-B7h: MSX-SOUND MIXER` の記載があるが、
RTL のアドレスデコーダにも top module にも存在しない。**未実装**。

### 本体デバイスとの衝突

- A0h-A2h は MSX 本体 PSG と衝突する。これは**設計上の意図**である。
  SSG #1 は `BUILTIN=0` でインスタンス化されており、リードデータを駆動しない
  （**確認済み**: top の `dual_ssg #(.BUILTIN(0))` と `ssg_core.v` の
  `generate if(builtin)`）。つまり SSG #1 は本体 PSG と同じレジスタ空間に
  重ねて発音するだけの存在で、読み出しは本体 PSG が担う。
- 7Ah-7Bh は MSX-MUSIC (7Ch-7Dh) とは別アドレスなので、リセット直後の
  既定状態では本体 MSX-MUSIC と衝突しない。
- C0h-C3h (MSX-AUDIO)、7Ch-7Dh (MSX-MUSIC)、7Eh-7Fh は既定で無効。
  Panasonic FM-PAC と同じ「起動直後は I/O 無効、メモリマップド I/O で
  有効化する」方式（**確認済み**: manual §3.1）。

## 3. メモリマップド I/O

**確認済み**: 同 `y8960_address_decode.v`。

窓は `A15==0 && A[13:5]==9'b11_1111_111` すなわち **7FE0h-7FFFh**、
ミラーが **3FE0h-3FFFh**。書き込み専用。`memory_io_en` が 1 のときのみ有効で、
top では `memory_io_en = ~scc_ma[5] = ~rammode`（**確認済み**）。
つまり **RAM モードではメモリマップド I/O は消える**。

| アドレス | 内容 |
|---|---|
| 7FEAh / 7FEBh | SSG Addr / Data |
| 7FECh / 7FEDh | OPL2 #2 Addr / Data |
| 7FEEh / 7FEFh | OPL2 #1 Addr / Data |
| 7FF0h / 7FF1h | DCSG #2 / DCSG #1 |
| 7FF2h / 7FF3h | OPLL #2 Addr / Data |
| 7FF4h / 7FF5h | OPLL #1 Addr / Data |
| 7FF6h | I/O Enabler 1 |
| 7FFFh | I/O Enabler 2 |

**この表は正しい。窓の中は高位側が #1 である**（**確認済み**: 2026-09-09 に
hra1129 さんから直接の回答）。直接 I/O 側の並びとは**別々に決まっている**ので、
片方から他方は導けない。理由はどちらも過去の実装との互換である。

| ブロック | 直接 I/O の #1 | 窓の #1 |
|---|---|---|
| OPLL | 7Ch-7Dh（高位） | 7FF4h-7FF5h（高位） |
| OPL2 | C0h-C1h（低位） | 7FEEh-7FEFh（高位） |
| DCSG | 3Eh（低位） | 7FF1h（高位） |

I/O Enabler 1 (7FF6h): b0 = OPLL#1, b1 = OPLL#2
I/O Enabler 2 (7FFFh): b0 = OPL2#1, b1 = OPL2#2, b2 = DCSG#1, b3 = DCSG#2,
b4 = SSG, b7 = MSX-TIMER

窓に当たらないメモリアクセス、および窓に当たっても読み出しの場合は SCC に回る
（**確認済み**: `w_mem_plain` / `w_mio_default_match`）。

## 4. SCC / メガ ROM マッパー

**確認済み**: `src/ikascc_patch/IKASCC_vrc_s.v`（IKASCC 純正からの patch）、
`src/ikascc_patch/scc_bank.v`。

IKASCC 内蔵マッパーを patch して 2 モードにしたもの。

**互換モード (rammode=0)** — Konami SCC 相当:
- BANK レジスタ: 5000-57FFh(BANK0) / 7000-77FFh(BANK1) / 9000-97FFh(BANK2) / B000-B7FFh(BANK3)。
  **rammode が 1 のときは受け付けない**（`&& !rammode`）
- SCC 音源レジスタ: `bankreg2 == 6'h3F` かつ A[15:11]==10011b、すなわち 9800-9FFFh

**RAM モード (rammode=1)** — 独自:
- モードレジスタ: 4?FBh (? = 8..F、`ablo == FBh` かつ A[15:11]==01001b) の b0
- BANK レジスタ: 4?FCh / 4?FDh / 4?FEh / 4?FFh。**rammode が 0 のときは受け付けない**
- SCC 音源レジスタが追加で `rammode && bankreg1 == 6'h3F` かつ A[15:11]==01111b
  = 7800-7FFFh にも出現
- 書き込みが SRAM まで通る

リセット値は `rammode=0`、`bankreg0..3 = 0,1,2,3`。

### 4.1 バンク番号と rammode が実際に何を決めているか

**確認済み(読解)**: 2026-09-09 に `d6d16a3` を読んだ。効いているのは次の 3 行。

| ファイル | 行の要旨 |
|---|---|
| `ikascc_patch/IKASCC_vrc_s.v` | `o_ROMADDR = { rammode, bankregN[4:0] }` |
| `ikascc_patch/scc_bank.v` | `cpu_address = { scc_ma[17:13], bus_address[12:0] }` |
| 同上 | `cpu_write = w_ram_mode & bus_write`、`w_ram_mode = scc_ma[18]` |

`scc_ma` は `o_ROMADDR` そのものなので、これを合わせると次になる。

- **メモリのアドレスは `bankreg[4:0]` だけで決まる。** 8KB × 32 バンク = 256KB。
  `rammode` はアドレスに寄与しない
- **`rammode` が決めるのは書き込みの可否だけ**（および `memory_io_en = ~scc_ma[5]`
  によるメモリマップド I/O の窓の有無。**確認済み**: top の当該行）
- **バンク番号による ROM/RAM の区別は無い。** `cpu_write` にバンク番号の分岐が
  一切無いので、RAM モードでは 32 バンクすべてが書ける
- **4000-5FFFh の書き込み保護も無い**（`w_mem_access = bus_valid & ~bus_io` で、
  アドレスによる分岐が無い）
- バンクレジスタは 6bit 書けるが、アドレスに出るのは `[4:0]` の 5bit。
  **bit5 は SCC 音源レジスタの `== 6'h3F` 比較にだけ効く**

メモリマップド I/O の窓は
`memory_io_en & bus_write & (A15==0) & (A[13:5]==9'b111111111)`
（**確認済み**: `y8960_address_decode.v`）。**書き込みのみ**で、
`rammode=1` のとき窓ごと消える。

## 5. メモリ構成

**確認済み**: `fpga/.../doc/memory_map.txt`。

- Serial Flash ROM 8MB: 000000h-77FFFFh = FPGA コンフィグ、780000h-7FFFFFh = SRAM 初期イメージ
- Serial SRAM 512KB:
  - 00000h-1FFFFh: BIOS-ROM 8KB × 16 バンク
  - 20000h-3FFFFh: BIOS-RAM 8KB × 16 バンク
  - 40000h-7FFFFh: ADPCM RAM 256KB

SCC マッパーが見るのは 00000h-3FFFFh の 256KB（**確認済み**:
`scc_bank.v` の `cpu_address` が 18bit）。ADPCM は 18bit アドレスで
40000h 以降を使う（**確認済み**: `sram_arbiter` のポート幅）。

BIOS-ROM の中身は電源投入時に Flash から SRAM へコピーされる
（**確認済み**: `system_controller.v` の `sram_initialize` 経路）。
**BIOS ROM イメージ自体はリポジトリに存在しない**。

## 6. 拡張ブロックの仕様

### 6.1 OPLL の 2 回路とアドレスの対応

回路の選択は `bus_address[1]` で行われる（**確認済み**:
`ikaopll_patch/opll.v:27-28` の `w_cs0_n` / `w_cs1_n`）。
直接 I/O とメモリマップド I/O のどちらも同じ規則で振り分けられる。

| 回路 | 直接 I/O | メモリマップド | 7FF6h の enable ビット | リセット時 |
|---|---|---|---|---|
| `u_ikaopll0` (core 0) | **7Ch-7Dh** | 7FF4h-7FF5h | **bit1** | **無効** |
| `u_ikaopll1` (core 1) | 7Ah-7Bh | 7FF2h-7FF3h | **bit0** | **有効** |

7Ah と 7FF2h は A[1:0]=10、7Ch と 7FF4h は A[1:0]=00 なので、この対応になる。

**回路とアドレスの対応は xlsx と一致している。** xlsx は 7Ch/7Dh と 7FF4h を
OPLL1（1 番目）と呼んでおり、これは core 0 に当たる。従来の MSX-MUSIC の
アドレスが 1 番目に割り当たっているという意味では、素直な設計である。

食い違っているのは次の 2 点。

1. **enable ビットの割り当て**: xlsx の I/O Enabler1 は b0=OPLL1, b1=OPLL2。
   RTL は b0 が `ff_opll1_io_en`（= 7Ah 側 = core 1 = xlsx の OPLL2）を、
   b1 が 7Ch 側（core 0 = xlsx の OPLL1）を制御する。**逆である**
   （**確認済み**: `y8960_address_decode.v:141, 218-219`）
2. **リセット時の状態**: `ff_opll1_io_en = 1`（7Ah 有効）、
   `ff_opll2_io_en = 0`（7Ch 無効）。
   つまり**電源投入直後、従来の MSX-MUSIC アドレス 7Ch-7Dh には出てこない**
   （**確認済み**: 同 115-116, 207-208 行）。
   これはマニュアル §3.1 の「起動直後・リセット直後は無効になっています」とも
   食い違う（SSG の A0-A2 も同様にリセット時有効）

参考: openMSX の FM-PAC 実装 (`src/sound/MSXFmPac.cc`) は `reset()` で
`enable = 0`、`writeIO` は `enable & 1` のときだけ OPLL に流す。
つまり FM-PAC は 7Ch-7Dh がリセット時無効で、7FF6h の **bit0** で有効化する。
Y8960 で FM-PAC と同じ手順（7FF6h に 01h）を踏むと、開くのは 7Ah 側になる。

2026-09-09 のユーザー判断により、**openMSX 側は FM-PAC を正として実装した**
（b0 が 7Ch-7Dh、b1 が 7Ah-7Bh、リセット直後は両方とも閉じる）。
RTL の現状は取り違えとして採らない。詳細は `implementation-plan.md` §3.3。
RTL 側を直すかどうかは hra1129 さんの領分。

### OPLLEX (拡張 OPLL)

YM2413 に、**チャンネルごとに**プリセット音色バンクを選ぶ機能を追加したもの。

| BANK 値 | 音色セット | 相当チップ |
|---|---|---|
| 0 | OPLL | YM2413 |
| 1 | OPLL-X | YM2423 |
| 2 | OPLL-P | YMF281 |
| 3 | VRC7 | DS1001 |

新設レジスタ **40h-48h**（ch0-ch8 に対応）の b1:b0 が BANK。
出典: `doc/spec/Y8960_Specifications.xlsx` の `OPLL-EX` シート
（**確認済み**: 抽出テキストの C62-C70 行）、および
madscient/Y8960emu の `doc/Y8960emu_Architecture.md` §3.2（**確認済み**）。

**RTL 側は未実装**。top が使う `src/ikascc_patch/opll.v`（正しくは
`ikaopll_patch/opll.v`）は素の IKAOPLL を 2 個並べているだけで、
BANK レジスタも 4 バンクの音色 ROM も入っていない（**確認済み**: 当該ファイル）。
つまり OPLLEX は現時点では**仕様だけが存在する**。

音色データの出典は "Copyright free OPLL(x) ROM patches" (David Viens,
Hubert Lamontagne)、CC BY-SA。

### OPL2EX (拡張 OPL2)

YM3812 (OPL2) に ADPCM-B を足したもの。レジスタ配置は Y8950 (MSX-AUDIO) と同一。
`07h`, `09h-12h`, `15h-17h` が ADPCM-B 系、それ以外は YM3812 に委譲。
出典: `doc/spec/*.xlsx` の `OPL2+ADPCM` シート、Y8960emu アーキ文書 §3.1（**確認済み**）。

Y8950 との差は **OPL2 の波形選択** (E0h-F5h の WS、および TEST レジスタ 01h の
b5 = 波形選択イネーブル)。xlsx の C53 行に「OPL2 から追加」と明記（**確認済み**）。

ADPCM メモリは manual §1.3 に「128KB×2ch か 256KB×1 (2ch 共通) を選択可能」と
あるが、**選択レジスタは RTL にもレジスタ表にも無い**。RTL の
`sram_arbiter` は ADPCM を 18bit(256KB) 単一空間として持ち、
2 つの ADPCM ブロックがそれを共有する（**確認済み**）。

### SSG — SSGS (YMZ705 / YMZ732 相当)

**2026-09-09 に仕様が変わった。** hra1129 さんによれば、このブロックは
「SSG × 2」ではなく **SSGS（YMZ705 相当）**である。
2026-08-30 の議論（Pen さんの提案）で **YMZ732 のレジスタ配置に合わせる**ことが
決まり、hra1129 さんが「現状 00,,,0F 本体互換 / 10,,,1F 追加3ch。
これを追加の方をずらします」と回答している（**確認済み**: Discord のスクリーンショット）。

確定したレジスタ配置（**確認済み**: madscient/EPSGemuEngine の
`src/YmzSsg.h` / `src/YmzSsg.cpp`。YMZ705/732/771 の実装）:

| 範囲 | 内容 |
|---|---|
| `$00-$1F` | SSG-1 |
| `$20-$3F` | SSG-2 |
| `$40` 以上 | ADPCM / シーケンサ領域（Y8960 が使うかは未確認） |

系統の選択は `(reg >> 5) & 1`。各系統内では `sub = reg & 0x1F` として:

| `sub` | 内容 |
|---|---|
| `$00-$0D` | YM2149 と同じレジスタ |
| `$0E-$0F` | **無い**。YMZ 系は I/O ポートを持たない |
| `$10-$12` | ch A/B/C の **4bit パンポット** |

パンポット値と L/R レベルの対応は**データシートに記載が無い**。
EPSGemuEngine は同世代の YMZ280B と同じ分配則を採っている
（中央値で両側全開、離れるほど反対側だけが線形に絞られ、端の 2 段は片側全振り）。
これは実装者の判断であって仕様ではない。

また AY コアの出力は片極性なので、**パンを掛ける前に無音時レベル（直流成分）を
差し引く**必要がある。省くと直流が定位に漏れる（EPSGemuEngine のコメントより）。

以下は**変更前**の RTL の記述である。`ssg/ssg_core.v` は上記の新配置に
追いついていない（レジスタポインタが 5bit で b4 がコア番号、パンポット無し）。

YM2149 相当 × 2。レジスタポインタは 5bit で、**b4 がコア番号**
（**確認済み**: `ssg_core.v` の `{ core_number, 4'dN }` によるデコード）。

- レジスタ 00h-0Fh → SSG #1（本体 PSG と重なる。読み出しは駆動しない）
- レジスタ 10h-1Fh → SSG #2（読み出しあり。1Fh の下位4bit が LED）

xlsx の `ssg` シートには F0h-FFh が RW として空欄で並んでいる。用途不明・**未実装**。

### DCSG

SN76489 相当 × 2。1 バイト書き込みのみ（レジスタラッチ方式）。拡張なし。

### MSX-TIMER

4 本のカウンタを持つ独立ブロック。B0h = レジスタ番号、B1h = 値、
B2h = 割り込みフラグ、B3h = カウンタ読み出し
（**確認済み**: `src/timer/msx_timer.v` の `c_register_index` 他）。
分解能は 0-7 の 8 段。**Base Clock 85.909080MHz (= 3579545 × 24) を
2^(10+2r) で分周したもの**で、11.919578us 〜 195290.369772us
（**確認済み**: `src/timer/doc/msx_timer.xlsx` を展開して
Base Clock と Unit Time の列を読み、式と一致することを確かめた）。

| reso | Unit Time | reso | Unit Time |
|---|---|---|---|
| 0 | 11.919578us | 4 | 3051.412028us |
| 1 | 47.678313us | 5 | 12205.648111us |
| 2 | 190.713252us | 6 | 48822.592443us |
| 3 | 762.853007us | 7 | 195290.369772us |

**同じシートの Count Time 列は分解能ではない。** Unit Time ×（終端値 + 1）で、
シートの例は終端値 5。そこに並ぶ 71.517us 〜 1171742us を分解能と取り違えない。

割り込みは MSX-TIMER と OPL2 内蔵タイマーの 2 系統が
`w_int_n = w_timer_intr_n & w_opl2_intr_n` で OR される（**確認済み**）。

## 7. 一次情報どうしの食い違い

いずれも**確認済み**（両方の記述を読み比べた）。

本書は食い違いの事実だけを記録する。どちらを採るかの判断は
`implementation-plan.md` §4.3 にある。

**出典は 3 系統ではなく 4 系統ある。** 上記 3 つに加えて、
buppu3/openMSX の `y8960` ブランチの既存実装（2026-01 時点の仕様に基づく）が
さらに別の値を持っている。DCSG は 48h/4Ah、ミキサーは 40h-41h。
3 つ目と 4 つ目の突き合わせ表は `implementation-plan.md` §4.3 にある。

1. **DCSG の I/O アドレス**: xlsx `dcsg` シートは **3Eh/3Fh**、
   RTL は **7Eh/7Fh** (`c_dcsg_io = 8'h7E`)。bit6 が違う。
   **決着済み**: 2026-09-09 に hra1129 さんが 3Eh/3Fh を正と回答。RTL が古い。
2. **OPLL の enable ビットとリセット時の状態**（§6.1 に詳述）。
   回路とアドレスの対応は RTL と xlsx で一致しているが、
   7FF6h の enable ビットの割り当てが xlsx と RTL で逆になっており、
   さらにリセット時に有効なのが 7Ch-7Dh ではなく 7Ah-7Bh の側である。
2.1. **OPL2 のメモリマップド側の呼び方**: xlsx は C0h-C1h を OPL2-1、
   7FECh-7FEDh を OPL2-2 としている。RTL の `bus_address[1]` から読むと
   どちらも 1 番目の回路に届くように見える（**確認済み**:
   `opl2_patch/opl2.v:102-103`）。
   **決着済み**: 2026-09-09 に hra1129 さんが **xlsx を正**と回答。
   直接 I/O と窓は別々に並びが決まっており（§3）、RTL から窓の側を推論した
   こちらの読みが誤りだった。DCSG の 7FF0h/7FF1h も同じ。

3. **ROM/RAM バンクの境界**: **出典が 4 つあり、4 つとも違う**
   （**確認済み**: 2026-09-09 に `d6d16a3` の当該行を読んだ）。

   | 出典 | 記述 |
   |---|---|
   | manual §4 | BANK#0-15 が ROM、#16-31 が RAM |
   | `IKASCC_vrc_s.v` のヘッダコメント | BANK#0-#7 が ROM、#8-#15 が RAM |
   | `scc_bank.v` のコメント | 互換モードは 0-31 が read only、RAM モードは 0-15 が read only で 16-31 が read/write |
   | **RTL の assign** | **バンク番号による分岐が無い。RAM モードなら全 32 バンクが書ける** |

   同じ `IKASCC_vrc_s.v` のヘッダは「BANK0...3 is ROM only」「BANK0 is ROM only」
   とも書いているが、これに当たるコードも無い。§4.1 を見ること。
4. **B6h-B7h のサウンドミキサー**: `ioport.txt` にあるが RTL に無い。
5. **ADPCM メモリの 128KB×2 / 256KB×1 切替**: manual にあるがレジスタが無い。
6. **BANK0 の出現アドレス**: manual §4 は「4000h-5FFFh と C000h-DFFFh の2か所」。
   BANK1-3 のミラーの有無は manual の図（画像）にあり、テキスト抽出できていない。
   `IKASCC.v` の `i_ABHI` デコードから復元は可能だが**未確認**。

## 8. 未完成であることの確認

- `doc/manual/Y8960_Programmer's_manual.docx` は §5 レジスタ仕様が
  「《記入》」のまま、§3.2-3.6・§6.2・§7.2 が空。**骨組みのみ**。
- OPLLEX の BANK 機能は RTL に無い（§6）。
- サウンドミキサー(B6h/B7h)、ADPCM メモリ構成切替、SSG の F0h-FFh は未実装。
- BIOS ROM イメージが存在しない。
