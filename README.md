# BuraiVibeProject

A NES/Famicom-style vertical shooter inspired by **『無頼ファイター』(Burai Fighter)** and the KID/Namco arcade sound tradition.

SDL2 + C++17 で書かれた、実機ファミコンの制約（CHRバンク、OAM、2A03 APU）を意識した縦スクロールSTG。

---

## 特徴

- **実機準拠の描画制約** — OAM 64エントリ、横1ライン8スプライト制限によるStrictチラつきを再現
- **多部位ボスシステム** — `Core` + `Shield×3` を独立した `Part` として管理し、関節型（FK）と剛体周回型（Orbital）の両トポロジーに対応
- **2A03 APU エミュレーション** — WAVファイルを使わず、矩形波×2 / 三角波 / ノイズをリアルタイム合成予定
- **固定スコアBGM** — 乱数生成を廃止し、A→A′→B→接続の32小節構造を `stage1_score.h` に記述

---

## 動作環境

| 項目 | 要件 |
|---|---|
| OS | Windows 10/11 (MSYS2 ucrt64) |
| コンパイラ | g++ (C++17) |
| ライブラリ | SDL2 / SDL2_image / SDL2_ttf / SDL2_mixer |
| ビルド | CMake 3.16+ |

---

## ビルド方法

### MSYS2 環境の準備

MSYS2 ターミナルで以下を実行:

    pacman -S mingw-w64-ucrt-x86_64-gcc
    pacman -S mingw-w64-ucrt-x86_64-cmake
    pacman -S mingw-w64-ucrt-x86_64-SDL2
    pacman -S mingw-w64-ucrt-x86_64-SDL2_image
    pacman -S mingw-w64-ucrt-x86_64-SDL2_ttf
    pacman -S mingw-w64-ucrt-x86_64-SDL2_mixer

### ビルド

PowerShell で:

    $env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
    cmake -B build
    cmake --build build --parallel 4

### 実行

    .\build\BuraiVibeGame.exe

`assets/` は CMake の POST_BUILD で `build/assets/` に自動コピーされます。

---

## 操作方法

| キー | 動作 |
|---|---|
| 矢印 / WASD | 移動 |
| Z / Space | 射撃 |
| 1〜0 | BGMトラック切替 |
| G | BGM再生成 |
| F5 | アセットリロード（開発用） |

（実装状況により変動）

---

## ディレクトリ構成

    BuraiVibeProject/
    ├─ assets/                     リソース（ビルド時に build/assets へコピー）
    │   ├─ sprites/                スプライトPNG + メタJSON
    │   │   ├─ player/             自機8方向
    │   │   ├─ effects/            爆発4フレーム
    │   │   └─ ...
    │   ├─ raw_enemies/            敵素材の元画像
    │   ├─ tilesets/               背景タイル
    │   ├─ music/                  （予約枠）
    │   └─ sounds/                 （予約枠）
    │
    ├─ src/
    │   ├─ main.cpp                エントリポイント
    │   ├─ stage.cpp/.h            ステージ進行
    │   ├─ enemy.cpp/.h            敵基底
    │   ├─ orbital_shield_enemy.*  3玉周回敵（Core 1 + Shield 3）
    │   ├─ multi_part_entity.*     部位集約の基底
    │   ├─ boss_entity.*           フェーズ管理付きボス基底
    │   ├─ rigid_multi_part_entity.*  剛体周回型の中間層
    │   ├─ oam_sdl_bridge.*        OAM → SDL描画の唯一の窓口
    │   │
    │   └─ boss/                   部位システム（敵種を問わず再利用）
    │       ├─ part.h              部位の抽象基底
    │       ├─ single_part.h       1スプライト部位
    │       ├─ composite_part.h    複数スプライト部位
    │       ├─ chain_node.h        FKチェーン参加インターフェース
    │       ├─ joint.h             球体関節
    │       ├─ core.h              核（弱点）
    │       ├─ tip.h               鎌爪（8方向スワップ）
    │       ├─ emitter.h           弾発射点（描画なし）
    │       ├─ render_unit.h       描画の最小単位
    │       ├─ dir_sprite.h        8方向→スプライト解決
    │       ├─ oam.h               OAM（Strictチラつき）
    │       └─ small_vector.h      固定長/可変両対応コンテナ
    │
    ├─ tests/                      単体テスト
    ├─ tools/                      Python製アセット生成
    └─ docs/                       設計メモ

---

## 設計方針

### 部位システムのトポロジー

ボスは **「集約軸」** と **「チェーン軸」** の2軸で管理されます。

- **集約軸** — `MultiPartEntity::parts[]`（所有・破壊判定）
- **チェーン軸** — `ChainNode::prev/next`（FK計算）

この分離により、部位破壊時の整合性が保たれます。

### 2種類の敵トポロジー

| 種類 | 座標計算 | 例 |
|---|---|---|
| 剛体周回型 (Rigid) | 親中心 + パラメトリック軌道 | `OrbitalShieldEnemy`（3玉） |
| 関節連結型 (Articulated) | FK（角度伝播） | プロミネンス型ボス（予定） |

### 音源方針

**WAVファイルは使用しません。**  
SDL2 のオーディオコールバック内で 2A03 APU（Pulse×2 / Triangle / Noise）を毎サンプル合成します。  
これにより実機の「金属的エコー」「ピッチスベンドKick」「LFSRノイズ」を再現します。

---

## 開発ロードマップ

- [x] ボス部位システム（`boss/` 階層）
- [x] 3玉周回敵（`OrbitalShieldEnemy`）
- [x] OAM → SDL 描画ブリッジ
- [x] 爆発スプライト素材（4フレーム）
- [ ] 2A03 APU リアルタイム合成
- [ ] 関節連結型ボス（プロミネンス）
- [ ] Stage 1 固定スコアBGM統合
- [ ] Strict チラつきの scanline 走査化

---

## ライセンス

未定（個人開発）

---

## 参考

- 『無頼ファイター』（KID / タクミ、1990）
