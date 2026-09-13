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
| `y8960/user-guide.md` | **Y8960 対応の使い方（エンドユーザー向け）** |
| `y8960/hardware-notes.md` | Y8960 のハードウェア仕様。madscient/openMSX_Y8960 からの写し |
| `y8960/implementation-plan.md` | blueMSX+ 側の実装計画・決定・実行経緯 |
| `build/README.md` | ビルド環境。このマシンの事情と手順、ヘッドレスで確かめられること |
| `automation/plan.md` | キー入力の注入とウィンドウを出さない実行の計画（上流候補） |
| `ai/README.md` | **AI 向けの作業規則。** 確度・コミットの分け方・出す前の点検 |

## 上流との関係

| | |
|---|---|
| このフォーク | `origin` = madscient/blueMSX-plus_Y8960 |
| 直接の上流 | Hesoten/blueMSX-plus（remote 名 `upstream`） |
| そのまた上流 | blueMSX 本家 |
| 作業ブランチ | `feature/y8960`（分岐点は上流 `main` の `5afffd55`。上流 `develop` の `90e2920b` までマージ済み） |
| ライセンス | **GPLv2**（blueMSX+ と同じ。ルートの `README.md` §License） |

**追従は `upstream/develop` を `feature/y8960` にマージする**（2026-09-13 ユーザー判断）。
`--no-ff` でマージコミットを残す。**リベースはしない** — `feature/y8960` は `origin` に
公開済みで、履歴を書き換えることになるため。
**前提**: 上流の受け口が `develop` であること（下の「上流へ出すとき」）。

#### 取り込んだ記録

| 日付 | 上流 | マージコミット | 衝突 | 確かめたこと |
|---|---|---|---|---|
| 2026-09-13 | `develop` `90e2920b`（40 コミット。PR #77 / #79、VDP・キーボードの修正ほか） | `d303604e` | `Sf7000PPI.c`、`romMapperOpcodeModule.c`（どちらも PR #77 とフォークの `ref` 付きの同じ修正の重なり。`ref` 付きを採った） | ビルド 0 エラー（警告 1 件は上流のみが触った `Win32ShortcutsConfig.c`）。`banktest` 19 項目と `ssgstest` の機械判定 5 項目がすべて OK（**確認済み**、画面） |

**衝突を片側で解くときは、ファイル全体がその側になる。** `git checkout --ours` は
自動マージできた部分も捨てる。2026-09-13 のマージでは `romMapperOpcodeModule.c` に
入るはずだった PR #79 の 1 行がこれで落ち、入れ直した。解いた後は
`git diff upstream/develop -- <file>` を見て、差がフォーク側の意図した変更だけであることを確かめる。

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

| | | 状態（2026-09-13 に確認） |
|---|---|---|
| PR #77 | I/O ポートの解放が登録と食い違う 3 件。`fix/ioport-unregister-mismatch` | **上流 `develop` にマージされた**（マージコミット `178eaf03`） |
| issue #78 | 1 ポートに 1 デバイスしか登録できない件。多重化の提案 | OPEN。コメント無し |
| PR #80 | #78 の実装（`IoPort` の多重化と `ioPortUnregister` の `ref`）。`feature/ioport-multi-claim` | 2026-09-13 に提案として提出。取り込むかは上流に委ねた |
| PR #81 | キーマトリクスの注入と `/hidden`。`feature/key-matrix-input` | 同上 |
| issue #74 | PSG のレジスタ番号を 4bit で丸めている件 | OPEN のまま。上流は PR #75（`be45fc46`）で直しており、本フォークは cherry-pick 済み |

PR #77 と issue #78 は本文を**日本語を先、英語を後**（`<details>` に格納）。
上流の所有者と報告者がどちらも日本語話者で、かつ公開リポジトリに
海外の利用者もいるため。**タイトルは英語**（既存の issue / PR に揃えた）。

**上流とのあいだの保留は、PR #80 / #81 に集めた**（2026-09-13 ユーザー判断）。取り込むかどうかは上流の
所有者に委ね、こちらは上流が動いたときに `develop` のマージで受け取る（入口を 1 つにする）。

**PR #80 が決着するまで、`IoPort.{c,h}` と `ioPortUnregister` の 53 ファイルは
上流と食い違ったままになる。** 上流が別の形で多重化を入れた場合、取り込みのときに
この 53 ファイルがまとめて衝突面になる。

