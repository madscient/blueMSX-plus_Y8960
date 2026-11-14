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
| 作業ブランチ | `feature/y8960`（分岐点は上流 `main` の `5afffd55`。上流 `develop` の `55ca0a3f` までマージ済み） |
| ライセンス | **GPLv2**（blueMSX+ と同じ。ルートの `README.md` §License） |

### 枝の役割

**枝に何かを入れる前に、その枝の役割をここで見る。** 上流候補の枝は、PR をその枝から出すので、
その機能以外のコミットを入れると PR に混ざる。

| 枝 | 役割 | 入れてよいもの |
|---|---|---|
| `feature/y8960` | フォークの統合の枝。Y8960 のリリースと、この `doc/fork/` の置き場 | 何でも（上流の同期、上流候補の枝のマージ、フォーク固有の変更） |
| `feature/makoto-opna` | **上流候補。Makoto の PR をこの枝から出す**。Makoto のリリースもここから作る（`opna/plan.md`） | Makoto のコミットと、上流 `develop` の同期だけ |
| `feature/sound-mixer` | **上流候補。サウンドミキサーの PR をこの枝から出す**（`mixer/plan.md`） | ミキサーのコミットだけ。**`upstream/develop` から切ったので、追跡先を `origin` にしてから push する** |
| `fix/*`、`feature/key-matrix-input` など | 上流に出した PR の枝。状態は下の「出した記録」 | その PR の修正だけ |

上流候補の枝は `upstream/develop` から切る。**`git worktree add -b <枝> ... upstream/develop` や `git checkout -b <枝> upstream/develop` は
追跡先を `upstream` にする**ので、引数なしの `git push` が上流へ向かう。最初の push は `git push -u origin <枝>` で行う。

**追従は `upstream/develop` を `feature/y8960` にマージする**（2026-09-13 ユーザー判断）。
`--no-ff` でマージコミットを残す。**リベースはしない** — `feature/y8960` は `origin` に
公開済みで、履歴を書き換えることになるため。
**前提**: 上流の受け口が `develop` であること（下の「上流へ出すとき」）。

**取り込んだら `py doc/fork/y8960/tests/check-irq-lines.py` を走らせる**（2026-11-13 ユーザー判断）。割り込みの線は番号の衝突を
git もコンパイラも知らせず、重なると互いの保留中の割り込みを消し合う。2026-11-13 に、上流の V9990 と V9968 が Y8960 の 3 本と
Makoto の 1 本に重なっていたのを、取り込んだ後に偶然見つけた。終了コード 1 は重なり、2 は道具が値を決められない呼び出し
（道具の `PASS_THROUGH` に足す）。置き場所の則は下の「フォーク固有の番号」。

#### 取り込んだ記録

