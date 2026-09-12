# ビルド環境

**ビルド手順の一次情報は `.github/workflows/ci.yml` である。**
CI が実際に通している唯一の構成なので、ローカルもそれに揃える。
本書はそこに書かれていない、**実際にビルドしたマシンの事情と、
そこで詰まった点の記録**である。

ビルド実績のあるマシンを OS の版で呼び分ける。**どの記述も
そのマシンで実際に踏んだことの記録**であって、一般則ではない。
新しいマシンで始めるときは §6 を見ること。

| 呼び名 | OS |
|---|---|
| **Win10 機** | Windows 10 Pro 19045 / x64 |

## 1. 環境（Win10 機、2026-09-12 時点）

すべて **確認済み**（その場で実行して確かめた）。

| 項目 | 値 | 確認方法 |
|---|---|---|
| Visual Studio | Community 2022 (17.x) と Community 2026 (18.x) が併存 | `vswhere -all -products * -property installationPath` |
| MSBuild (VS2022) | `...\2022\Community\MSBuild\Current\Bin\MSBuild.exe` | `vswhere -version "[17.0,18.0)" -find "MSBuild\**\Bin\MSBuild.exe"` |
| 利用可能な PlatformToolset (x64) | VS2022 が **v143** | `MSBuild\Microsoft\VC\v170\Platforms\x64\PlatformToolsets` を列挙 |
| MSVC ツールチェイン | 14.44.35207（ビルドログの CL.exe のパスによる） | ビルドログ |
| vcpkg のグローバル統合 | **あり**（`%LOCALAPPDATA%\vcpkg\vcpkg.user.props` が `C:\vcpkg` を読み込む） | 同ファイルを読んだ |
| vcpkg のインストール済みライブラリ | **無し**（`C:\vcpkg\installed` にトリプレットが無い） | 同ディレクトリを列挙 |

**ツールセットは v143 を選んだ。** 理由は CI（`windows-2022`）と同じになること。
落ちたときに「CI では通る」が基準として使える。

`blueMSX/Make/msvc2026/` の vcxproj は **v145** を指定しており、
VS18 の MSBuild で使える。**こちらは試していない。**

## 2. 手順

```sh
# MSBuild の場所はマシンごとに違うので決め打ちにしない。
# v143 が要るので VS2022 のものを名指しで取る（VS18 の MSBuild には v143 が無い）。
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
MSBUILD=$("$VSWHERE" -products '*' -version '[17.0,18.0)' \
          -requires Microsoft.Component.MSBuild \
          -find 'MSBuild/**/Bin/MSBuild.exe' | head -1)

cd blueMSX/Make/msvc2022
"$MSBUILD" blueMSX.sln /nologo /m \
  '/t:blueMSX;TraceWindow;Trainer;SimpleDebugger' \
  '/p:Configuration=Release;Platform=x64;BlueMsxPackUpx=false' \
  /p:VcpkgEnabled=false > build.log
```

**`-p:PlatformToolset` は渡していない。** `msvc2022` の vcxproj が v143 を
指定しており、VS2022 にそれが在るため（§1）。無いマシンでの対処は §6。

**3rdparty のダウンロードは要らない。** 依存はリポジトリに同梱されている。

| 引数 | なぜ要るか |
|---|---|
| `/t:blueMSX;TraceWindow;Trainer;SimpleDebugger` | CI と同じターゲット。ソリューションには `DeviceViewer` も在るが、**意図的に外してある**（`scripts/ci-package.ps1` の冒頭に明記） |
| `BlueMsxPackUpx=false` | UPX 圧縮を切る。CI も切っている |
| `VcpkgEnabled=false` | §3.1 |

Configuration は `Debug` / `Release` / `Final` の 3 つ、Platform は `x64` / `Win32`
（**確認済み(読解)**: `blueMSX.sln` の構成一覧）。openMSX のような
`Developer` 構成は無い。

**`msbuild ... | tail` のようにパイプで繋がないこと。** パイプラインの終了コードは
`tail` のものになるので、ビルドが失敗しても後続の `&&` が通ってしまう。
ログはファイルにリダイレクトして、終了コードを直接見る。

### 出力先

| | |
|---|---|
| 実行ファイル | `blueMSX/Make/msvc2022/x64/Release/blueMSX+.exe` |
| プラグイン | 同 `Tools/*.dll` |
| 中間ファイル | 同じディレクトリ（`.obj` が同居する） |

`Win32` は `x64` が付かず `blueMSX/Make/msvc2022/Release/` になる
（**確認済み(読解)**: `scripts/ci-package.ps1` のパス組み立て）。

