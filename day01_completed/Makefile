# Makefile for Day 01 - Basic Boot
# シンプルなブートローダー用ビルドスクリプト

# === ツール設定 ===
AS = nasm              # アセンブラ
QEMU = qemu-system-i386 # エミュレータ

# === ファイル名設定 ===
BOOT_SRC = boot.s      # ソースファイル
OS_IMG = os.img        # 起動イメージ

# === メインターゲット ===
# 'make' または 'make all' で実行される
all: $(OS_IMG)
	@echo "✅ ブートローダーの作成が完了しました！"
	@echo "📁 $(OS_IMG) が作成されました"
	@echo ""
	@echo "🚀 実行するには: make run"
	@echo "🧹 クリーンアップ: make clean"

# === イメージファイル作成 ===
$(OS_IMG): $(BOOT_SRC)
	@echo "🔨 ブートローダーをアセンブルしています..."
	$(AS) -f bin $(BOOT_SRC) -o $(OS_IMG)
	@echo "📏 ファイルサイズ確認中..."
	@ls -l $(OS_IMG) | awk '{print "   サイズ: " $$5 " バイト"}'
	@if [ `wc -c < $(OS_IMG)` -eq 512 ]; then \
		echo "✅ 正しく512バイトになりました！"; \
	else \
		echo "❌ エラー：512バイトではありません"; \
		exit 1; \
	fi

# === QEMU実行 ===
run: $(OS_IMG)
	@echo "🚀 QEMUでOSを起動しています..."
	@echo "💡 終了するには QEMUウィンドウを閉じるか Ctrl+C を押してください"
	@echo ""
	$(QEMU) -fda $(OS_IMG) -boot a


# === デバッグ実行 ===
# シリアル出力付きで実行（デバッグ時に便利）
debug: $(OS_IMG)
	@echo "🔍 デバッグモードで起動しています..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio -monitor tcp:127.0.0.1:4444,server,nowait

# === クリーンアップ ===
clean:
	@echo "🧹 生成されたファイルを削除しています..."
	rm -f $(OS_IMG)
	@echo "✅ クリーンアップ完了"

# === ヘルプ ===
help:
	@echo "=== Day 01 ブートローダー開発 ==="
	@echo ""
	@echo "利用可能なコマンド:"
	@echo "  make all     - ブートローダーをビルド"
	@echo "  make run     - QEMUで実行"
	@echo "  make debug   - デバッグモードで実行"
	@echo "  make clean   - 生成ファイルを削除"
	@echo "  make help    - このヘルプを表示"
	@echo ""
	@echo "📚 学習ポイント:"
	@echo "  - 512バイトのブートセクター"
	@echo "  - BIOS割り込みによる文字表示"
	@echo "  - x86リアルモードの基礎"

# === Phonyターゲット ===
# ファイル名と重複しないよう宣言
.PHONY: all run debug clean help