| 日付 | 上流 | マージコミット | 衝突 | 確かめたこと |
|---|---|---|---|---|
| 2026-09-13 | `develop` `90e2920b`（40 コミット。PR #77 / #79、VDP・キーボードの修正ほか） | `d303604e` | `Sf7000PPI.c`、`romMapperOpcodeModule.c`（どちらも PR #77 とフォークの `ref` 付きの同じ修正の重なり。`ref` 付きを採った） | ビルド 0 エラー（警告 1 件は上流のみが触った `Win32ShortcutsConfig.c`）。`banktest` 19 項目と `ssgstest` の機械判定 5 項目がすべて OK（**確認済み**、画面） |
| 2026-09-18 | `develop` `5b97360e`（17 コミット。PR #80 / #81 の取り込み、`matrix[][]` の整理、キーボード・ジョイスティック・SG-1000・Game Reader の修正ほか） | `0795608d` | 無し | ビルド 0 エラー / 0 警告。`run-keytest` 12 項目、`banktest` と `ssgstest` の機械判定がすべて OK（**確認済み**、画面） |
| 2026-09-29 | `develop` `9fd3d21e`（36 コミット。SCC の精度修正、VDP コマンドのウェイト、PSG、既定の FM コアを emu2413 / emu8950 に、Game Reader、C-BIOS v1.0.1 ほか） | `6569450c` | 無し | ビルド 0 エラー / 7 警告（すべて Y8960 が触っていない `Win32*.c` と `SimpleDebugger`）。上流の SCC 修正 7 件を `Y8960Scc.c` に移植した後（`a96f5c71`、`y8960/implementation-plan.md` §5.7）に、`banktest` の機械判定 15 項目、`ssgstest` の機械判定 5 項目、`run-keytest` 12 項目がすべて OK、`sndtest` が PASS（**確認済み**、画面と録音） |
| 2026-10-04 | `develop` `83d8f0f1`（15 コミット。PR #96 / #97 / #98。turbo R の R800 のタイミング、Ports ページとプリンタ出力、パッケージファイルのハンドル判定） | `0e7f9de7` | `Board/MSX.c`（上流の R800 ウェイト設定とフォークの MSX2++ の PSG 分岐が隣り合う。両方を残した） | ビルド 0 エラー / 6 警告（すべて上流と同一の `Win32*.c`。Y8960 の 15 本は再コンパイルされ、警告なし）。フォーク元の音源コアへの上流の修正は 0 件で、移植は無し。`banktest` の機械判定 15 項目、`ssgstest` の機械判定 5 項目、`run-keytest` 12 項目がすべて OK、`sndtest` が PASS（**確認済み**、画面と録音。`y8960/implementation-plan.md` §11 の (32)） |
| 2026-10-05 | `develop` `ae181356`（3 コミット。PR #99 の 2 コミットとそのマージ。本体 MSX-AUDIO の openMSX バックエンドの ADPCM の終端） | `6a484779` | 無し | ビルド 0 エラー / 0 警告（再コンパイルは 4 本で、Y8960 のソースは含まない）。入った 3 本は OPL2EX のフォーク元だが、OPL2EX には同じ修正が `0a066de3` で入っており、移植は無し（変更行を突き合わせた。違いはコメントの 3 行だけ）。`adpcmtest` が、本体 MSX-AUDIO の openMSX バックエンドでマージ前の NG からマージ後の OK に変わり、Y8960 では前後とも OK（**確認済み**、`/hidden` で画面）。**`banktest` `ssgstest` `sndtest` `run-keytest` は回していない**（入った 3 本を使う機種構成が無いため）。`y8960/implementation-plan.md` §11 の (36) |
| 2026-10-07 | `develop` `9cfe017f`（8 コミット。PR #101 / #102。MSX-AUDIO の ADPCM の終端の追い修正、割り込みを選ばれたバックエンドからだけ出す、テープの保存） | `47317652` | 無し | ビルド 0 エラー / 6 警告（すべて上流と同一の `Win32*.c`）。追い修正 4 件を OPL2EX と読み比べ、停止番地を手前へ動かしたときの 1 行を移植した（`56292e3e`。コア単体の試験に 3 項目を足し、移植前に 2 項目が落ちることを見た）。`adpcmtest`（本体 MSX-AUDIO の 2 バックエンドと Y8960）、`banktest` の機械判定 15 項目、`ssgstest` の機械判定 5 項目、`run-keytest` 12 項目がすべて OK、`sndtest` が PASS（**確認済み**、`/hidden` で画面と録音）。`y8960/implementation-plan.md` §11 の (37) |
| 2026-10-16 | `develop` `96771382`（28 コミット。PR #103-#107 / #110 / #111。Rookie Drive NX、フォルダディスクの修正、MegaFlashROM SCC+ SD の修正、物理ドライブの接続、フォルダをハードディスク・フロッピーとして挿して書き戻す） | `43b73cad` | `MediaDb.{h,cpp}`、`Win32Menu.c`（上流が Rookie Drive NX に RomType 180 とメニュー ID 41122 を取り、フォークの Y8960 と重なった。**Y8960 の RomType を 200-207 へ移した**。ユーザー判断。`v3.1.1-y8960.2` の利用者の `config.ini`・履歴・ステートが壊れる破壊的変更） | ビルド 0 エラー / 7 警告（(37) と同じ 6 件と `SimpleDebugger` の 1 件。Y8960 のファイルには無い）。フォーク元の音源コアへの上流の修正は 0 件で、移植は無し。新しい番号の構成で `banktest` の機械判定 15 項目、`ssgstest` の機械判定 5 項目がすべて OK、`/romtype1 Y8960`（207）でも `banktest` 15 項目 OK、`/listromtypes` と `/listmachines` が終了コード 0（**確認済み**、`/hidden` で画面）。**`sndtest` と `run-keytest` は回していない**（上流は `SoundChips/` と `Input/` に触れていない）。`y8960/implementation-plan.md` §11 の (38) |
| 2026-10-19 | `develop` `475dadfa`（41 コミット。PR #112-#118。GFX9000 / Video9000、ROM の判定、フラッシュの書き込み保護、zip の一覧、コマンドラインの ROM の拡張子） | `24246cc5` | `MediaDb.{h,cpp}`、`Win32Menu.c`（上流が RomType 181 / 182 とメニュー ID 41123 / 41124 を取った。RomType は重ならず両方残した。**メニュー ID 41123 が `Y8960 Cartridge` と重なり、Y8960 側を 41125 へ移した**。メニュー ID は外に出ない） | ビルド 0 エラー / 6 警告（すべて上流と同一の `Win32*.c`）。フォーク元の音源コアへの上流の修正は 0 件で、移植は無し。出力先の試験構成と `banktest.rom` が古いままだったので写し直した。`banktest` の機械判定 15 項目（カード一式でも 15 項目）、`run-keytest` 12 項目がすべて OK、`/listromtypes` と `/listmachines` が終了コード 0（**確認済み**、`/hidden` で画面）。**`sndtest` は回していない**（上流は `SoundChips/` に触れていない）。`y8960/implementation-plan.md` §11 の (46) |
| 2026-10-27 | `develop` `0d6ba91b`（5 コミット。PR #119 / #120。ASCII16-X・Yamanooto・Flash-ROM SCC のフラッシュを変えた分だけ保存、長いパスのダイアログ、ポインタの自動非表示） | `a4a6e998` | 無し | ビルド 0 エラー / 3 警告（すべて上流と同一の `Win32*.c`。Release / x64、`msvc2026`）。フォーク元の音源コアへの上流の修正は 0 件で、移植は無し。Y8960 のマッパーは `AmdFlash` も `sramLoader` も使わない。`banktest` の 1-7・10・13-24 が OK（**確認済み**、`/hidden` で画面。9・11・12 は聴く項目で、聴いていない）。**`sndtest` と `run-keytest` は回していない**（上流は `SoundChips/` と `Input/` に触れていない）。`y8960/implementation-plan.md` §11 の (57) |
| 2026-11-13 | `develop` `55ca0a3f`（166 コミット。PR #122-#131 / #133-#135。サウンドミキサーのメーターを RMS に作り直し、MIDI フェーダー、設定の読み込み、ダイアログの位置、`/mute`、文字モードの色 0、Agent Control と MCP サーバ、VDP ビューア、V9968 + geo3d v2.0。上流の `v3.2.0-pre-alpha` と同じコミット） | `bfb20e9b` | 13 ファイル 26 箇所。両側の追加を並べただけのものが 10 本（言語 5 本、`MediaDb.cpp`、`MegaromCartridge.c`、`SlotManager.c`、`blueMSX.vcxproj` 2 本）。`MediaDb.h`（上流の RomType 183 は Y8960 の 200-208 と重ならず、両方残した）。`Win32Menu.c`（**上流の V9968 がメニュー ID 41125 を取り、`Y8960 Cartridge` とまた重なった。Y8960 をカートリッジ 1 の範囲の上端 41149 へ移した**。ユーザー判断。下の「フォーク固有の番号」）。`AudioMixer.c`（9 箇所。上流がメーターを作り直し、フォークのミキサーの作り直しと重なった。解き方は `y8960/implementation-plan.md` §11 の (65)） | ビルド 0 エラー / 7 警告（すべて上流と同一の `Win32*.c` と `SimpleDebugger`。Release / x64、`msvc2026`）。上流が足した `McpServer` も 0 エラー / 0 警告。フォーク元の音源コアへの上流の修正は 0 件で、移植は無し。`banktest` の 1-7・10・13-24（`(whole cart)` と `(banktest)` の両方）、`v099test` 1-7、`adpcmtest` 1-2、`run-keytest` 12 項目がすべて OK、`sndtest` が PASS で、**録音がマージ前の実行ファイルとバイト単位で一致**（音が出始めてから 1,154,812 フレーム）。ミキサーの probe が全項目 OK（**確認済み**、`/hidden` で画面と録音）。メニュー命令 41149 / 41299 で両スロットに `Y8960 Cartridge`（207）が、41125 / 41275 で V9968（183）が挿さる（**確認済み**、閉じた後の設定ファイル）。`y8960/implementation-plan.md` §11 の (65) |

**フォーク固有の番号は、上流が番号を伸ばす向きと反対の端に置く。** 上流は新しい項目に、最後の番号の次を順に振る。
RomType は 2026-10-16 に 200 番台へ移してから重なっていない。カートリッジのメニュー ID は 41123 → 41125 と 2 回重なり、
2026-11-13 に `ID_FILE_CART_Y8960` をカートリッジ 1 の範囲の上端 41149 へ移した（ユーザー判断）。範囲は `ID_FILE_CART_OFFSET`（150）で
決まり、カートリッジ 2 は 150 足した 41299。41140-41149 と 41290-41299 は、移した時点で全ソースに使われていなかった
（**確認済み**: `blueMSX/Src` を数字で検索。履歴は 41003-41032、名前付きの項目は 41070-41125）。
**前提**: 上流が番号を下から順に取ること、`ID_FILE_CART_OFFSET` が 150 のままであること。どちらかが変わったら置き場所を見直す。
**割り込みの線（`boardSetInt` のビット）も同じ**で、上流は `0x8000` まで下から取っている。Y8960 の 3 本は 2026-11-13 に上端から下へ
`0x40000000`（MSX-TIMER）・`0x20000000`（OPL2EX）・`0x10000000`（OPM）と並べた（ユーザー判断。`y8960/implementation-plan.md` §9.19）。
ビット 31 は `pendingInt` が符号付きの `int` なので使わない。
Makoto（`feature/makoto-opna`）の 41126 は動かしていない。Makoto の枝は上流に出す PR の枝なので、上流の振り方（次の番号）に揃えたままにしてある。

