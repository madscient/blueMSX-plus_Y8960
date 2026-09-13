# このフォークで作業するときの規則

本リポジトリは [blueMSX+](https://github.com/Hesoten/blueMSX-plus) の
unofficial fork である。blueMSX+ 自身が blueMSX の非公式フォークなので、
上流は 2 段になっている。

**本書は索引である。規則の中身はここに書かない。**

| | |
|---|---|
| **作業の規則（AI 向け）** | **`doc/fork/ai/README.md`** |
| フォークの事実関係・ライセンス・実装状況 | `doc/fork/README.md` |
| ビルドと起動の手順 | `doc/fork/build/README.md` |
| Y8960 の文書 | `doc/fork/y8960/README.md` |
| Y8960 の実装計画・決定・経緯 | `doc/fork/y8960/implementation-plan.md` |
| Y8960 のハードウェア仕様 | `doc/fork/y8960/hardware-notes.md`（写し。§3.3 に新しい情報あり） |
| キー入力の注入・ウィンドウを出さない実行の計画 | `doc/fork/automation/plan.md` |

**セッションをまたいだら `implementation-plan.md` §0 を最初に読む。**
現在地・確かめずに残してあること・次の一手がそこにある。
着手の前に決めるべきことは同 §9。

## 上流に出すものへ混ぜない

**プルリクエストや issue に、AI 向け文書を混ぜないこと。**
上流は `.gitignore` で `CLAUDE.md` を除外しており、この種の文書を
リポジトリに置かない方針である。手順は `doc/fork/ai/README.md`。