`.gitignore` が `x64/` と `[Rr]elease/` を無視するので、出力は追跡されない
（**確認済み**: `git check-ignore -v` で確認）。

## 3. 詰まりどころ

### 3.1 vcpkg のグローバル統合

`%LOCALAPPDATA%\vcpkg\vcpkg.user.props` が `C:\vcpkg` の props を読み込む。
これは**そのマシンの全 MSBuild C++ プロジェクトに無条件で割り込む**。

**このマシンでは実害が出ていない。** `C:\vcpkg\installed` にライブラリが
一つも入っていないので、足されるものが無い（**確認済み**）。

それでも `-p:VcpkgEnabled=false` を付けている。**付けない場合を試していない**ので、
「付けなくても通る」とは書けない。付けても害は無く、後で vcpkg に
ライブラリが入ったときの事故も防げる。

### 3.2 PowerShell の `>` では出力も終了コードも取れない

**`blueMSX+.exe` は Windows サブシステムのアプリなので、PowerShell から
素直に呼ぶと待たずに返る。** `$LASTEXITCODE` は空、リダイレクト先は 0 バイトになる
（**確認済み**: `& $exe /listromtypes > out.txt` を実行し、両方が空だった）。

`Start-Process` に `-Wait -PassThru -RedirectStandardOutput` を渡すと取れる
（**確認済み**: 同じコマンドが終了コード 0 と 97 行の出力を返した）。

```powershell
$p = Start-Process -FilePath "$dir\blueMSX+.exe" -ArgumentList "/listromtypes" `
     -WorkingDirectory $dir -RedirectStandardOutput $out `
     -NoNewWindow -Wait -PassThru
$p.ExitCode
```

### 3.3 初回起動の既定ドライバでは画面が出ないことがある

**症状**: blueMSX+ の枠は出るが、**MSX の画面が描かれず FPS が 0 のまま**になる。
マシン自体は生成されており、ステータスバーには機種名も VRAM 容量も出る。
エラーダイアログは出ない。

**原因**: 出力先に `bluemsx.ini` が無い状態で起動すると、
**`video.driver=d3d12`** が既定になる（**確認済み**: 初回起動後に生成された
`bluemsx.ini` を読んだ）。この環境ではこのドライバで描画が進まない。

**対処**: 生成された `bluemsx.ini` の該当行を書き換えてから起動する。

```sh
# 出力先の bluemsx.ini
video.driver=directx hw
```

**確認済み**: 同じビルド・同じ機種で、この 1 行を変えただけで
MSX の画面（MSX2+ の青い BASIC 画面）が出た。

**このフォークの変更とは無関係である**（**確認済み**: 変更前のコミットの
ソースでビルドし直しても同じ症状が出て、`video.driver` を変えると同じように直った）。
インストール済みの blueMSX+ が動くのは、そちらの `bluemsx.ini` が
`directx hw` を持っているため。

**エミュレータの挙動を確かめる前に、まずこの 1 行を見ること。**
描画されないだけで「動いていない」ように見えるので、
**無関係な変更を疑って時間を失う。** 実際に失った。

## 4. 動かす

**ビルドしただけでは動かない。** `blueMSX/ReleaseFiles/` の中身
（`Machines` / `Databases` / `Themes` / `Keyboard Config` / `Properties` /
`Shortcut Profiles` / `Tools/Cheats`）が実行ファイルの隣に要る
（**確認済み(読解)**: `scripts/ci-package.ps1` が配布物をそう組み立てている）。

初回に一度だけコピーする。約 15MB。出力先は gitignore 済みなので作業ツリーは汚れない。

```sh
DEST=blueMSX/Make/msvc2022/x64/Release
for d in Databases "Keyboard Config" Machines Properties "Shortcut Profiles" Themes; do
    cp -r "blueMSX/ReleaseFiles/$d" "$DEST/"
done
cp -r blueMSX/ReleaseFiles/Tools/Cheats "$DEST/Tools/"
```

`Tools` だけは既にプラグインの DLL が入っているので、`Cheats` を中に足す。
名前は衝突しない（**確認済み**: `ReleaseFiles/Tools` の中身は `Cheats` だけ）。

### 実機の BIOS ROM

リポジトリの `ReleaseFiles/Machines` は**機種定義だけで、実機の BIOS ROM を
含まない**。この状態で使えるのは C-BIOS 系の 24 機種である（**確認済み**:
`/listmachines` が 24 行を返した）。