**`Board/MSX.c` の `machineInitialize` から `msxPsgCreate` までは、上流とフォークの両方が触っている。**
フォークは PSG の生成を `BOARD_MSX2PP` で分岐し、上流は同じ場所に R800 のウェイト設定を置いた
（2026-10-04 の同期で衝突）。上流がこの区間をまた触れば衝突しうる。解くときは両方を残す。

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

**fork からの PR の CI は、最初は所有者の承認を待った。いまは自動で走る。**
PR #77（2026-09-12）では GitHub が `action_required` で止め、上流の所有者の承認を待った
（**確認済み**: `mergeStateStatus` が `BLOCKED`、`mergeable` は `MERGEABLE`）。
**PR #99（2026-10-04）では、作成の数秒後に 4 構成のビルドが走り出した**（**確認済み**:
`gh api .../actions/runs/<id>/jobs` で 4 本とも `in_progress`）。何が変わったのかは確かめていない。

**だから、上流の 4 構成（Debug / Release × x64 / Win32）は PR を出せば確かめられる。**
ローカルで 4 構成を回して結果を PR に書いても、上流の作業は減らない（上流は自分で CI を回す）。
報告のために回す必要はない。PR を出したら `gh pr checks <番号> -R Hesoten/blueMSX-plus` で結果を見る。

### これから出すもの

| | | 状態 |
|---|---|---|
| （出さない） | 本体 MSX-AUDIO の既定の emu8950 バックエンドが、ADPCM の止まったあとも最後の値を出し続け、`07h` = 00h でも止まらない | **PR にも issue にもしない**（2026-10-04 ユーザー判断）。根は digital-sound-antiques/emu8950 に在り、そちらに issue が出ている（#7、#8）。要るなら、それを上流に知らせるだけにする。同 §9.13 |
| 1. Makoto | YM2608（OPNA）カートリッジ。`opna/plan.md` | **出す（2026-11-13 ユーザー）。** 枝 `feature/makoto-opna` が PR の枝。上流 `develop` `55ca0a3f` から 171 コミット遅れで、同期すると `FileHistory.c`・`MediaDb.h`・`MegaromCartridge.c`・`Win32Menu.c` の 4 本で衝突する。どれも上流の V9968 と並ぶ位置の衝突で、番号は重ならない（RomType 188 と 183、メニュー ID 41126 と 41125。**確認済み**: `git merge-tree`、2026-11-13）。**決定（2026-11-13 ユーザー）**: (1) **RomType は 188 のまま出す。** 上流の最大は 183 で 184-187 が空くので、空ける理由（Makoto の配布版 makoto.2〜.4 の利用者の `config.ini`・カートリッジ履歴・セーブステートが 188 を持つ）を PR の本文に書き、`MediaDb.h` の注記も上流の読み手に通じる言い方に直す。空きがあっても上流のコードは困らない（**確認済み(読解)**: `CommandLine.c` の短縮名の走査は `UNKNOWN` を飛ばし、`Machine.c` は上限しか見ない）。前提: 上流が詰めるよう求めないこと。求められたら定数 1 つと注記を直し、配布版の利用者のファイルは壊れる。(2) **ライセンス表示のコミット `b2d4d185` は PR に入れない。** 上流 `a07b26f4` のころに数えた一覧で、その後上流が Dear ImGui（MIT、表示が要る）などを足したので、今の上流には不完全。デバイスの PR に配布物全体の話を混ぜない。**そのため `feature/makoto-opna` からは出さず、`upstream/develop` から新しい枝を切り、残る 8 コミットを積み直す**（`feature/makoto-opna` とリリースのタグはそのまま残す）。見送った案: 今の上流に合わせて直して入れる（配布物全体の話が混ざる）、そのまま入れる（不完全な一覧を渡す）。(3) **割り込みの線を `0x1000` から `0x20` に移し、/IRQ を繋ぐコミットに畳んだ。** 上流が 2026-10-16 に V9990 へ `0x1000` を割り当て（`085c55ef`）、重なっていた（`opna/plan.md` §4.11）。**PR の枝は `feature/makoto-cartridge`**（`upstream/develop` `55ca0a3f` の上に 8 コミット、`4b1d9e31`）。**PR #137 として出した**（2026-11-13、下の「出した記録」）。**マージされた**（2026-11-14 JST）。ビルド・probe 54 項目・エミュレータの中の 6 区間が通った（同 §4.11）。**`Properties.c` で #136 と衝突する**（隣り合う行に両方が足した。**確認済み**: `git merge-tree`）。後に入る方の枝で解く。ミキサーの新しい口を使う部分（`opnafm` / `opnassg`）は、ミキサーが上流に入ってから足す（`mixer/plan.md` §5） |
| 2. サウンドミキサー | 個体ごとの音量・パン・有効と窓。`mixer/plan.md` | **出す（同）。** 上流が PR #122 / #123 でメーターを作り直したので、新しい `develop` の上で作り直した（同日）。作り直しは rebase でなく上流 `develop` のマージで行った（この枝は `feature/y8960` に取り込み済みのため。`mixer/plan.md` §4.4）。マージ（`dc114bdc`）で 9 本が衝突し、両側を残した。**個体ごとのメーターは上流の RMS の形に合わせた（ユーザー判断、`5d8f9a0b`）。** `feature/y8960` にも取り込んだ（`34e99ce6`、`y8960/implementation-plan.md` §11 の (67)）。両方の枝を `origin` に push した。窓のメーターが動くことはユーザーが見た（`mixer/plan.md` §5「人が聴いて確かめること」の 5）。**PR #136 として出した**（2026-11-13、下の「出した記録」）。**上流は取り込まないと返事した**（同日。上流が別の設計でミキサーを作り直しているため）。フォークのミキサー（`feature/sound-mixer` と、`feature/y8960` に取り込んだ分）をどうするかは**未決** |
| 3. Y8960 | Y8960 カートリッジのエミュレーション | **出す（同）。** **これまで上流に出すと決めた記述は無い**（`y8960/implementation-plan.md` と本表を 2026-11-13 に検索。上流に出したのは I/O ポートの多重化とキー注入だけ）。枝も無いので、`upstream/develop` から切り直してコードだけを載せる（下の「上流へ出すとき」）。**未決**: (a) RomType 200-208 とメニュー ID 41149 を上流の振り方（次の番号）に合わせるか。合わせるとフォークの利用者の `config.ini`・履歴・ステートが壊れる（2026-10-16 の移し替えと同じ種類の破壊）。(b) ミキサーの新しい口（種別 `y8960` と、5 ブロックの名前付きの登録）に依るので、ミキサーの PR を先に通すか、その部分を外して出すか。**決定: Y8960 の PR は #136 が上流に入ってから出す**（2026-11-13 ユーザー。#136 の本文にもそう書いた）。
見送った案: 同時に出す。出し方は 2 通りで、ミキサーの枝の上に積むと #136 が入るまで差分にミキサーの 8 コミットが混ざり
（PR の出し先は上流のリポジトリの枝に限られる）、ミキサーに依る部分を外して出すと名前付きの登録に戻す PR が 1 本増える。
前提: Y8960 がミキサーの新しい口に依ること。
**この決定の前提は崩れた**: #136 は取り込まれない（2026-11-13 の上流の返事）。上流は Y8960 の PR を #136 に依らない形で
`develop` に送るよう求め、条件を 3 つ付けた（下の「出した記録」の #136）。条件の 3「ROM の種別はカートリッジ全体だけ」は (a) に関わる
（200-206 と 208 は PR に入れない）。出し方は**未決**。(c) ハードウェアが開発中なので、仕様が変わるたびに追いかけの PR が要る。
**割り込みの線は PR の前に直した**: MSX-TIMER・OPL2EX・OPM の 3 本が上流の V9990 / V9968 と重なっていたので、上端へ移した（2026-11-13、`y8960/implementation-plan.md` §9.19、§11 の (68)） |

