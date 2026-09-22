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
| `disk-export/plan.md` | メモリ上のディスクイメージを書き出す機能の計画と経緯（上流候補） |
| `opna/plan.md` | Makoto（YM2608 OPNA カートリッジ）対応の計画と経緯（上流候補） |
| `ai/README.md` | **AI 向けの作業規則。** 確度・コミットの分け方・出す前の点検 |

## 上流との関係

| | |
|---|---|
| このフォーク | `origin` = madscient/blueMSX-plus_Y8960 |
| 直接の上流 | Hesoten/blueMSX-plus（remote 名 `upstream`） |
| そのまた上流 | blueMSX 本家 |
| 作業ブランチ | `feature/y8960`（分岐点は上流 `main` の `5afffd55`。上流 `develop` の `5b97360e` までマージ済み） |
| ライセンス | **GPLv2**（blueMSX+ と同じ。ルートの `README.md` §License） |

**追従は `upstream/develop` を `feature/y8960` にマージする**（2026-09-13 ユーザー判断）。
`--no-ff` でマージコミットを残す。**リベースはしない** — `feature/y8960` は `origin` に
公開済みで、履歴を書き換えることになるため。
**前提**: 上流の受け口が `develop` であること（下の「上流へ出すとき」）。

#### 取り込んだ記録

| 日付 | 上流 | マージコミット | 衝突 | 確かめたこと |
|---|---|---|---|---|
| 2026-09-13 | `develop` `90e2920b`（40 コミット。PR #77 / #79、VDP・キーボードの修正ほか） | `d303604e` | `Sf7000PPI.c`、`romMapperOpcodeModule.c`（どちらも PR #77 とフォークの `ref` 付きの同じ修正の重なり。`ref` 付きを採った） | ビルド 0 エラー（警告 1 件は上流のみが触った `Win32ShortcutsConfig.c`）。`banktest` 19 項目と `ssgstest` の機械判定 5 項目がすべて OK（**確認済み**、画面） |
| 2026-09-18 | `develop` `5b97360e`（17 コミット。PR #80 / #81 の取り込み、`matrix[][]` の整理、キーボード・ジョイスティック・SG-1000・Game Reader の修正ほか） | `0795608d` | 無し | ビルド 0 エラー / 0 警告。`run-keytest` 12 項目、`banktest` と `ssgstest` の機械判定がすべて OK（**確認済み**、画面） |

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

| | | 状態（2026-09-18 に確認） |
|---|---|---|
| PR #77 | I/O ポートの解放が登録と食い違う 3 件。`fix/ioport-unregister-mismatch` | **上流 `develop` にマージされた**（マージコミット `178eaf03`） |
| issue #78 | 1 ポートに 1 デバイスしか登録できない件。多重化の提案 | OPEN のまま。PR #80 がマージされたので中身は解決している |
| PR #80 | #78 の実装（`IoPort` の多重化と `ioPortUnregister` の `ref`）。`feature/ioport-multi-claim` | **マージされた**（`fddc5683`） |
| PR #81 | キーマトリクスの注入と `/hidden`。`feature/key-matrix-input` | **マージされた**（`fd6e7e53`）。レビューで `w 0` の不具合の修正を求められ、応じた（`automation/plan.md` §4）。`matrix[][]` は上流がマージ後に `MsxPPI.h` の記述から組む形に整理した（`3aeb35b7`、`6f58b6d1`） |
| issue #74 | PSG のレジスタ番号を 4bit で丸めている件 | OPEN のまま。上流は PR #75（`be45fc46`）で直しており、本フォークは cherry-pick 済み |

PR #77 と issue #78 は本文を**日本語を先、英語を後**（`<details>` に格納）。
上流の所有者と報告者がどちらも日本語話者で、かつ公開リポジトリに
海外の利用者もいるため。**タイトルは英語**（既存の issue / PR に揃えた）。

**上流に出したものは、上流が動いたときに `develop` のマージで受け取る**（2026-09-13 ユーザー判断。
入口を 1 つにする）。PR #80 / #81 はこの形で戻ってきた（`0795608d`）。

