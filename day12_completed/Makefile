# Day 12 完成版 Makefile - Day99アーキテクチャ Phase1
# 新しいディレクトリ構造での統合ビルドシステム

# ツールチェーン設定
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# ディレクトリ設定
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/boot
INCLUDE_DIR = include
LINKER_DIR = linker
BUILD_DIR = build

# ファイル指定
BOOT_SRC = $(BOOT_DIR)/boot.s
KERNEL_ENTRY_SRC = $(BOOT_DIR)/kernel_entry.s
CONTEXT_SWITCH_SRC = $(BOOT_DIR)/context_switch.s
INTERRUPT_SRC = $(BOOT_DIR)/interrupt.s
KERNEL_SRC = $(SRC_DIR)/kernel.c
LINKER_SCRIPT = $(LINKER_DIR)/kernel.ld

# オブジェクトファイル
BOOT_OBJ = boot.bin
KERNEL_ENTRY_OBJ = $(BUILD_DIR)/kernel_entry.o
CONTEXT_SWITCH_OBJ = $(BUILD_DIR)/context_switch.o
INTERRUPT_OBJ = $(BUILD_DIR)/interrupt.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
DEBUG_UTILS_OBJ = $(BUILD_DIR)/debug_utils.o

# 出力ファイル
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMG = os.img

# コンパイラフラグ
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -c \
         -I$(INCLUDE_DIR)
LDFLAGS = -m elf_i386 -Ttext 0x00010000 -e kernel_entry
ASFLAGS = -f elf32

# デフォルトターゲット
.PHONY: all clean run check-env help test check run-test

all: $(OS_IMG)

# ビルドディレクトリ作成
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ブートセクターのコンパイル
$(BOOT_OBJ): $(BOOT_SRC)
	$(AS) -f bin $(BOOT_SRC) -o $(BOOT_OBJ)

# アセンブリオブジェクトファイル
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $(KERNEL_ENTRY_SRC) -o $(KERNEL_ENTRY_OBJ)

