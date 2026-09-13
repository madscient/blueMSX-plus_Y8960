# Y8960 エミュレーション実装 — 文書の役割

Y8960 カートリッジのエミュレーションを blueMSX+ に追加する作業のための文書。
Y8960 対応は upstream には存在しない、このフォーク固有の機能である。

作業時の規則は `doc/fork/ai/README.md`、フォーク全体の事実関係は
`doc/fork/README.md` にある。**ルートの `CLAUDE.md` は索引だけ**で、
規則の中身は置かない（追跡されてはいる。`.gitignore` が上流の除外を
打ち消している）。

## 文書一覧

| 文書 | 読者 | 役割 |
|---|---|---|
| `README.md` (本書) | 開発者・AI | 文書の役割の記録・索引 |
| `user-guide.md` | **エンドユーザー** | Y8960 対応の使い方。**経緯・内部の識別子・開発文書への参照を書かない** |
| `hardware-notes.md` | 開発者・AI | Y8960 ハードウェア仕様の調査結果。**madscient/openMSX_Y8960 からの写し**（情報リソース。blueMSX+ の成果物ではない） |
| `implementation-plan.md` | 開発者・AI | blueMSX+ 側の実装計画・決定事項・実行経緯 |

`implementation-plan.md` が**作業計画と経緯を記録する文書**である。
セッションをまたぐ引き継ぎ情報・見送った判断・訂正はすべてここに書く。
**引き継ぎの入口は同 §0。**

**`user-guide.md` を直すときは、書く主張を先にコードか実行で確かめる。**
エンドユーザー向けの文書には確度の印を付けられないので、
確かめられないものは本文に書かず、同文書の「動作の確認が済んでいない機能」に置く。

### テスト

| ファイル | 役割 |
|---|---|
| `tests/banktest.asm` + `make-banktest.py` | MSX 側で走る回帰テスト。バンクマッパー・窓・ミラー・タイマ・DCSG・OPLLEX・OPL2EX・SSGS。128KB に展開 |
| `tests/ssgstest.asm` + `make-ssgstest.py` | 基盤タイプ `MSX2++` で SSGS が本体の PSG として働くか、窓とイネーブラが無いか。16KB の素の ROM |
| `tests/opllex-bank-probe.c` | OPLLEX のコア単体試験（ヘッドレス） |
| `tests/opl2ex-probe.cpp` + `opl2ex-host-stub.c` | OPL2EX のコア単体試験（ヘッドレス） |
| `tests/ssgs-probe.c` + `ssgs-host-stub.c` | SSGS のコア単体試験（ヘッドレス） |
| `tests/MSX2+ - C-BIOS + Y8960/` | 7 ブロックを置いた構成。**`y8960bas.rom` が要る** |
| `tests/MSX2+ - C-BIOS + Y8960 (banktest)/` | 同じ構成で、Y8960 SCC に `banktest.rom` を載せたもの。本体 MSX-MUSIC は外してある |
| `tests/MSX2+ - C-BIOS + Y8960 (cartridge)/` | Y8960 SCC を**外した**構成。`/rom1 <rom> /romtype1 Y8960SCC` でカートリッジとして挿す |
| `tests/MSX2++ - C-BIOS + Y8960/` | 基盤タイプ `MSX2++`。MSX-TIMER と Y8960 SCC を置き、Y8960 SCC に `ssgstest.rom` を載せたもの |

コア単体試験のビルド手順は各ファイルの冒頭にある。

## 状態

**7 ブロックすべてに中身が入り、MSX 側から確かめられるものは確かめ終えた。**
MSX-MIXER だけは実機側が未実装で、入口（値を憶える）までで止まっている。

**現在地・次の一手・確かめずに残してあることは `implementation-plan.md` §0。**
セッションをまたぐときはそこを最初に読む。残る未決は同 §9。

**カートリッジ内の音源ブロックは等価回路であって、本体のチップではない。**
だから本体のコアを共有せず、フォークとして実装している（同 §11 の 2026-09-12 (10)）。

**ハードウェア仕様は `hardware-notes.md` だけを見ないこと。**
写しより新しい情報が `implementation-plan.md` §3.3 にある。

## テストの回し方

構成フォルダをビルド出力の `Machines/` にコピーし、ROM を置いて起動する。

```sh
DEST=blueMSX/Make/msvc2022/x64/Release/Machines
cp -r "doc/fork/y8960/tests/MSX2+ - C-BIOS + Y8960 (banktest)" "$DEST/"
py "doc/fork/y8960/tests/make-banktest.py" <pasmo.exe> "$DEST/MSX2+ - C-BIOS + Y8960 (banktest)/banktest.rom"
```

`banktest` の画面に出る行のうち、**`OK` / `NG` のものは機械判定**、
`(LISTEN)` のものは**人が聴いて判定する**。期待する聞こえ方は各行に書いてある。

```
1 ROM BANKS: OK        5 TIMER ENABLER: OK     9 OPLL (LISTEN)       13 WINDOW, BANK 20: OK
2 RAM WRITE: OK        6 TIMER COUNTS: OK     10 OPL2 READ BACK: OK  14 PAGE 0 NO WRITE: OK
3 ROM PROTECT: OK      7 TIMER FLAG: OK       11 OPL2 (LISTEN)       15 MIRROR AT 0000H: OK
4 SCC WINDOW: OK       8 DCSG SWEPT           12 SSGS (LISTEN)
```

`ssgstest` は `MSX2++` の構成に `ssgstest.rom` を置いて起動する。
1 / 2 / 6 / 7 / 8 が機械判定、3 / 4 が聴く項目、5 がかな LED を見る項目。

```sh
cp -r "doc/fork/y8960/tests/MSX2++ - C-BIOS + Y8960" "$DEST/"
py "doc/fork/y8960/tests/make-ssgstest.py" <pasmo.exe> "$DEST/MSX2++ - C-BIOS + Y8960/ssgstest.rom"
```

**起動するとウィンドウが出る。** 画面の読み取り方と注意は
`doc/fork/build/README.md` §5.1。**画面は `PrintWindow` で撮る**
（画面から写すと、手前に来た別のウィンドウの中身が写る）。

**`y8960bas.rom` はリポジトリに置かない**（再配布に許諾が要る。
`doc/fork/README.md` の「持ち込んではいけないもの」）。

**BANK2 と BANK3 は試せない。** カートリッジの init はページ1 だけが自分の
スロットに切り替わった状態で走るので、ページ2 に向けた書き込みはマッパーに
届かない（`ENASLT` が要る）。

**非回帰テストの注意**: 修正後に通ることは、修正前に落ちることを示すまで
証拠にならない。規則は `doc/fork/ai/README.md` の「判別力は、壊した版に掛けて示す」。

## 取り込み元

Y8960 の実装方針は madscient/openMSX_Y8960 から引いている。
一次仕様である hra1129/Y8960_Cartridge からは**何も取り込んでいない**。
`hardware-notes.md` は RTL と仕様書を読解した結果の記述であって、複製ではない。
理由は `doc/fork/README.md` の「ライセンスと帰属」。

OPLLEX の音色データは "Copyright free OPLL(x) ROM patches"（CC BY-SA）。
帰属表示は `Src/SoundChips/Y8960OpllCore.c` と `user-guide.md` にある。
