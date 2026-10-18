# このフォークで作業するときの規則

本リポジトリは [blueMSX+](https://github.com/Hesoten/blueMSX-plus) の
unofficial fork である。blueMSX+ 自身が blueMSX の非公式フォークなので、
上流は 2 段になっている。

**本書は索引である。規則の中身はここに書かない。**

| | |
|---|---|
| **作業の規則（AI 向け）** | **`doc/fork/ai/README.md`** |
| フォークの事実関係・ライセンス・実装状況 | `doc/fork/README.md` |
| **枝の役割（枝に何かを入れる前に見る）** | `doc/fork/README.md` の「枝の役割」 |
| ビルドと起動の手順 | `doc/fork/build/README.md` |
| Y8960 の文書 | `doc/fork/y8960/README.md` |
| Y8960 の実装計画・決定・経緯 | `doc/fork/y8960/implementation-plan.md` |
| Y8960 のハードウェア仕様 | `doc/fork/y8960/hardware-notes.md`（写し。§3.3 に新しい情報あり） |
| キー入力の注入・ウィンドウを出さない実行の計画 | `doc/fork/automation/plan.md` |
| ディスクイメージ書き出しの計画・経緯 | `doc/fork/disk-export/plan.md` |
| Makoto（OPNA）対応の計画・経緯 | `doc/fork/opna/plan.md` |
| サウンドミキサーの作り直しの計画・経緯 | `doc/fork/mixer/plan.md` |
| リリースの手順と記録 | `doc/fork/README.md` の「リリース」 |

**セッションをまたいだら `implementation-plan.md` §0 を最初に読む。**
現在地・確かめずに残してあること・次の一手がそこにある。
着手の前に決めるべきことは同 §9。

## gh を使う前に既定のリポジトリを確かめる

**`gh repo set-default --view` で見る。** この作業ツリーには `origin`（フォーク）と
`upstream`（上流）の 2 つの remote があり、既定が未設定だと `gh` は上流を選ぶ。
設定はマシンごとの `.git/config` にあり、別のマシンでは違う。フォークに向けるなら
`-R madscient/blueMSX-plus_Y8960` を付ける。

## 上流に出すものへ混ぜない

**プルリクエストや issue に、AI 向け文書を混ぜないこと。**
上流は `.gitignore` で `CLAUDE.md` を除外しており、この種の文書を
リポジトリに置かない方針である。手順は `doc/fork/ai/README.md`。