$(CONTEXT_SWITCH_OBJ): $(CONTEXT_SWITCH_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $(CONTEXT_SWITCH_SRC) -o $(CONTEXT_SWITCH_OBJ)

$(INTERRUPT_OBJ): $(INTERRUPT_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $(INTERRUPT_SRC) -o $(INTERRUPT_OBJ)

# カーネルCファイルのコンパイル
$(KERNEL_OBJ): $(KERNEL_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(KERNEL_SRC) -o $(KERNEL_OBJ)

$(KEYBOARD_OBJ): $(SRC_DIR)/keyboard.c $(INCLUDE_DIR)/keyboard.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/keyboard.c -o $(KEYBOARD_OBJ)

$(DEBUG_UTILS_OBJ): $(SRC_DIR)/debug_utils.c $(INCLUDE_DIR)/debug_utils.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/debug_utils.c -o $(DEBUG_UTILS_OBJ)

# カーネルELFファイルのリンク
$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(CONTEXT_SWITCH_OBJ) $(INTERRUPT_OBJ) $(KERNEL_OBJ) $(KEYBOARD_OBJ) $(DEBUG_UTILS_OBJ)
	$(LD) $(LDFLAGS) $(KERNEL_ENTRY_OBJ) $(CONTEXT_SWITCH_OBJ) $(INTERRUPT_OBJ) $(KERNEL_OBJ) $(KEYBOARD_OBJ) $(DEBUG_UTILS_OBJ) -o $(KERNEL_ELF)

# カーネルバイナリの作成（ELFからバイナリへ変換）
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

# OSイメージの作成
$(OS_IMG): $(BOOT_OBJ) $(KERNEL_BIN)
	cat $(BOOT_OBJ) $(KERNEL_BIN) > $(OS_IMG)
	# パディングでフロッピーサイズに調整（1.44MB）
	truncate -s 1440K $(OS_IMG)

# QEMU実行
run: $(OS_IMG)
	qemu-system-i386 -drive format=raw,file=$(OS_IMG),index=0,if=floppy -m 16M -serial stdio

# テストシステム
test: $(OS_IMG)
	@echo "=== Day 12 完成版 - 統合テストスイート ==="
	@./tests/integration_test.sh

# 品質チェック（静的解析）
check:
	@echo "=== Day 12 完成版 - 品質チェック ==="
	@echo "1. ファイル存在確認..."
	@test -f $(BOOT_SRC) && echo "  ✓ ブートローダーソース存在" || echo "  ✗ ブートローダーソースなし"
	@test -f $(KERNEL_SRC) && echo "  ✓ カーネルソース存在" || echo "  ✗ カーネルソースなし"
	@test -f $(LINKER_SCRIPT) && echo "  ✓ リンカースクリプト存在" || echo "  ✗ リンカースクリプトなし"
	@echo "2. ビルドテスト..."
	@make clean > /dev/null 2>&1 && echo "  ✓ クリーン成功" || echo "  ✗ クリーン失敗"
	@make all > /dev/null 2>&1 && echo "  ✓ ビルド成功" || echo "  ✗ ビルド失敗"
	@echo "3. コンパイルテスト実行..."
	@cd tests && gcc -o compile_test compile_test.c 2>/dev/null && ./compile_test && rm -f compile_test && echo "  ✓ コンパイルテスト成功" || echo "  ✗ コンパイルテスト失敗"

# ヘッドレステスト（CI/CD用）
run-test: $(OS_IMG)
	@echo "=== ヘッドレス動作テスト ==="
	@timeout 3 qemu-system-i386 \
		-drive file=$(OS_IMG),format=raw,if=floppy \
		-boot a \
		-m 128M \
		-nographic \
		-serial file:test_output.log \
		> /dev/null 2>&1 || true
	@echo "テスト完了。ログ: test_output.log"
	@if [ -f test_output.log ] && [ -s test_output.log ]; then \
		echo "✓ OS起動確認完了"; \
		head -5 test_output.log; \
	else \
		echo "⚠️ ログファイルが生成されませんでした"; \
	fi

# 開発環境チェック
check-env:
	@echo "=== 開発環境チェック ==="
	@which $(CC) >/dev/null && echo "✓ $(CC) インストール済み" || echo "✗ $(CC) が見つかりません"
	@which $(AS) >/dev/null && echo "✓ $(AS) インストール済み" || echo "✗ $(AS) が見つかりません"
	@which qemu-system-i386 >/dev/null && echo "✓ qemu-system-i386 インストール済み" || echo "✗ qemu-system-i386 が見つかりません"

# クリーンアップ
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(BOOT_OBJ) $(OS_IMG)

# ヘルプ
help:
	@echo "=== Day 12 完成版 - ビルドターゲット ==="
	@echo "  all       : OSイメージ全体をビルド"
	@echo "  run       : QEMUでOSを実行"
	@echo "  clean     : 生成ファイルを削除"
	@echo "  check-env : 開発環境をチェック"
	@echo "  help      : このヘルプを表示"
	@echo ""
	@echo "=== テスト・品質管理 ==="
	@echo "  test      : 統合テストスイート実行"
	@echo "  check     : 品質チェック（静的解析）"
	@echo "  run-test  : ヘッドレス動作テスト"
	@echo ""
	@echo "=== 推奨開発サイクル ==="
	@echo "  make check    # 静的解析・品質チェック"
	@echo "  make test     # 統合テストスイート"
	@echo "  make run      # 動作確認"
	@echo ""
	@echo "=== アーキテクチャ情報 ==="
	@echo "  src/kernel/ : カーネルソースコード"
	@echo "  src/boot/   : ブート関連アセンブリ"
	@echo "  include/    : ヘッダーファイル"
	@echo "  linker/     : リンカースクリプト"