**順番はミキサー → Makoto → Y8960（2026-11-13 ユーザー）。Makoto はミキサーに依らないので、ミキサーを待たずに出してよい（同）。**
ミキサーを先にするのは、上流との衝突が上流の手の入っている所（`AudioMixer.c` のメーター）にあり、遅らせるほど重くなるため。
Makoto の衝突は上流の新しい装置の隣に並ぶ形で、同期のたびに出ても軽い。Y8960 はミキサーに依り、外に出る番号を決め直すので最後。
Makoto の種別名（`opnafm` / `opnassg`）は、ミキサーと Makoto のうち後に上流に入る方の枝で足す。
見送った案: Makoto → ミキサー → Y8960（クロコの最初の提案）。理由に挙げた「衝突が軽く、ミキサーに依らない」は、いつ出してもよい理由で、先に出す理由ではなかった。
前提: 上流の `AudioMixer.c` との衝突がミキサーの枝の方が重いこと（`git merge-tree` で 9 本と 4 本、2026-11-13）。
**2026-11-13 にミキサーが取り込まれないと決まり、上の順番と、種別名を後に入る方の枝で足す件は前提を失った。** Makoto は入った（#137）。
`opnafm` / `opnassg` の名前を使うのはフォークのミキサーだけになる。
**1 本につき 1 セッションで進める**（同日の提案）。着手する前に、その行の「未決」をユーザーに聞く。

### 出した記録

| | | 状態（2026-10-05 に確認） |
|---|---|---|
| PR #77 | I/O ポートの解放が登録と食い違う 3 件。`fix/ioport-unregister-mismatch` | **上流 `develop` にマージされた**（マージコミット `178eaf03`） |
| issue #78 | 1 ポートに 1 デバイスしか登録できない件。多重化の提案 | OPEN のまま。PR #80 がマージされたので中身は解決している |
| PR #80 | #78 の実装（`IoPort` の多重化と `ioPortUnregister` の `ref`）。`feature/ioport-multi-claim` | **マージされた**（`fddc5683`） |
| PR #81 | キーマトリクスの注入と `/hidden`。`feature/key-matrix-input` | **マージされた**（`fd6e7e53`）。レビューで `w 0` の不具合の修正を求められ、応じた（`automation/plan.md` §4）。`matrix[][]` は上流がマージ後に `MsxPPI.h` の記述から組む形に整理した（`3aeb35b7`、`6f58b6d1`） |
| PR #99 | 本体 MSX-AUDIO の openMSX バックエンドが、ADPCM のサンプルを時刻どおりに終えるようにする。`fix/y8950-adpcm-end-of-sample`（`upstream/develop` `83d8f0f1` の上に 2 コミット、ソース 3 本） | **マージされた**（`ae181356`、2026-10-05。2026-10-04 作成）。出した 2 コミット（`5f751215`、`ab8f6774`）のまま入り、`6a484779` で受け取った。修正の内容と試験は `y8960/implementation-plan.md` §11 の (35)。Y8960 の OPL2EX を突き合わせる途中で見つけたので、記録はそこにある |
| issue #100 | Z80 を標準の 2 倍・3 倍に速くした MSX2+ の機種が、起動ロゴで止まる件。設定の「VDPコマンド待ち時間」を下げると起動する。**上流の PR #67 の `d86414f5`（LMMC に 1 バイトごとの待ちを足したコミット）から止まるようになった。** このフォークの変更とは無関係で、手を入れていない上流の先頭（`83d8f0f1`）で再現する | **閉じた**（2026-10-05、"not planned"。2026-10-04 作成。同じ日に、版ごとの結果を確かめて本文と題名を書き直した）。**上流の返事（同日）: 実機に近づけた変更の影響で、Z80 を上げた比率に合わせて「VDPコマンド待ち時間」を下げて使う、との案内。直すとは書かれていない。** **不具合とは扱わない**（2026-10-05 ユーザー判断。実機の CPU クロックは公式の高速モードでも 6.144 MHz までで、止まるのはそれを超える 7.159 MHz 以上）。本文は #78 と同じ形（日本語を先、英語を `<details>`、題名は英語）。実機でどうなるかが分からないので、不具合だとは書いていない。確かめたこと・書いたこと・書かなかったことは `opna/plan.md` §4.9（Makoto の試験の途中で見つけたので、記録はそこにある） |
| issue #74 | PSG のレジスタ番号を 4bit で丸めている件 | OPEN のまま。上流は PR #75（`be45fc46`）で直しており、本フォークは cherry-pick 済み |
| PR #136 | サウンドミキサー（個体ごとの音量・パン・有効と窓、個体のメーターを RMS に）。`feature/sound-mixer`（`upstream/develop` `55ca0a3f` の上、8 コミット、`5d8f9a0b`） | **OPEN**（2026-11-13 作成。`MERGEABLE`。CI の 4 構成（Debug / Release × Win32 / x64）がすべて pass、**確認済み**: `gh pr checks`）。本文は日本語を先、英語を `<details>`。目的の 2 点（`mixer/plan.md` §0）と、Y8960 の PR をこれが入った後に出す予定であることを書いた（ユーザーの指示）。**取り込まれない**（2026-11-13 の上流の返事。PR は OPEN のまま、2026-11-14 に確認）。理由は、音源ごと・チャネルごとのミキサーを上流が別の設計で進めていること。同じ返事で、Y8960 の PR をこれに依らない形で `develop` に送るよう求められた。条件は 3 つ: (1) 登録は `mixerRegisterChannel`（5 ブロックを別々に登録する形はそのままでよい）、(2) `Properties.c` で `propGetMixerTypeName` に `"y8960"` を足す行を外す、(3) ROM の種別はカートリッジ全体（`ROM_Y8960`）だけにする。上流の新しいミキサーのために Dear ImGui の窓が入った、とユーザーは聞いている（2026-11-14。上流のリポジトリの中には書かれていない: PR #133 の本文、ImGui のコミット 6 本、ImGui のソースを `mixer` で検索）。返事と閉じ方は**未決** |
| PR #137 | Makoto（YM2608 / OPNA カートリッジ）。`feature/makoto-cartridge`（`upstream/develop` `55ca0a3f` の上、8 コミット、`4b1d9e31`） | **OPEN**（2026-11-13 作成。`MERGEABLE`。CI の 4 構成（Debug / Release × Win32 / x64）がすべて pass、**確認済み**: `gh pr checks`）。本文は日本語を先、英語を `<details>`。下書きをユーザーが確認した。RomType 188 を保つ理由と、#136 と `Properties.c` で衝突して後に入る方で解くことを書いた。ライセンス表示（`b2d4d185`）は入れていない（上の「これから出すもの」の 1）。**マージされた**（2026-11-14 JST、マージコミット `e5b6f5b4`。出した `4b1d9e31` のまま入った）。上流はその後、Makoto のファイルに 10 コミットを足した（同日。取り外しで解放する、BUSY が 100 秒で張り付く、ステートの読み込み 3 件、デバッガの表示 2 件、ymfm まわり 2 件、割り込みの線を `BOARD_INT_MAKOTO` の名前にする）。**`feature/makoto-opna` にはどれも入っていない。** **`feature/y8960` には Makoto が入っていない**ので、次の `develop` のマージで初めて入る |
| issue #138 | VDP の垂直帰線と RS-232C（MSX と NMS1210）が割り込みの線 `0x01` を共有し、互いの保留を消す件。オリジナルの blueMSX から | **OPEN**（2026-11-13 作成）。本文は日本語を先、英語を `<details>`。コードを読んだ見立てで、走らせていないと書いた。提案は RS-232C を空いている `0x80` へ移すことと、使用中の線の一覧を 1 か所にまとめること。Y8960 の線を上端へ移したことと、Y8960 の PR をその値で出す予定も添えた。SFG と Yamaha NET の共有（`0x40`、`0x800`）は、実機で同時に使えないので入れていない（ユーザー判断）。経緯は `y8960/implementation-plan.md` §11 の (68)。**2026-11-14 の時点で OPEN、コメントは無い。ただし提案の 2 つはどちらも上流に入った**: PR #146（`fix/board-interrupt-bit-table`、2026-11-14 JST マージ）が `Board.h` に線の表 `BOARD_INT_*` を置き、`Board.c` に表の名前どうしの重なりでビルドを落とす検査を足した。MSX の RS-232C は `BOARD_INT_MSX_RS232` = `0x80` に移った（`ec3ac28c`）。NMS1210 の割り込みの処理は、呼ぶ者がいないとして消された（`fc390a52`）。#146 は #138 に触れていない。**次の同期では、Y8960 の 3 本の線をこの表と検査に足す**（検査は表に並べた名前しか見ないので、足さなくてもビルドは通る。**確認済み(読解)**: `2f26d0a1` の `BOARD_INT_ALL`）。`check-irq-lines.py` が `BOARD_INT_*` の名前を読めるかは見ていない |

