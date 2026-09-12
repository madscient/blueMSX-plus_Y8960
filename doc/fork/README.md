# doc/fork — このフォーク固有の文書

本リポジトリは [blueMSX+](https://github.com/Hesoten/blueMSX-plus) のフォークである。
blueMSX+ 自身が [blueMSX](https://msxblue.com/bluemsx/) の非公式フォークなので、
上流は 2 段になっている。

ここには blueMSX+ に存在しない、**このフォークの作業に関する文書**を置く。
対象読者は開発者。エンドユーザー向けではない。

**作業時に守る規則は本書に書く。** ルートの `CLAUDE.md` は上流の `.gitignore` が
除外しており（`# Claude Code local state and per-session working files.`）、
**追跡されないローカルファイル**である。したがって他の環境や他の人には届かない。
残す規則は本書の「ライセンスと帰属」以降に置くこと。

## 文書一覧

| パス | 内容 |
|---|---|
| `y8960/` | Y8960 カートリッジのエミュレーション実装 |
| `y8960/README.md` | Y8960 の文書の役割と索引 |
| `y8960/hardware-notes.md` | Y8960 のハードウェア仕様。madscient/openMSX_Y8960 からの写し |
| `y8960/implementation-plan.md` | blueMSX+ 側の実装計画・決定・実行経緯 |
| `build/README.md` | ビルド環境。このマシンの事情と手順、ヘッドレスで確かめられること |

## 上流との関係

| | |
|---|---|
| このフォーク | `origin` = madscient/blueMSX-plus_Y8960 |
| 直接の上流 | Hesoten/blueMSX-plus（**remote は未設定**。2026-09-12 時点で `origin` だけ） |
| そのまた上流 | blueMSX 本家 |
| 作業ブランチ | `main` |
| ライセンス | **GPLv2**（blueMSX+ と同じ。ルートの `README.md` §License） |

**追従の方法は未定である。** 上流を remote として追加するか、
どうマージ／リベースするかを決めていない。決めたらここに書く。

## Y8960 の実装状況

**2026-09-12 時点で、コードは一行も書かれていない。**

| ブロック | 状態 | 土台 |
|---|---|---|
| OPLLEX | 未着手 | `Src/SoundChips/Emu2413/` のフォーク |
| OPL2EX | 未着手 | `Src/SoundChips/OpenMsxY8950Latest/` のフォーク |
| SSGS | 未着手（**I/O の重ね合わせが未決**） | `Src/SoundChips/AY8910.c` のフォーク |
| MSX-TIMER | 未着手 | 新規 |
| MSX-MIXER | 未着手 | 新規 |
| Y8960 SCC + マッパー | 未着手 | `Src/SoundChips/SCC.c` + 新規マッパー |
| DCSG | 未着手 | `Src/SoundChips/SN76489.c`（既存コア） |
| I/O イネーブラ / MMIO 窓 | 未着手 | SCC のマッパーが持つ |

詳細と残作業は `y8960/implementation-plan.md`。
**着手の前に決めるべきことは同 §9。**

## ライセンスと帰属

blueMSX+ 全体は **GPLv2**（ルートの `README.md` §License）。

### 持ち込んではいけないもの

**hra1129/Y8960_Cartridge の FPGA ソースと図表。**
そのライセンスは条項 3 で「書面による事前の許可なしに販売、および
商業的な製品や活動に使用しないこと」を課しており、
**非商用制限は GPL と両立しない**。コードもデータも取り込まない。

`y8960/hardware-notes.md` は**読解した事実の記述**であって、コードや文章の
複製ではない。仕様を参照するのは構わないが、この線を越えない。

### 落としてはいけない帰属表示

blueMSX+ のファイルは冒頭に帰属を持つ。**どれも消さない。**

- 原著者（blueMSX の Daniel Vik ほか）
- blueMSX+ のフォーク表示（`Modified 2026 by Hesoten for blueMSX+ fork.`）
- 取り込み元があるものはその表示（`OpenMsxY8950Latest.h` の openMSX 由来の記述など）

**新しくファイルを作るときも、土台にしたファイルの帰属を引き継ぐ。**

| 土台にする既存コード | ライセンス |
|---|---|
| `Src/SoundChips/Emu2413/` | MIT (Mitsutaka Okazaki) |
| `Src/SoundChips/NukedOPLL/` | GPLv2 |
| `Src/SoundChips/OpenMsxY8950Latest/` | GPL（openMSX 由来） |
| `Src/SoundChips/AY8910.c` / `SN76489.c` / `SCC.c` | GPLv2（blueMSX 由来） |

音色データを別の出所から持ち込むときは、その出所の表示も要る。
"Copyright free OPLL(x) ROM patches" (David Viens / Hubert Lamontagne) は
**CC BY-SA なので帰属表示が要る**。詳細は `y8960/implementation-plan.md` §5.2。

## 作業の記録と確度

主題ごとに作業計画と実行経緯の文書を持ち、決定・却下・訂正はその都度書く。
Y8960 なら `y8960/implementation-plan.md`。口頭で終えない。

主張には確度を併記する。走らせて確かめたなら**確認済み**とその手段を、
読んだだけなら**確認済み(読解)** とファイル名と行を、作っただけなら**未検証**、
出典を示せないなら**推測**と根拠を一行。

**外に出る値を決める直前に一声かける。** `RomType` の数値、ミキサー種別の
識別子、I/O アドレス、設定ファイルのキー、セーブステートの節名。
これらは後から変えると利用者のファイルに波及する。確定したものは
`y8960/implementation-plan.md` §6 にだけ書く（二重に持つと片方が古くなる）。

### 上流のファイルに触ったら記録する

**衝突面はできるだけ小さく保つ。** 上流のファイルを触ったまま放置すると、
上流がそこに手を入れた瞬間に解決の手間になる。触ったファイルと理由は
`y8960/implementation-plan.md` に書く。

Y8960 の実装で上流のファイルに手が入ることが分かっているのは、
`Src/Media/MediaDb.{h,cpp}`、`Src/Board/Machine.c`、
`Src/SoundChips/AudioMixer.h`、`Src/Emulator/Properties.c`、
`Src/Win32/Win32machineConfig.c`、ビルド定義 3 系統、`Src/SoundChips/SN76489.{c,h}`。
`Src/Memory/IoPort.c` も入る可能性がある（implementation-plan §9.1）。

## ビルドと実行

**手順と、このマシンの事情は `build/README.md` にある。**
ビルド手順の一次情報は `.github/workflows/ci.yml` で、ローカルもそれに揃える。
2026-09-12 に Release / x64 が通ることを確認した。

**ファイルを 1 本足すごとにビルド定義 3 系統に書き足す。** どれも明示列挙で、
自動収集は無い（implementation-plan §4.4）。片方だけ直すと、
もう片方のビルドが後で壊れる。

**ビルドをパイプに繋がないこと。** `msbuild ... | tail` のようにすると
パイプラインの終了コードが `tail` のものになり、ビルドが失敗しても後続の
`&&` が通ってしまう。ログはファイルにリダイレクトして終了コードを直接見る。

### 起動にはウィンドウが要る。ただし例外がある

**エミュレータを起動するとウィンドウが出る。人間の並行作業と衝突しうるので、
走らせる前に一声かけること。自動で繰り返し起動しない。**

**例外**: `/listromtypes` `/listmachines` `/listspecials` `/listthemes` は
一覧を標準出力に書いて `exit(0)` する。**ウィンドウが作られる前**に終わる
（**確認済み**: 実行してもプロセスもウィンドウも残らず、終了コード 0 と
出力が返った）。RomType の登録とマシン構成の妥当性はこれで確かめられる。
射程は `build/README.md` §5。

## 出す前に

`origin` は GitHub にある。**push したオブジェクトは親リポジトリから SHA で
辿れて、後から消せない。** 点検は push の前にしか意味が無い。

- **ローカル固有のパスが混入していないか。** 成果物にもコミットメッセージにも
  書かない。スクリプトでパスが要るときは環境に依存しない形にする
- **個人情報が混入していないか。** 成果物にもコミットメッセージにも書かない
- 未コミットの変更が残っていないか
- 文書の索引と実体が食い違っていないか

## 外部リポジトリ

本リポジトリには取り込んでいない情報源。

| リポジトリ | 役割 | 扱い |
|---|---|---|
| [hra1129/Y8960_Cartridge](https://github.com/hra1129/Y8960_Cartridge) | Y8960 の**一次仕様**（FPGA RTL + docx/xlsx）。WIP | 仕様の参照元。**コードもデータも取り込まない**（非商用ライセンスで GPL と非互換） |
| [madscient/openMSX_Y8960](https://github.com/madscient/openMSX_Y8960) | openMSX 側の Y8960 実装。全ブロック実装済み・実測済み。GPL-2.0 | **移植元**。設計判断と確定値の出所 |
| [madscient/Y8960emu](https://github.com/madscient/Y8960emu) | OPLLEX / OPL2EX の別実装（ymfm ベース）。MIT | 設計の参照と挙動の突き合わせ用。コアは移植しない |
| [madscient/EPSGemuEngine](https://github.com/madscient/EPSGemuEngine) | YMZ705/732/771 系 SSG の実装。MIT + BSD-3 | SSGS のレジスタ配置とパンポット則の出所 |