**ROM の置き場所はマシンごとに違うので、この文書には書かない。**
インストール済みの blueMSX+ があるなら、その `Machines` フォルダの中身を
出力先の `Machines` に重ねればよい。`Shared Roms` も一緒に入る。

```sh
DEST=blueMSX/Make/msvc2022/x64/Release
cp -r "<インストール済み blueMSX+ の Machines>"/* "$DEST/Machines/"
```

**上書きで重ねる。** リポジトリ側にしかない機種定義も残る。

出力先は gitignore 済みなので、**ROM を置いても追跡対象にならない**
（**確認済み**: コピー後に `git status` がクリーンだった）。
`ReleaseFiles/Machines` の側には置かないこと。そちらは追跡される。

**`/listmachines` に出る数は、ROM が揃っている機種の数である。**
`machineIsValid` が `checkRoms` でスロットの ROM ファイルの実在を確かめ、
欠けている機種を落とす（**確認済み(読解)**: `Src/Board/Machine.c:647-690`）。
このマシンでは 253 機種のうち **96 機種**が通った（**確認済み**）。
残りは ROM が揃っていない。

### Y8960 のファームウェア ROM

**Y8960 を駆動する拡張BASIC と BIOS が実在する。**
madscient/MsxSoundSuiteExtension の `y8960bas.rom`、128KB。
Y8960 SCC のマッパーが動けば、これを載せて起動できる
（`../y8960/implementation-plan.md` §8.3）。

**リポジトリに入れてはいけない。** 再配布には MSX ライセンシング
コーポレーションの許諾が要る（`../README.md` の「持ち込んではいけないもの」）。
実機の BIOS ROM と同じく、**gitignore 済みのビルド出力に置く**。

置き場所はマシンごとに違うのでこの文書には書かない。
マッパーを実装したら、カートリッジとして挿す形で使えるはず（**未検証** —
`ROM_Y8960SCC` がまだ無い）。

```sh
blueMSX+.exe /rom1 "<y8960bas.rom のパス>" /romtype1 Y8960SCC
```

`/romtype1` が受けるのは `romTypeToShortString` が返す短縮名である
（**確認済み(読解)**: `Src/Emulator/CommandLine.c` の `romTypeFromShortString`）。
値は `../y8960/implementation-plan.md` §6.1。

### マシン構成は作業ツリーを直接指せる。ただし ROM は付いてこない

`/machinedir` にパスを渡すと、コピーではなく**作業ツリーの
`blueMSX/ReleaseFiles/Machines` を読ませられる**（**確認済み**:
ROM を重ねる前は、コピー側と作業ツリー側で `/listmachines` が同じ 24 機種を返した）。

**ただし作業ツリーには実機 ROM が無い**ので、そちらを指すと C-BIOS 系だけに戻る。
実機機種と作業ツリーの構成を同時に使うことはできない。

| 何を指すか | 使える機種 | テスト構成の置き場所 |
|---|---|---|
| 出力先の `Machines`（既定） | ROM を重ねた分すべて | 出力先。追跡されない |
| `/machinedir` で作業ツリー | C-BIOS 系だけ | 作業ツリー。追跡される |

**Y8960 のテストは C-BIOS 機で足りる**（Y8960 はカートリッジであって、
本体の BIOS に依存しない）ので、どちらでも回せる。

### Y8960 のテスト用マシン構成をどこに置くか

**未決。** 置き場所で性質が変わる。

| 置き場所 | 性質 |
|---|---|
| 出力先の `Machines/` | 追跡されない。使い捨て。ビルド出力を消すと消える |
| `blueMSX/ReleaseFiles/Machines/` | 追跡される。**配布物に入る**（`ci-package.ps1` が `ReleaseFiles` を丸ごと詰める） |

Y8960 が WIP のうちは前者でよい。**配布に載せる段になったら後者へ移す**。
どちらに置いても `/machinedir` で読ませられる。

## 5. ヘッドレスで確かめられること

**blueMSX+ には、ウィンドウを作らずに終わる経路がある。**
`/listspecials` `/listromtypes` `/listmachines` `/listthemes` は、
一覧を標準出力に書いて `exit(0)` する。**エミュレータのウィンドウが
作られる前**である（**確認済み(読解)**: `Src/Win32/Win32.c` の
`commandLinePrintLists` と、それを呼ぶ 4892 行の `exit(0)`）。

実測でもウィンドウは出なかった（**確認済み**: 実行後に
`Get-Process "blueMSX+"` が空。終了コード 0）。

出力は標準出力に出る。リダイレクトされていればそこへ、されていなければ
親コンソールを借りる（**確認済み(読解)**: 同ファイル 4255-4300 行の
`consoleWrite`）。**openMSX と違い、コンソールに出力できる。**