PR #77 と issue #78 は本文を**日本語を先、英語を後**（`<details>` に格納）。
上流の所有者と報告者がどちらも日本語話者で、かつ公開リポジトリに
海外の利用者もいるため。**タイトルは英語**（既存の issue / PR に揃えた）。

**上流に出す日本語では、board を「板」と書かない。** board time は「ボードクロック」と書く
（2026-10-04 ユーザー指摘。「板時刻」は日本語として不自然なため。エミュレータの中の時刻なので、実時間を指す
「壁時計」にはしない）。PR #99 の本文は同じ日に直した（板時刻 → ボードクロック、板のタイマ → ボードのタイマ、
スタブの板 → スタブのボード。英語の部分は変えていない）。
**AI 向けの文書に在る「板時刻」は、そのままでよい**（同日ユーザー判断。AI が理解できれば足りる）。

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

`origin` の GitHub Releases に出す。決めたことは次のとおり（2026-09-15 ユーザー判断。公開の流れは 2026-10-18 に上流の変更に合わせた）。

| | |
|---|---|
| タグ名 | `v<土台の blueMSX+ の版>-y8960.<通し番号>`。1 本目は `v3.1.1-y8960.1`。Makoto 対応（`feature/makoto-opna`）は同じ形で `-makoto.<通し番号>`（2026-09-22 ユーザー判断） |
| pre-release | Y8960 のハードウェアが開発中である間は pre-release で出す |
| ビルド | **pre-release を作ると**、上流由来の `.github/workflows/prerelease.yml` が Final × x64 / Win32 をビルドし、zip を添付する。zip の名前は `blueMSX+-v<版>-<枝>-<コミット 7 桁>-<x64/Win32>.zip`。**`release.yml` は `main` の上の `vX.Y.Z` タグしか公開しない**（上流の `08810742` から）。フォークのタグを push しても、`release.yml` は何も公開せずに終わる |
| 作り方 | **下書きを経ずに作る。** `gh release create <タグ> --prerelease --target feature/y8960 --title "blueMSX+ <タグ>" --notes-file <ノート> -R madscient/blueMSX-plus_Y8960`。`prerelease.yml` は `release` の `prereleased` でだけ起動し、**`prereleased` は下書きから公開した pre-release では発火しない**（GitHub の文書「Events that trigger workflows」の release の節。2026-10-18 に読んだ）。ノートの下見はローカルのファイルで済ませる。**`gh` の既定のリポジトリはマシンごとに違い、未設定だと上流（`Hesoten/blueMSX-plus`）を選ぶので、`-R` を必ず付ける** |
| バイナリ zip | `scripts/ci-package.ps1` が組む。`ReleaseFiles/`・実行ファイル・プラグインだけで、**`doc/` は入らない**（**確認済み(読解)**） |
| ソースアーカイブ | GitHub が自動で添付する。`.gitattributes` の `export-ignore` で **`CLAUDE.md` と `doc/fork/` を除き、`doc/fork/y8960/user-guide.md` だけを戻す** |
| 使い方 | zip には同梱しない。リリースノートから、そのタグの `user-guide.md` へリンクする |

**前提**: 上流の `prerelease.yml` と `ci-package.ps1` をそのまま使っていること。
上流がこれらを変えたら、公開の流れ（`release: prereleased` で起動）と zip の中身を見直す。

