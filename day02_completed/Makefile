# Makefile for Day 02 - Protected Mode Switch

AS   = nasm
QEMU = qemu-system-i386

BOOT_SRC = boot.s
OS_IMG   = os.img

all: $(OS_IMG)
	@echo "✅ Day 02: プロテクトモードブートイメージ作成完了"
	@echo "🚀 実行: make run"

$(OS_IMG): $(BOOT_SRC)
	@echo "🔨 アセンブルしています..."
	$(AS) -f bin $(BOOT_SRC) -o $(OS_IMG)
	@echo "📏 ファイルサイズ確認..."
	@if [ `wc -c < $(OS_IMG)` -eq 512 ]; then \
	  echo "✅ 512バイトOK"; \
	else \
	  echo "❌ エラー: 512バイトではありません"; exit 1; \
	fi

run: $(OS_IMG)
	@echo "🚀 QEMUで起動しています..."
	$(QEMU) -fda $(OS_IMG) -boot a

debug: $(OS_IMG)
	@echo "🔍 デバッグモードで起動..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio -monitor tcp:127.0.0.1:4444,server,nowait

clean:
	@echo "🧹 クリーンアップ..."
	rm -f $(OS_IMG)
	@echo "✅ 完了"

.PHONY: all run debug clean
