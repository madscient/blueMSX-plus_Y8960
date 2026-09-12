# Y8960 エミュレーション実装 — 文書の役割

Y8960 カートリッジのエミュレーションを blueMSX+ に追加する作業のための文書。
Y8960 対応は upstream には存在しない、このフォーク固有の機能である。

作業時の規則とフォーク全体の事実関係は `doc/fork/README.md` にある
（ルートの `CLAUDE.md` は追跡されないローカルファイルなので、
残す規則はそちらに置かない）。

## 文書一覧

| 文書 | 役割 |
|---|---|
| `README.md` (本書) | 文書の役割の記録・索引 |
| `hardware-notes.md` | Y8960 ハードウェア仕様の調査結果。**madscient/openMSX_Y8960 からの写し**（情報リソース。blueMSX+ の成果物ではない） |
| `implementation-plan.md` | blueMSX+ 側の実装計画・決定事項・実行経緯 |
| `tests/MSX2+ - C-BIOS + Y8960/` | 7 ブロックを置いたテスト用マシン構成 |

`implementation-plan.md` が**作業計画と経緯を記録する文書**である。
セッションをまたぐ引き継ぎ情報・見送った判断・訂正はすべてここに書く。

## 状態（2026-09-12）

**Phase 0 が完了した。** 7 ブロックが `RomType` を持ち、
マシン構成に置いて起動できる（C-BIOS 0.29+ の起動を確認）。
**中身はどれも空**で、I/O ポートも取らず音も出ない。

前提として**上流の I/O ポートを重ね合わせ対応に直した**
（`implementation-plan.md` §4.2）。書き込みが複数デバイスへ配られることは
実測で確かめてある。

**次は Phase 1**（SCC とマッパー、MSX-TIMER）。残る未決は同 §9。

**ハードウェア仕様は `hardware-notes.md` だけを見ないこと。**
写しより新しい情報が `implementation-plan.md` §3.3 にある。Y8960 を駆動する
ファームウェア（MsxSoundSuiteExtension）が前提にしている仕様で、
写しが未決としていた論点のいくつかはそこで決着している。

## テストの回し方

**テスト用マシン構成がある。** `tests/MSX2+ - C-BIOS + Y8960/` を
ビルド出力の `Machines/` にコピーすると、7 ブロックを置いた構成で起動できる。
C-BIOS MSX2+ に 7 行足したもので、本体の ROM は C-BIOS のものを相対パスで参照する。

**Y8960 SCC には `y8960bas.rom` が要る。** マッパーが ROM を伴うため、
無いとこの構成は `/listmachines` に出ない。
ROM は madscient/MsxSoundSuiteExtension から各自で用意し、**コピー先**の
構成フォルダに置く。**リポジトリには置かないこと**（再配布に許諾が要る。
`doc/fork/README.md` の「持ち込んではいけないもの」）。

```sh
DEST=blueMSX/Make/msvc2022/x64/Release/Machines
cp -r "doc/fork/y8960/tests/MSX2+ - C-BIOS + Y8960" "$DEST/"
cp "<y8960bas.rom のパス>" "$DEST/MSX2+ - C-BIOS + Y8960/"
```

### バンクマッパーのテスト

`tests/banktest.asm` が**バンクメモリの回帰テスト**である。ROM カートリッジとして
Y8960 SCC に載せ、MSX 側からバンクレジスタを叩いて結果を画面に出す。
C-BIOS でも動く（ROM の init から走るため、スロット切り替えが要らない）。

```sh
py "doc/fork/y8960/tests/make-banktest.py" <pasmo.exe> <出力先>/banktest.rom
```

出来た ROM を `tests/MSX2+ - C-BIOS + Y8960 (banktest)/` の構成に置いて起動する。
画面に 3 行出る。**どれか 1 つでも `NG` なら回帰している。**

```
1 ROM BANKS: OK
2 RAM WRITE: OK
3 ROM PROTECT: OK
```

**判別力は確かめてある**（`implementation-plan.md` §5.7）。

### 本体のファームウェアを載せる構成

**この構成で確かめられるのは「起動してクラッシュしないこと」までである。**
C-BIOS は BASIC を持たないので、`y8960bas.rom` の拡張 BASIC を呼べない。
バンクが正しくマップされているかは、**実機 BIOS を持つ機種に載せて
拡張 BASIC を呼ばないと分からない**。

（起動時に C-BIOS が `Init ROM in slot: 1` を 2 回出すが、
**これを判定に使わないこと。** 標準の MSX BIOS は ROM ヘッダを
`4000H` と `8000H` でしか探さず、この表示が何を意味するかは未解明である。）

**自動で判定できるのはここまで**（`implementation-plan.md` §8）。

ただし**検証の経路は分かっている**（`implementation-plan.md` §8）。
`/listromtypes` と `/listmachines` はウィンドウを出さずに終わるので、
RomType の登録とマシン構成の妥当性はヘッドレスで判定できる。
デバイスの生成と音は起動しないと確かめられない。

手順は `doc/fork/build/README.md` §5。実装が入ったらここに具体化する。

**非回帰テストの注意**: 修正後に通ることは、修正前に落ちることを示すまで
証拠にならない。テストを書いたら、まず期待値を壊して落ちるのを確認すること。

## 取り込み元

Y8960 の実装方針は madscient/openMSX_Y8960 から引いている。
一次仕様である hra1129/Y8960_Cartridge からは**何も取り込んでいない**。
`hardware-notes.md` は RTL と仕様書を読解した結果の記述であって、複製ではない。
理由は `doc/fork/README.md` の「ライセンスと帰属」。