**2026-11-13 の同期（`55ca0a3f`）で上流が両方を変えた。** 起動の条件は変わっていない。ビルドのターゲットに `McpServer` が加わり、
zip の `Tools/` に `bluemsx-mcp.exe` と、その案内 `bluemsx-mcp.md` / `bluemsx-mcp.ja.md`（`blueMSX/McpServer/README{,.ja}.md` を改名したもの）が入るようになった
（**確認済み(読解)**: `git diff 0d6ba91b 55ca0a3f -- .github/workflows/prerelease.yml scripts/ci-package.ps1`）。`doc/` が入らないことは変わらない。
この形の zip はまだ公開していない。

**`prerelease.yml` はフォークの `main` にも置いてある**（`06e81676`。上流 `develop` のものと同一）。
フォークの Actions の一覧に `prerelease` が出るのは、`main` に置いてからである。**`main` に無くても
pre-release で走るかは確かめていない。** 上流は `main` に無い状態で走っている。

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

**リリースノートは下書きのときに書く。** `prerelease.yml` はノートに触れない。
y8960.2 までの流れ（`release.yml` が公開する）では自動生成のノートを公開後に差し替えていた。

### リリースの記録

| タグ | コミット | 確かめたこと |
|---|---|---|
| `v3.1.1-y8960.1`（**下書きに戻した**。ライセンス表示を欠くため） | `61f40fe7` | Actions の run 34860219682 で x64 / Win32 のビルドと公開が成功。**公開物を落として確かめた**（**確認済み**）: バイナリ zip 2 本に `doc/` と `CLAUDE.md` が無い。GitHub のソースアーカイブ（zip / tar.gz）は `export-ignore` に従い、`doc/` 以下は `user-guide.md` だけ。x64 / Win32 の実行ファイルとも `/listromtypes` と `/listmachines` が終了コード 0 で、前者に `Y8960SCC` が出た。**Final 構成で音や画面を確かめたことはない**（手元の確認はすべて Release 構成） |
| `v3.1.1-y8960.6`（pre-release、2026-11-04） | `12e8a2ba` | y8960.5 以降の Y8960 の仕様追随（Y8960_Cartridge v0.9.5〜v0.9.9。System Controller（RomType 208）、MSX-TIMER のクロックを 42.95454MHz に、OPL2EX の読み出しと ROM ビットと SP-OFF、SSGS の `A2h` と 6 ビットの番号とリセット値）。上流からの取り込みは無い。**ノートに v0.9.9 へのリンクと、MSX-TIMER の進み方が y8960.5 の半分になった注意を書いた。** run 41788300197 で成功（同時に起動した `release.yml` の run 41788299754 も成功し、何も公開していない）。`--target` に短縮 SHA を渡すと 422 で拒まれたので、手順どおり枝名で作った（作る直前に `git ls-remote` で枝先が `12e8a2ba` であることを見た。タグは `12e8a2ba` を指す）。**公開物を落として確かめた**（**確認済み**）: zip 2 本（`blueMSX+-v3.1.1-feature-y8960-12e8a2b-{x64,Win32}.zip`）の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/` と `CLAUDE.md` は無い。ソースアーカイブ（zip / tar.gz）の `doc/` は `user-guide.md` だけで、`CLAUDE.md` は無い。x64 / Win32 とも `/listromtypes` と `/listspecials` が終了コード 0 で、`Y8960SCC` と `Y8960 Cartridge` が出た。**x64 / Win32 の exe（Final 構成）で、208 を含む `(banktest)` の構成と、特殊カートリッジ（`(whole cart)` に `/romtype1 Y8960`）の両方で `banktest` の 1-7・13-24 が OK**（`/hidden` で 4 つの時刻に写した。9・11・12 は聴く項目で、聴いていない）。**10 は NG**: 使った `banktest.rom` は y8960.5 のときと同じ 10-26 の版（(58) の前）で、10 が `C1h` からレジスタを読み戻す。この版は `C1h` で `0Fh` 以外に `FFh` を返すので、NG はこの版の仕様どおり（y8960.5 では同じ ROM で OK だった）。**試験 ROM はこの日も組めなかった**（`z80asm` も `z88dk-z80asm` も 2 行のソースで戻らない。`doc/fork/build/README.md` の 2026-11-04）。そのため **`v099test`・`sndtest`・`adpcmtest` は配布物で走らせていない**。この版で変わった動作は開発用のビルドでだけ見ている（`y8960/implementation-plan.md` §11 の (58)-(60)）。ノートの注意の 3 行目はそう書いた。ログインせずに見て、リリースのページ・zip 2 本・タグの `user-guide.md`・v0.9.9 のページが 200（無いタグは 404）。y8960.5 は公開したまま。makoto.4 のノートの Y8960 へのリンク（日本語と英語の 2 か所）を y8960.6 に向け直した（一度は依頼の範囲外として止められ、ユーザーに聞いてから直した。ほかの行は変えていない。読み戻した本文は末尾の改行 1 個を除いて送ったものと一致。ページは 200）。**試験 ROM の代わりにファームウェア（MSSE 0.5.5 の `y8960bas.rom`）を x64 / Win32 の exe で動かした**: `MINIT` と `MPLAY` が通り、SCC が鳴った。**ただし MSX-TIMER で進む演奏（分解能 2）は全音符が 4.0 秒で、VDP で進む演奏（分解能 0）の 2.0 秒の倍になった。** ファームウェアが MSX-TIMER を 85.909MHz として周期を決めているため（`y8960/implementation-plan.md` §11 の (61)）。**ノートの注意の先頭に、MSSE v0.5.5 以前の `y8960bas.rom` では分解能 1・2 で半分の速さになる旨を足した**（日本語と英語。ユーザー判断。読み戻した本文は末尾の改行 1 個を除いて送ったものと一致） |
| `v3.1.1-y8960.5`（pre-release、2026-10-27） | `35b844c3` | y8960.4 以降の Y8960 の仕様追随（Y8960_Cartridge v0.9.4。v0.9.3 の「RAM モードで BANK1 の SCC の上に窓を出さない」と、v0.9.4 の `7FFFh` の読み出し）と上流 `0d6ba91b` までの取り込み。**ノートに v0.9.4 へのリンクと、BANK1 の SCC の上の窓の動作が y8960.4 から逆になった注意を書いた。** run 40418821346 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本（`blueMSX+-v3.1.1-feature-y8960-35b844c-{x64,Win32}.zip`）の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/` と `CLAUDE.md` は無い。ソースアーカイブ（zip / tar.gz）の `doc/` は `user-guide.md` だけで、`CLAUDE.md` は無い。x64 / Win32 とも `/listromtypes` と `/listspecials` が終了コード 0 で、`Y8960SCC` と `Y8960 Cartridge` が出た。**x64 / Win32 の exe（Final 構成）で `sndtest` が PASS**（SCC の A-N と OPM の O-Q）、**`banktest` の 1-7・10・13-24 が OK**（`/hidden` で 4 つの時刻に写した。9・11・12 は聴く項目で、聴いていない）。**試験 ROM はこの日に組めなかった**（`z80asm` が 2 バイトのソースでも終わらなかった。原因は調べていない）。`sndtest.rom` は前のセッションが 10-21 11:05 に組んだもので、`sndtest.asm` の最後のコミット（10-21 00:53）より後（**推測**: 時刻からで、組み直して比べてはいない）。`banktest.rom` は出力先にあった 10-26 07:41 のもので、`banktest.asm` の最後のコミット（(55)、10-26 07:55）より前に組まれているが、24 が出るので (55) の作業ツリーから組んだ版と見ている（**推測**: コミット前の 14 分にソースが変わっていないかは確かめていない）。ログインせずに見て、リリースのページ・zip 2 本・タグの `user-guide.md`・v0.9.4 のページが 200（無いタグは 404）。y8960.4 は公開したまま。makoto.4 のノートの Y8960 へのリンク（日本語と英語の 2 か所）を y8960.5 に向け直した（ほかの行は変えていない。読み戻した本文は末尾の改行 1 個を除いて送ったものと一致） |
| `v3.1.1-y8960.4`（pre-release、2026-10-21） | `4a0259d0` | y8960.3 以降の Y8960 の仕様追随（Y8960_Cartridge v0.9.2。SCC の定位、窓の `7FF0h`-`7FFFh` と出る条件、OPM を RomType 206 で追加）、作り直したミキサー（ツール → ミキサー（個体ごと））、上流 `475dadfa` までの取り込み。**ノートに v0.9.2 へのリンクと、206 が DCSG の番号だった注意を書いた。** run 39464749198 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本（`blueMSX+-v3.1.1-feature-y8960-4a0259d-{x64,Win32}.zip`）の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/` と `CLAUDE.md` は無い。ソースアーカイブ（zip / tar.gz）の `doc/` は `user-guide.md` だけで、`CLAUDE.md` は無い。x64 / Win32 とも `/listromtypes` と `/listspecials` が終了コード 0 で、`Y8960SCC` と `Y8960 Cartridge` が出た。**x64 / Win32 の exe（Final 構成）で `sndtest` が PASS**（SCC の A-N と OPM の O-Q）、**`banktest` の 13-23 が OK**（画面に残った分。1-12 は写していない。`(cartridge)` と `(banktest)` の構成を足した）。ログインせずに見て、リリースのページ・zip 2 本・タグの `user-guide.md`・v0.9.2 のページが 200。ノートの注意の 3 行目を確かめた結果に書き換えた。y8960.3 は公開したまま。makoto.4 のノートの Y8960 へのリンク（日本語と英語の 2 か所）を y8960.4 に向け直した（ほかの行は変えていない。ページは 200） |
| `v3.1.1-y8960.3`（pre-release、2026-10-18） | `0f91ba65` | y8960.2 以降の Y8960 の仕様追随（2026-10-16 / 10-17 / 10-18 版）と上流 `96771382` までの取り込み。**RomType を 200-207 に移した破壊的変更を含む**（ノートに「20 を足す」移行手順）。run 39055851855 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本（`blueMSX+-v3.1.1-feature-y8960-0f91ba6-{x64,Win32}.zip`）の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/` と `CLAUDE.md` は無い。ソースアーカイブの `doc/` は `user-guide.md` だけで、`CLAUDE.md` は無い。x64 / Win32 とも `/listromtypes` と `/listspecials` が終了コード 0 で、`Y8960SCC` と `Y8960 Cartridge` が出た。**x64 / Win32 の exe（Final 構成）で `sndtest` が PASS**（1.00 / 0.50 / 1.00 / 0.50 / 0.25。`(cartridge)` の構成を足した）。ログインせずに見て、リリースのページ・zip 2 本・タグの `user-guide.md` が 200。ノートの注意の 3 行目を確かめた結果に書き換えた。y8960.2 は公開したまま。makoto.4 のノートの Y8960 へのリンク（日本語と英語の 2 か所）を y8960.3 に向け直した |
| `v3.1.1-y8960.2`（pre-release） | `337c23a3` | y8960.1 以降の `feature/y8960` 全体（Y8960 一式のカートリッジ、開始ページの修正、上流 `develop` `5b97360e` の取り込み）にライセンス文書を足した版。run 35730812428 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本に `LICENSE.txt` と `Licenses/` の 7 本、`LICENSE.txt` に OPLLEX の音色データ（CC BY-SA）の表示。`doc/` と `CLAUDE.md` は無い。ソースアーカイブの `doc/` は `user-guide.md` だけ（新しく足した `doc/fork/opna/` も外れている）。x64 / Win32 とも `/listspecials` に `Y8960 Cartridge`。ログインせずに見て、y8960.1 の zip の URL は 404。makoto.2 のノートの Y8960 へのリンクを y8960.2 に向け直した |
| `v3.1.1-makoto.1`（**下書きに戻した**。ライセンス表示を欠くため） | `2ade339f`（`feature/makoto-opna`） | Makoto 対応。Y8960 は含まない。Actions の run 35725735949 で成功。**公開物を落として確かめた**（**確認済み**）: バイナリ zip 2 本とソースアーカイブに `doc/` と `CLAUDE.md` が無い（枝にもともと無い）。リズム ROM も入っていない。x64 / Win32 とも `/listspecials` に `MAKOTO` が出た。使い方とリズム ROM は利用者が用意する旨はリリースノートに書いた（枝に利用者向け文書を置かないため）。**ymfm の BSD-3-Clause の表示はバイナリ zip に入っていない**（`doc/fork/opna/plan.md` §5） |
| `v3.1.1-makoto.2`（pre-release） | `b2d4d185`（`feature/makoto-opna`） | makoto.1 にライセンス文書（`LICENSE.txt` と `Licenses/` の 7 本）を足した版。Actions の run 35728110343 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/`・`CLAUDE.md`・リズム ROM は無い。x64 / Win32 とも `/listspecials` に `MAKOTO`。ログインせずに見て、リリース一覧は makoto.2 だけ、makoto.1 の zip の URL は 404（タグのページは注釈だけを出す） |
| `v3.1.1-makoto.3`（pre-release） | `588325e5`（`feature/makoto-opna`） | FM と SSG を実レートで回して sinc で変換する版（`doc/fork/opna/plan.md` §4.5）。Actions の run 37149676836 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/`・`CLAUDE.md` は無く、リズム ROM も無い（8,192 バイトのファイルが 1 本も無い）。ソースアーカイブにも `doc/` と `CLAUDE.md` は無い。x64 / Win32 とも `/listspecials` に `MAKOTO`。**x64 / Win32 の exe（Final 構成）で試験 ROM を走らせ、6 区間すべて OK**（リズム ROM だけを足した。Final 構成と Win32 で音を確かめたのはこの版が初めて）。ログインせずに見て、リリースのページと x64 の zip が 200。リリースの題名は自動で `v3.1.1-makoto.3` になるので、`blueMSX+ v3.1.1-makoto.3` に直した。makoto.2 は公開したまま |
| `v3.1.1-makoto.4`（pre-release） | `f7a4d0f8`（`feature/makoto-opna`） | ymfm の ADPCM-B に上流の PR #40 を入れ、FM と SSG を別々の種別のミキサーチャンネルにした版（`doc/fork/opna/plan.md` §4.6、§4.7）。Actions の run 37155442592 で成功。**公開物を落として確かめた**（**確認済み**）: zip 2 本の最上位に `LICENSE.txt`、`Licenses/` に 7 本。`doc/`・`CLAUDE.md` は無く、リズム ROM も無い（8,192 バイトのファイルが 1 本も無い）。ソースアーカイブにも `doc/` と `CLAUDE.md` は無い。x64 / Win32 とも `/listspecials` に `MAKOTO`。**x64 / Win32 の exe（Final 構成）で試験 ROM の 6 区間がすべて OK**（リズム ROM だけを足した）。x64 の exe で、`bluemsx.ini` の `MIXER_CHANNEL_OPNA_SSG.enable=no` で SSG の区間だけが消え、`MIXER_CHANNEL_OPNA_FM.enable=no` で FM とリズムの区間だけが消えた。x64 の exe で、実機 BIOS の機種（`MSX2+ - Sony HB-F1XDJ`）でも 6 区間 OK。ログインせずに見て、リリースのページと zip 2 本が 200。題名を `blueMSX+ v3.1.1-makoto.4` に直した。makoto.2 と makoto.3 は公開したまま |