**PR #79 は取り込んだ（`d303604e`）。取り込む前のこのフォークには次の不具合があった。** 対象の 5 つのカートリッジ
（MegaFlashROM SCC+ / SCC+ SD、Opcode PSG / Module、Yamanooto）は、自分のポート
（`10h`-`12h` や `50h`-`52h`）で PSG を受けるのに、内部の AY8910 を `AY8910_MSX` で
作っていたので `A0h`-`A2h` も取っていた。

| | 上流（1 ポート 1 デバイス） | このフォーク（重ね合わせ） |
|---|---|---|
| カートリッジスロットに挿す | 本体 PSG が先に取っているので登録は黙って落ちる。**抜くと本体 PSG の `A0h`-`A2h` を解放してしまう** | 本体 PSG と重なる。**本体 PSG への書き込みがカートリッジの PSG にも届き、同じ音が 2 つ鳴る。`A2h` の読みはカートリッジ側のレジスタ 14（書かれたままの 0）と AND され、ジョイスティックが全部押された値になる** |
| マシン構成のスロットに置く | カートリッジが先に取り、本体 PSG の登録が落ちる | 同上 |

いずれも **確認済み(読解)**（`ay8910Create` / `ay8910ReadData` / `ioPortRegister` と
`MSX.c` の生成順）。**このフォークの行は動かして見てはいない**（**未検証**）。

**PR #79 は issue #78 の部分的な修正ではなく、別の不具合の修正と読む。** #78 は
「正しく重なる登録（FM-PAC と本体 MSX-MUSIC など）を表が持てない」、#79 は
「デコードしていないポートを登録していた」。#79 は `IoPort.{c,h}` に触れず、
#78 の例（FM-PAC）は直らない。逆に #78 を直した後でも #79 は要る（このフォークがその状態）。
**#78 との関係は未確認** — #79 のコミットは #78 の約 6.5 時間後で、#78 は
MegaFlashROM SCC+ SD に触れているが、PR に本文も参照も無く、#78 にもコメントが無い。
上流が多重化を採るかどうかは #78 で聞くまで分からない。

**Yamanooto の ECHO は PR #79 の後も issue #78 の形のまま残る。** ECHO を立てると
`A0h`/`A1h` を登録し、落とすと解放する。上流では登録が落ち、解放で本体 PSG のポートが
消える。このフォークでは `ref` 付きで解放するので、重ね合わせの下で設計どおりに働く
（**確認済み(読解)**）。

**issue #78 には実測値を載せていない。** 手元で測った
「書き込みが両方に 258 件ずつ届く」という数字は、一時的な計測コードを
入れないと再現できない。**読み手に検証手段の無い数字は根拠にならない**ので、
コードと生成順から辿れる事実だけで論じる形にした（2026-09-12 ユーザー判断）。

## Y8960 の実装状況

**7 ブロックが `RomType` を持ち、マシン構成に個別に置ける。すべてに中身が入っている。**
加えて基盤タイプ `MSX2++`（Y8960 内蔵、SSGS が本体の PSG）がある。

| ブロック | 状態 | 土台 |
|---|---|---|
| OPLLEX | 実装済み・**音を確認** | `Src/SoundChips/Emu2413/` のフォーク |
| OPL2EX | 実装済み・**音を確認** | `Src/SoundChips/OpenMsxY8950Latest/` のフォーク |
| SSGS | 実装済み・**音を確認**（カートリッジ / 内蔵とも） | `Src/SoundChips/AY8910.c` のフォーク |
| MSX-TIMER | 実装済み・**音は持たない** | 新規 |
| MSX-MIXER | **入口のみ**（実機側が未実装） | 新規 |
| Y8960 SCC + マッパー | 実装済み・**音を確認** | `Src/SoundChips/Y8960Scc.c`（`SCC.c` のフォーク）+ 新規マッパー |
| DCSG | 実装済み・**音を確認** | `Src/SoundChips/Y8960Dcsg.c`（`SN76489.c` のフォーク） |
| I/O イネーブラ / MMIO 窓 | 実装済み | SCC のマッパーが持つ |

**5 つの音源ブロックすべてを、エミュレータの中で人が聴いて確かめた。**
SCC と DCSG は録音からも判定している（`y8960/tests/sndtest.asm`）。
**タイマ割り込みが CPU に届くことも確かめた**（`banktest` の 19）。
詳細は `y8960/implementation-plan.md` の §0 と各 §「確かめたこと」。

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