**`IoPort.{c,h}` と `ioPortUnregister` の 53 ファイルは、もう衝突面ではない。**
上流が PR #80 をそのまま採ったので、多重化と `ref` は上流の側にある。

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

## リリース

`origin` の GitHub Releases に出す。決めたことは次のとおり（2026-09-15 ユーザー判断）。

| | |
|---|---|
| タグ名 | `v<土台の blueMSX+ の版>-y8960.<通し番号>`。1 本目は `v3.1.1-y8960.1` |
| pre-release | タグに `-` を含むので `release.yml` が自動で pre-release にする。Y8960 のハードウェアが開発中である間はこれでよい |
| ビルド | タグを push すると上流由来の `.github/workflows/release.yml` が Final × x64 / Win32 をビルドし、zip を添付して公開する |
| バイナリ zip | `scripts/ci-package.ps1` が組む。`ReleaseFiles/`・実行ファイル・プラグインだけで、**`doc/` は入らない**（**確認済み(読解)**） |
| ソースアーカイブ | GitHub が自動で添付する。`.gitattributes` の `export-ignore` で **`CLAUDE.md` と `doc/fork/` を除き、`doc/fork/y8960/user-guide.md` だけを戻す** |
| 使い方 | zip には同梱しない。リリースノートから、そのタグの `user-guide.md` へリンクする |

**前提**: 上流の `release.yml` と `ci-package.ps1` をそのまま使っていること。
上流がこれらを変えたら、タグ名の形（`v*` で起動、`-` で pre-release）と zip の中身を見直す。

**`doc/fork/` の下に足したファイルは、既定でソースアーカイブから外れる。**
利用者向けの文書を足したら `.gitattributes` に `-export-ignore` の行を足すこと。
**入れ子のディレクトリも戻す必要がある** — ディレクトリが除外のままだと、
`git archive` はその下を見に行かず、中のファイルを戻しても落ちる。

確かめたこと（**確認済み**、`git archive --worktree-attributes HEAD | tar -t`）:
変更前は `CLAUDE.md` と `doc/` 以下 43 件が入り、変更後は `doc/fork/y8960/user-guide.md` と
その親ディレクトリの 4 件だけになった。`doc/fork/y8960` を戻す行を無効にした版では
`user-guide.md` も落ちた。**GitHub の自動アーカイブが `export-ignore` に従うことは、
公開したアーカイブを落として確かめる**（下の記録）。

見送ったこと: **`user-guide.md` を zip に同梱する案。** `ci-package.ps1` は上流のファイルで、
触ると衝突面が 1 つ増えるため。

**フォークのワークフローは、所有者が Actions タブで有効にするまで登録されない。**
登録前にタグを push しても何も起きず、エラーも出ない（**確認済み** 2026-09-15:
`gh api repos/<owner>/<repo>/actions/workflows` の `total_count` が 0 で、
`gh workflow run` は `not found on the default branch` を返した）。有効にした後は、
**タグを打ち直さずに** `gh workflow run release.yml --ref <タグ>` で起動できる。
`github.ref` がタグになるので、公開の段まで走る（1 本目はこの形で公開した）。

**リリースノートは自動生成の中身を差し替える。** `release.yml` は
`generate_release_notes` で変更履歴へのリンクだけを書くので、
公開後に `gh release edit --notes-file` で使い方へのリンクと注意を載せる。

### リリースの記録

| タグ | コミット | 確かめたこと |
|---|---|---|
| `v3.1.1-y8960.1`（pre-release） | `61f40fe7` | Actions の run 34860219682 で x64 / Win32 のビルドと公開が成功。**公開物を落として確かめた**（**確認済み**）: バイナリ zip 2 本に `doc/` と `CLAUDE.md` が無い。GitHub のソースアーカイブ（zip / tar.gz）は `export-ignore` に従い、`doc/` 以下は `user-guide.md` だけ。x64 / Win32 の実行ファイルとも `/listromtypes` と `/listmachines` が終了コード 0 で、前者に `Y8960SCC` が出た。**Final 構成で音や画面を確かめたことはない**（手元の確認はすべて Release 構成） |

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
