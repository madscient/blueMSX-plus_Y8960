# doc/fork — このフォーク固有の文書

本リポジトリは [blueMSX+](https://github.com/Hesoten/blueMSX-plus) のフォークである。
blueMSX+ 自身が [blueMSX](https://msxblue.com/bluemsx/) の非公式フォークなので、
上流は 2 段になっている。

ここには blueMSX+ に存在しない、**このフォークの作業に関する文書**を置く。
対象読者は開発者。エンドユーザー向けではない。

**本書は人間向けの事実と索引である。** 作業の規則（AI 向け）は `ai/` に分けてある。

上流は `.gitignore` で `CLAUDE.md` を除外しており、AI 向け文書をリポジトリに
置かない方針だが、**このフォークでは場所を限定して置く**（2026-09-12 ユーザー判断）。
限定した場所はルートの `CLAUDE.md`（**索引のみ**）と `doc/fork/ai/` の 2 か所。

## 文書一覧

| パス | 内容 |
|---|---|
| `y8960/` | Y8960 カートリッジのエミュレーション実装 |
| `y8960/README.md` | Y8960 の文書の役割と索引 |
| `y8960/hardware-notes.md` | Y8960 のハードウェア仕様。madscient/openMSX_Y8960 からの写し |
| `y8960/implementation-plan.md` | blueMSX+ 側の実装計画・決定・実行経緯 |
| `build/README.md` | ビルド環境。このマシンの事情と手順、ヘッドレスで確かめられること |
| `ai/README.md` | **AI 向けの作業規則。** 確度・コミットの分け方・出す前の点検 |

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

### 上流へ出すとき

**受け口は `develop` である。** `main` へ向けた PR は
`.github/workflows/retarget-main-pr.yml` が自動で `develop` に付け替える。
最初から `develop` に向ければその手間が要らない。

**`develop` は `main` と分岐している。** 追従に使っている
`main` ベースのコミットはそのままでは載らないので、
`upstream/develop` から枝を切り直し、上流に当たる形で書き直す。
このフォーク側の都合（Y8960 のための API 変更など）を混ぜないこと。

**fork からの PR は CI が自動で走らない。** GitHub が
`action_required` で止め、上流の所有者の承認を待つ
（**確認済み** 2026-09-12: PR #77 で `mergeStateStatus` が `BLOCKED`、
`mergeable` は `MERGEABLE`）。**こちらからは動かせない。**

ローカルで同じ 4 構成（Debug / Release × x64 / Win32）を回すことはできるが、
**その結果を PR に書いても上流の作業は減らない**（上流は自分で CI を回す）。
報告のために回す必要はない。

### 出した記録

| | |
|---|---|
| PR #77 | I/O ポートの解放が登録と食い違う 3 件。`fix/ioport-unregister-mismatch` |
| issue #78 | 1 ポートに 1 デバイスしか登録できない件。多重化の提案 |

どちらも本文は**日本語を先、英語を後**（`<details>` に格納）。
上流の所有者と報告者がどちらも日本語話者で、かつ公開リポジトリに
海外の利用者もいるため。**タイトルは英語**（既存の issue / PR に揃えた）。

**issue #78 には実測値を載せていない。** 手元で測った
「書き込みが両方に 258 件ずつ届く」という数字は、一時的な計測コードを
入れないと再現できない。**読み手に検証手段の無い数字は根拠にならない**ので、
コードと生成順から辿れる事実だけで論じる形にした（2026-09-12 ユーザー判断）。

## Y8960 の実装状況

**7 ブロックが `RomType` を持ち、マシン構成に個別に置ける。**
そのうち 3 ブロックは中身が入っている。

| ブロック | 状態 | 土台 |
|---|---|---|
| OPLLEX | **器のみ** | `Src/SoundChips/Emu2413/` のフォーク |
| OPL2EX | **器のみ** | `Src/SoundChips/OpenMsxY8950Latest/` のフォーク |
| SSGS | **器のみ** | `Src/SoundChips/AY8910.c` のフォーク |
| MSX-TIMER | 実装済み・**音は持たない** | 新規 |
| MSX-MIXER | **器のみ** | 新規 |
| Y8960 SCC + マッパー | 実装済み・**音は未聴取** | `Src/SoundChips/Y8960Scc.c`（`SCC.c` のフォーク）+ 新規マッパー |
| DCSG | 実装済み・**音は未聴取** | `Src/SoundChips/Y8960Dcsg.c`（`SN76489.c` のフォーク） |
| I/O イネーブラ / MMIO 窓 | 実装済み | SCC のマッパーが持つ |

**音を実際に聴いた者はまだいない。** 確かめてあるのは MSX 側から見える
レジスタとメモリの挙動まで（`y8960/implementation-plan.md` の各 §「確かめたこと」）。
**タイマ割り込みが CPU に届くことも未確認**で、フラグが立つところまでしか見ていない。

**カートリッジ内の音源ブロックは、SCC や DCSG そのものではなく等価回路である。**
だから本体のコアを共有せず、フォークとして実装している
（openMSX_Y8960 と同じ方針。経緯は `y8960/implementation-plan.md` §11 の
2026-09-12 (10)）。**上流の `SCC.{c,h}` と `SN76489.{c,h}` には手を入れていない。**

**I/O ポートの重ね合わせ**（Y8960 の SSGS が本体 PSG に重なるために要る）は
上流の `IoPort` を直して解決済み。経緯は `y8960/implementation-plan.md` §4.2。

詳細と残作業は `y8960/implementation-plan.md`。
**現在地・確かめずに残してあること・次の一手は同 §0。**
着手の前に決めるべきことは同 §9。

**ハードウェア仕様は `y8960/hardware-notes.md` だけを見ないこと。**
写しより新しい情報が `y8960/implementation-plan.md` §3.3 にある。

## ライセンスと帰属

blueMSX+ 全体は **GPLv2**（ルートの `README.md` §License）。

### 持ち込んではいけないもの

**hra1129/Y8960_Cartridge の FPGA ソースと図表。**
そのライセンスは条項 3 で「書面による事前の許可なしに販売、および
商業的な製品や活動に使用しないこと」を課しており、
**非商用制限は GPL と両立しない**。コードもデータも取り込まない。

**MsxSoundSuiteExtension の ROM イメージ**（`y8960bas.rom` など）。
**再配布には MSX ライセンシングコーポレーションの許諾が別途必要**で、
許諾されているのは個人利用に限られる。blueMSX+ は GPLv2 で配布されるので、
同梱すれば配布物に再配布制限付きのバイナリが入ることになる。

**どちらも、ローカルで検証に使うことは妨げられない。** 実機の BIOS ROM と
同じく gitignore 済みのビルド出力に置く（`build/README.md` §4）。
`ReleaseFiles/` の側には置かないこと。そちらは追跡され、配布物に入る。

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

`Src/SoundChips/Y8960OpllCore.c` は Emu2413 のフォークなので MIT 表示を引き継ぎ、
音色データの CC BY-SA 表示を併せて持つ。

音色データを別の出所から持ち込むときは、その出所の表示も要る。
**"Copyright free OPLL(x) ROM patches" (David Viens / Hubert Lamontagne、
CC BY-SA) は `Src/SoundChips/Y8960OpllCore.c` に入っている。**
同ファイル冒頭の音色テーブルに付いた出典表示が帰属表示なので、消さない。
詳細は `y8960/implementation-plan.md` §5.2。

## 作業の規則は `ai/` にある

**AI 向けの作業規則は `doc/fork/ai/README.md` に分けてある。**
確度の付け方、コミットの分け方、上流のファイルを触ったときの記録、
出す前の点検はそちら。**上流はこの種の文書をリポジトリに置かない方針だが、
このフォークでは場所を限定して置く**（ルートの `CLAUDE.md` は索引のみ）。

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

## 外部リポジトリ

本リポジトリには取り込んでいない情報源。

| リポジトリ | 役割 | 扱い |
|---|---|---|
| [hra1129/Y8960_Cartridge](https://github.com/hra1129/Y8960_Cartridge) | Y8960 の**一次仕様**（FPGA RTL + docx/xlsx）。WIP | 仕様の参照元。**コードもデータも取り込まない**（非商用ライセンスで GPL と非互換） |
| [madscient/openMSX_Y8960](https://github.com/madscient/openMSX_Y8960) | openMSX 側の Y8960 実装。全ブロック実装済み・実測済み。GPL-2.0 | **移植元**。設計判断と確定値の出所 |
| [madscient/Y8960emu](https://github.com/madscient/Y8960emu) | OPLLEX / OPL2EX の別実装（ymfm ベース）。MIT | 設計の参照と挙動の突き合わせ用。コアは移植しない |
| [madscient/EPSGemuEngine](https://github.com/madscient/EPSGemuEngine) | YMZ705/732/771 系 SSG の実装。MIT + BSD-3 | SSGS のレジスタ配置とパンポット則の出所 |
| [madscient/MsxSoundSuiteExtension](https://github.com/madscient/MsxSoundSuiteExtension) | **Y8960 を駆動する拡張BASIC と BIOS**。開発中 | **ファームウェアが前提にしている仕様の出所。** `y8960bas.rom` は検証に使えるが、**ROM をリポジトリに入れてはいけない** |