## Y8960 の実装状況

**8 ブロックが `RomType` を持ち、マシン構成に個別に置ける。すべてに中身が入っている。**
DCSG と、OPLLEX / OPL2EX の 2 回路目は 2026-10-16 の仕様書でハードウェアから消え、外した。
OPM は 2026-10-20 の仕様書で増え、DCSG の番号だった RomType 206 を使っている。
System Controller はハードウェアの v0.9.9 に追随したときに RomType 208 で足した（2026-11-04）。

| ブロック | 状態 | 土台 |
|---|---|---|
| OPLLEX | 実装済み・**音を確認** | `Src/SoundChips/Emu2413/`（emu2413 v1.5.9）のフォーク。レート変換器は v1.6.4 のもの |
| OPL2EX | 実装済み・**音を確認** | `Src/SoundChips/OpenMsxY8950Latest.cpp` ほか（openMSX の Y8950 の移植版）のフォーク |
| SSGS | 実装済み・**音を確認** | `Src/SoundChips/AY8910.c` のフォーク |
| MSX-TIMER | 実装済み・**音は持たない** | 新規 |
| OPM | 実装済み・**音程・音量・左右を録音で確認** | `Src/SoundChips/MameYM2151.c` と `ym2151.c` のフォーク |
| MSX-MIXER | 実装済み・**SCC と OPM の音量、マスターを録音で確認**（窓の `7FF7h`-`7FFAh`、`7FFDh`、`7FFEh`） | 新規 |
| Y8960 SCC + マッパー | 実装済み・**音とチャンネル別の定位を録音で確認** | `Src/SoundChips/Y8960Scc.c`（`SCC.c` のフォーク）+ 新規マッパー |
| I/O イネーブラ / MMIO 窓 | 実装済み | SCC のマッパーが持つ |
| System Controller | 実装済み・**I/O 40h-42h の識別だけ**。Config ROM は持たない | 新規（switched I/O の ID 80h） |

