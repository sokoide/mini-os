# リポジトリガイドライン

本ガイドは、この教育用 x86-32 ミニ OS への貢献者が効率よく作業するための指針をまとめます。開発とテストの起点は **`day12_completed/`**（12 日カリキュラムの最終完成形）です。

## プロジェクト構造とモジュール構成（`day12_completed/` 起点）

- `include/` C ヘッダ（`kernel.h`, `keyboard.h`, `debug_utils.h`, `error_types.h`）
- `src/` 実装
  - `kernel.c`（割込み・スケジューラ・VGA/シリアル出力）
  - `keyboard.c`（PS/2 入力・SPSC リングバッファ・`read_line`）
  - `debug_utils.c`（ロギング・メトリクス・プロファイリング）
  - `boot/`（`boot.s`, `kernel_entry.s`, `interrupt.s`, `boot_constants.inc`）
- `tests/` QEMU 駆動の機能テストとハーネス（`test_framework.*` + `test_*.c`）
- `linker/` `kernel.ld`
- `../docs/` アーキテクチャ正本・品質資料

> 各ステップの完成版は `day01_completed/` 〜 `day11_completed/`、演習ガイドは `day01/` 〜 `day12/`（README のみ）。

## ビルド・テスト・開発コマンド

`day12_completed/` で実行:

- `make all` → `os.img` をビルド
- `make run` / `make run-nogui` / `make run-noserial` → QEMU で実行
- `make analyze` → cppcheck + clang 静的解析
- `make quality` → 追加チェック（マジックナンバー・長すぎる関数）
- `make test` → 全テストイメージをヘッドレス QEMU で実行
- `make test-pic|test-thread|test-interrupt|test-sleep` → 個別テスト
- `make clean` / `make test-clean` → ビルド/テスト成果物を削除
- `make check-env` → ツールチェーン確認（i686-elf-gcc, nasm, qemu）

## コーディングスタイルと命名規約

- 言語: C（gnu99）、NASM（elf32/bin）
- インデント: スペース 4 文字、タブなし
- 命名: 関数/変数は `snake_case`、マクロ/定数は `UPPER_SNAKE_CASE`
- ヘッダは API を公開し、実装は `src/` に置く
- 定数（PIC/PIT/IDT/VGA）はヘッダに集約し、マジックナンバーを避ける

## テストのガイドライン

- テストは `tests/` 配下。各領域はドライバとカーネル側テストのペア
- ローカル実行: `make test` または個別 `make test-...`
- スケジューラ/割込み/キーボードの変更にはテストを追加。ヘッドレス QEMU を優先
- テストは決定論的に。アサーションにはシリアルログを活用

## コミット・プルリクエストのガイドライン

- コミット: 命令形の件名（72 文字以内）、本文で理由と影響を記述
- 関連 Issue を参照（例: `Fixes #123`）し、影響モジュールを明記
- PR には必ず含める:
  - 変更内容と理由、実行結果（短いログ）やスクリーンショット
  - テスト計画（コマンド）と結果、追加/更新したテスト
  - 挙動や API が変わる場合はドキュメントを更新（下記参照）

## ドキュメントの要件

- 挙動（スケジューラポリシー、入力 API、メモリレイアウト等）を変更した場合は `../docs/ARCHITECTURE_ja.md`（正本）と品質資料（`../docs/reference/`）を更新
- 既存の図表（A20、システム構成図など）は維持し、差分のみ追加する
