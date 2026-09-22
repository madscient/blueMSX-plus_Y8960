# Makoto（YM2608 OPNA カートリッジ）対応 — 計画と経緯

対象読者は開発者と AI。Y8960 とは独立した、音源カートリッジの追加。

上流候補なので、**`upstream/develop` から切った枝 `feature/makoto-opna` に、
コードだけのコミットで作る**（`doc/fork/README.md` の「上流へ出すとき」）。
この文書は上流に出さない。

## 1. 調べて分かったこと

### 1.1 Makoto のハードウェア

出典は VGMPlay MSX の Makoto ドライバ（sharksym/vgmplay-sharksym の
`src/drivers/Makoto.asm`）。**確認済み(読解)**。製品ページ（Supersoniqs）と
MSX Resource Center の開発スレッドには I/O の記述が無い。

| | |
|---|---|
| チップ | YM2608 (OPNA) + YM3016 (DAC) |
| クロック | 8 MHz |
| I/O ポート | `14h` 表アドレス / 表ステータス、`15h` 表データ、`16h` 裏アドレス / 裏ステータス、`17h` 裏データ |
| 表（`14h`/`15h`） | FM 1–3ch、SSG、リズム（ADPCM-A）、共通レジスタ |
| 裏（`16h`/`17h`） | FM 4–6ch、ADPCM-B |
| ADPCM-B のメモリ | ドライバは **1bit DRAM モード**（裏 `01h` の bit1 = 0）に設定する。アドレスは 4 バイト単位 16bit なので上限 256 KB |
| `14h` の読み | ドライバは読まない。コメントに「Music Module とバスが衝突する」とある |
| 検出 | 裏 `00h` に `80h`（START）を書き、裏ステータスの bit5 が立つことを見る |

**未確認**:

- **基板に載っている ADPCM-B の RAM の容量。** ドライバは容量を調べない。
  1bit DRAM モードのアドレス上限（256 KB）までは書けることしか分からない
- **/IRQ が MSX の /INT に繋がっているか。** ドライバは割り込みを切って使う
  （`29h` に `80h`）ので、ドライバからは決まらない
- `14h` の読みで何が返るか（本物のステータスか、衝突で不定か）

### 1.2 ymfm

GitHub の aaronsgiles/ymfm、`81aec25ccb`（2026-07-27）時点。**確認済み(読解)**。

| | |
|---|---|
| ライセンス | BSD-3-Clause（GPLv2 と両立する） |
| OPNA の実装 | `ymfm_opn.{h,cpp}` の `ym2608` |
| 依存 | `ymfm.h`、`ymfm_fm.{h,ipp}`、`ymfm_adpcm.{h,cpp}`、`ymfm_ssg.{h,cpp}` |
| 同居しているもの | `ymfm_opn.cpp` には OPN / OPNB / OPN2 系の他チップも入っている |

**ymfm がホストに任せているもの**（`ymfm_interface` の仮想関数）:

| | ymfm の口 | blueMSX 側で要るもの |
|---|---|---|
| リズムの波形 ROM（8 KB） | `ymfm_external_read(ACCESS_ADPCM_A, …)` | 利用者が置くファイルを読む（§2） |
| ADPCM-B の RAM | `ymfm_external_read/write(ACCESS_ADPCM_B, …)` | カートリッジが持つバッファ |
| タイマ A / B | `ymfm_set_timer` | ボードのタイマに繋ぐ。**割り込みを使わないソフトもステータスのフラグで待つ**ので要る |
| IRQ | `ymfm_update_irq` | 繋ぐかどうかは §1.1 の未確認事項による |
| 出力 | `generate()`。FM の出力は 8 MHz / 144 ≈ 55.5 kHz | ミキサーのレートへ変換する |

`ym2608::reset()` はリズム 6 音の開始・終了アドレス（`0000h`–`1FFFh`）を
自分で設定する。ROM の中身だけを渡せばよい。

### 1.3 blueMSX 側

| | |
|---|---|
| `14h`–`17h` の既存の使い手 | `romMapperSvi328RsIDE.c`（SVI-328 のみ）。I/O ポートは多重登録できるので、重なっても登録は落ちない |
| C++ の規格 | `msvc2022/blueMSX.vcxproj` はファイルごとに `LanguageStandard` を指定している（`OpenMsxY8950Latest*.cpp` が `stdcpp20`） |
| ビルド定義 | `msvc2022`、`msvc2026`、`gcc/Makefile` の 3 系統。明示列挙 |

## 2. 決まったこと（2026-09-22 ユーザー判断）

| | 決定 | 前提 |
|---|---|---|
| 枝 | `upstream/develop` から `feature/makoto-opna`。コードだけ | 上流の受け口が `develop` であること |
| 計画文書 | 本書（`feature/y8960` 側） | 上流は AI 向け文書を置かない方針 |
| リズム ROM | **利用者が置く。無ければリズムだけ無音**、他の音は鳴る | ROM は YM2608 内蔵のマスク ROM の吸い出しで、再配布できない |
| ymfm の取り込み | **必要なファイルを無改変で** `Src/SoundChips/ymfm/` に置き、取り込んだコミットを記録する | ymfm の更新にそのまま追従できることを、コード量より優先する |

### 見送ったもの

- **リズムを実装しない案** — 見送り。ROM を利用者が置けば鳴らせるので、
  鳴らせる経路を最初から閉じる理由が無い
- **`ymfm_opn.cpp` から `ym2608` だけを抜き出す案** — 見送り。
  ymfm を更新するたびに抜き出し直すことになる

## 3. 外に出る値（決める直前に聞く）

後から変えると利用者のファイルに波及するもの。**まだ決めていない。**

| 値 | 波及先 |
|---|---|
| RomType の名前と番号 | マシン構成の `config.ini`、ROM データベース |
| ミキサーのチャンネル種別 | `bluemsx.ini` の音量・パンの設定キー |
| セーブステートの節名 | 保存済みのステート |
| リズム ROM のファイル名と置き場所 | 利用者の手順 |
| ADPCM-B の RAM 容量 | 大きい方へ変えるのは安い。小さい方へは既存のステートが読めなくなる |

## 4. 経緯

- 2026-09-22 調査と §2 の決定。