**OPM 以外の 4 つの音源ブロックを、エミュレータの中で人が聴いて確かめた。** OPM は録音からの判定だけ。
SCC は録音からも判定している（`y8960/tests/sndtest.asm`）。
**タイマ割り込みが CPU に届くことも確かめた**（`banktest` の 19。OPM のタイマは 23）。
詳細は `y8960/implementation-plan.md` の §0 と各 §「確かめたこと」。

**カートリッジ内の音源ブロックは、SCC そのものではなく等価回路である。**
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
| `Src/SoundChips/AY8910.c` / `SCC.c` | GPLv2（blueMSX 由来） |
| `Src/SoundChips/ym2151.c` | GPLv2（blueMSX 由来） |
| `Src/SoundChips/MameYM2151.c` | 明記なし（MAME の YM2151 の旧版。Jarek Burczynski）。blueMSX に同梱されている条件のまま |

`Src/SoundChips/Y8960OpllCore.c` は Emu2413 のフォークなので MIT 表示を引き継ぎ、
音色データの CC BY-SA 表示を併せて持つ。

**このファイルの中身は、書いた人ごとに条件が違う。** GPLv2 のリポジトリに在ることは、
各部分の条件を変えない。

| 部分 | 書いた人 | 条件 |
|---|---|---|
| emu2413 本体 | Mitsutaka Okazaki | MIT（`Src/SoundChips/Emu2413/LICENSE`） |
| セーブステート用の 2 関数と変更表記 | Hesoten（blueMSX+） | 明記なし。GPLv2 のプロジェクトの中で書かれた |
| Y8960 向けの変更（4 面の音色表の器、バンクレジスタ、改名） | madscient | このフォークの著者自身 |
| 音色データ | David Viens、Hubert Lamontagne | CC BY-SA。出典は版を書いていない |

**このファイルを GPL でないリポジトリへ持ち出すときは、Hesoten の部分を外す。**
madscient/DSAemuEngine（MIT）の写しはそうなっている（**確認済み** 2026-10-04: 2 つのファイルの差は、
変更表記、`Y8960OPLL_RateConv_getBufferLength`、`Y8960OPLL_relinkAfterRestore` と、コメントの
言い回し 3 か所だけ）。**openMSX 由来の OPL2EX のコア（`Y8960Opl2Core.*` / `Y8960Opl2Adpcm.*`）は
持ち出せない。** GPL でしか許諾されていない。

**音色データの「継承」がプログラムの側に及ぶかは、作者の意向が分かっていない。**
出典を明らかにしたまま、作者の反応を待つ（2026-10-04 ユーザー判断。
`y8960/implementation-plan.md` §9.14）。配布物の `LICENSE.txt` は、データは CC BY-SA のまま、
という立場で書いてある。

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
| [madscient/EPSGemuEngine](https://github.com/madscient/EPSGemuEngine) | YMZ705/732/771 系 SSG の実装。MIT + BSD-3 | SSGS の旧配置（2026-09-12 から 2026-10-16 まで）とパン則の出所。今の配置は仕様書による |
| [madscient/MsxSoundSuiteExtension](https://github.com/madscient/MsxSoundSuiteExtension) | **Y8960 を駆動する拡張BASIC と BIOS**。開発中 | **ファームウェアが前提にしている仕様の出所。** `y8960bas.rom` は検証に使えるが、**ROM をリポジトリに入れてはいけない** |