| 手段 | 確かめられること |
|---|---|
| `/listromtypes` | **RomType が登録されたか。**表示名と短縮名もそのまま出る |
| `/listmachines` | **マシン構成が読めるか。** `config.ini` をパースし、スロットの ROM ファイルの実在まで見る（**確認済み(読解)**: `machineIsValid` が `machineCreate` を呼び、`checkRoms` で ROM の実在を確かめる） |
| `/listspecials` | 内蔵カートリッジの一覧 |
| `/listthemes` | テーマが読めるか |

### 確かめられないこと

**デバイスの生成は走らない。** `machineIsValid` は `config.ini` のパースと
ROM の実在までで、`machineInitialize()`（`romMapper*Create` を呼ぶところ）
には届かない。したがって次は**エミュレータを実際に起動しないと分からない**。

- I/O ポートの登録が通ったか（重なって黙って落ちていないか）
- 音が出るか
- レジスタの挙動

**起動するとウィンドウが出る。人間の並行作業と衝突しうるので、
走らせる前に一声かけること。自動で繰り返し起動しない。**

**MSX 側の操作は自動化できない。** `SendKeys` でキーを送っても MSX には
届かない（**確認済み**: BASIC の画面に文字が出ないことを写して見た）。
DirectInput 経由と見られる（**未確認**）。
BASIC からコマンドを打つ必要がある検証は、**人の手を借りるしかない**。

音声は `mixerStartLog()` が WAV に落とせる（**確認済み(読解)**:
`Src/SoundChips/AudioMixer.c:630`、UI からは `Src/Emulator/Actions.c:406`）。
**コマンドラインから開始する手段は見当たらない。**

## 6. 別のマシンで再開するとき

§1 と §3 は**実際に踏んだことの記録**であって、どのマシンでも同じとは限らない。
別のマシンでは次を確かめ直すこと。

1. **v143 が使えるか**（§1）。VS ごとに
   `MSBuild/Microsoft/VC/*/Platforms/x64/PlatformToolsets` を列挙する。
   無ければ `msvc2026`（v145）を使うか、`-p:PlatformToolset` で上書きする
2. **vcpkg のグローバル統合の有無と、インストール済みライブラリ**（§3.1）
3. **`ReleaseFiles` のコピー**（§4）。ビルドしただけでは動かない
4. **実機の BIOS ROM の置き場所**（§4）。マシンごとに違うのでこの文書には
   書いていない。無くても C-BIOS 系 24 機種は動く

`x64/` 配下は全部生成物なので、clone して §2 と §4 を踏めば復元できる。

## 7. 実行経緯

### 2026-09-12（Win10 機の立ち上げ）

- `.github/workflows/ci.yml` がビルド手順の一次情報であることを確認。
  ターゲット・構成・`BlueMsxPackUpx=false` はここから採った
- 環境を調査。VS2022 と VS2026 が併存、v143 が使える、
  vcpkg のグローバル統合はあるがライブラリは空
- **Release / x64 のビルドが通ることを確認（確認済み）**
  - **0 エラー / 7 警告 / 2 分 2 秒**
  - 警告はすべて既存のもの（`GetVersionExA` の C4996 が 4 件ほか）。
    このフォークで足したコードは無いので、**すべて上流由来**である
  - `LNK4286` / `LNK2005` は 0 件。vcpkg による汚染は起きていない
  - 出力: `blueMSX+.exe` (6.3MB) と プラグイン 3 本
- `ReleaseFiles` を出力先にコピー（§4）
- **ヘッドレスで走る経路を発見した（確認済み）**。
  `/listromtypes` が終了コード 0 と 97 行の出力を返し、
  プロセスもウィンドウも残らなかった。
  `/listmachines` は 24 機種を返した（ROM が揃っている C-BIOS 系）
- **`/machinedir` で作業ツリーの `Machines` を直接読ませられることを確認（確認済み）**。
  コピー側と同じ 24 機種が返った
- PowerShell の `>` では GUI サブシステムのアプリの出力も終了コードも取れないことを
  実測（§3.2）。`Start-Process -Wait -PassThru -RedirectStandardOutput` に切り替えた
- **実機の BIOS ROM を出力先に重ねた**（§4）。`/listmachines` が 24 機種から
  **96 機種**に増えた（**確認済み**）。コピー後も `git status` はクリーンで、
  ROM は追跡されていない
- **`msvc2026`（v145）と `Win32` プラットフォーム、`Debug` / `Final` 構成は
  試していない**
