#!/bin/bash

# Day12 完成版 - 統合テストスクリプト
# 【目的】システム全体の動作確認とヘッドレステスト

set -e  # エラーで停止

echo "========================================="
echo "Day12 完成版 - 統合テストスイート"
echo "========================================="
echo ""

# 設定
TEST_DIR="$(dirname "$0")"
PROJECT_DIR="$(cd "$TEST_DIR/.." && pwd)"
OS_IMAGE="$PROJECT_DIR/os.img"
TEST_LOG="$TEST_DIR/integration_test.log"

cd "$PROJECT_DIR"

# テスト結果カウンター
TOTAL_TESTS=0
PASSED_TESTS=0

# テスト結果記録関数
record_test() {
    local test_name="$1"
    local result="$2"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ "$result" = "PASS" ]; then
        echo "✅ $test_name: PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "❌ $test_name: FAIL"
    fi
}

echo "1. ビルドテスト"
echo "-----------------"

# クリーンビルド
if make clean > /dev/null 2>&1; then
    record_test "Clean build preparation" "PASS"
else
    record_test "Clean build preparation" "FAIL"
fi

# コンパイル
if make all > /dev/null 2>&1; then
    record_test "Compilation" "PASS"
else
    record_test "Compilation" "FAIL"
fi

echo ""
echo "2. サイズチェック"
echo "------------------"

# OSイメージのサイズチェック（1.44MB = 1474560 bytes）
if [ -f "$OS_IMAGE" ]; then
    SIZE=$(stat -f%z "$OS_IMAGE" 2>/dev/null || stat -c%s "$OS_IMAGE" 2>/dev/null)
    if [ "$SIZE" -eq 1474560 ]; then
        record_test "OS image size (1.44MB)" "PASS"
    else
        record_test "OS image size (actual: $SIZE bytes)" "FAIL"
    fi
else
    record_test "OS image creation" "FAIL"
fi

echo ""
echo "3. ブートシグネチャチェック"
echo "----------------------------"

# MBRブートシグネチャ確認（0x55AA）
if [ -f "$OS_IMAGE" ]; then
    # ブートセクター（先頭512バイト）の末尾2バイトを確認
    SIGNATURE=$(xxd -s 510 -l 2 -p "$OS_IMAGE" 2>/dev/null)
    if [ "$SIGNATURE" = "55aa" ]; then
        record_test "Boot signature (0x55AA)" "PASS"
    else
        record_test "Boot signature (found: 0x$SIGNATURE)" "FAIL"
    fi
else
    record_test "Boot signature check" "FAIL"
fi

echo ""
echo "4. ヘッドレス動作テスト"
echo "-----------------------"

# 短時間起動テスト（3秒間）
if [ -f "$OS_IMAGE" ]; then
    echo "QEMUで3秒間動作テスト中..."

    # timeout コマンドの存在確認
    if command -v timeout >/dev/null 2>&1; then
        # Linux環境でtimeout使用
        timeout 3 qemu-system-i386 \
            -drive file="$OS_IMAGE",format=raw,if=floppy \
            -boot a \
            -m 128M \
            -nographic \
            -serial file:"$TEST_LOG" \
            > /dev/null 2>&1 || true
    else
        # macOS環境でバックグラウンド実行+kill使用
        qemu-system-i386 \
            -drive file="$OS_IMAGE",format=raw,if=floppy \
            -boot a \
            -m 128M \
            -nographic \
            -serial file:"$TEST_LOG" \
            > /dev/null 2>&1 &

        QEMU_PID=$!

        # 3秒待機
        sleep 3

        # QEMUを終了
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi

    # シリアル出力を確認
    if [ -f "$TEST_LOG" ] && [ -s "$TEST_LOG" ]; then
        record_test "Headless boot test (3 seconds)" "PASS"

        # 基本的なメッセージを確認
        if grep -q "KERNEL:" "$TEST_LOG" 2>/dev/null; then
            record_test "Kernel initialization messages" "PASS"
        else
            record_test "Kernel initialization messages" "FAIL"
        fi

        if grep -q "Thread" "$TEST_LOG" 2>/dev/null; then
            record_test "Thread system activation" "PASS"
        else
            record_test "Thread system activation" "FAIL"
        fi
    else
        record_test "Headless boot test (3 seconds)" "FAIL"
        record_test "Serial output generation" "FAIL"
    fi
else
    record_test "Headless boot test" "FAIL"
fi

echo ""
echo "5. メモリレイアウト確認"
echo "-----------------------"

# カーネルELFファイルの確認
KERNEL_ELF="$PROJECT_DIR/build/kernel.elf"
if [ -f "$KERNEL_ELF" ]; then
    # セクション情報を確認
    if i686-elf-objdump -h "$KERNEL_ELF" > /dev/null 2>&1; then
        record_test "Kernel ELF sections" "PASS"

        # .textセクションの存在確認
        if i686-elf-objdump -h "$KERNEL_ELF" | grep -q "\.text" 2>/dev/null; then
            record_test "Code section (.text)" "PASS"
        else
            record_test "Code section (.text)" "FAIL"
        fi
    else
        record_test "Kernel ELF sections" "FAIL"
    fi
else
    record_test "Kernel ELF file" "FAIL"
fi

echo ""
echo "6. コンパイルテスト実行"
echo "-----------------------"

# コンパイルテストを実行
if [ -f "$TEST_DIR/compile_test.c" ]; then
    if gcc -o "$TEST_DIR/compile_test" "$TEST_DIR/compile_test.c" 2>/dev/null; then
        if "$TEST_DIR/compile_test" > /dev/null 2>&1; then
            record_test "Compile test execution" "PASS"
        else
            record_test "Compile test execution" "FAIL"
        fi
        rm -f "$TEST_DIR/compile_test"
    else
        record_test "Compile test build" "FAIL"
    fi
else
    record_test "Compile test availability" "FAIL"
fi

echo ""
echo "========================================="
echo "統合テスト結果サマリー"
echo "========================================="
echo ""
echo "実行テスト数: $TOTAL_TESTS"
echo "成功テスト数: $PASSED_TESTS"
echo "失敗テスト数: $((TOTAL_TESTS - PASSED_TESTS))"

if [ $TOTAL_TESTS -gt 0 ]; then
    SUCCESS_RATE=$((100 * PASSED_TESTS / TOTAL_TESTS))
    echo "成功率: ${SUCCESS_RATE}%"
else
    echo "成功率: 0%"
fi

echo ""

# 最終判定
if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo "🎉 全ての統合テストが成功しました！"
    echo "Day12完成版は正常に動作しています。"

    # テストログがある場合は要約を表示
    if [ -f "$TEST_LOG" ]; then
        echo ""
        echo "=== 動作ログサンプル ==="
        head -10 "$TEST_LOG" 2>/dev/null || echo "ログファイルが読み取れません"
    fi

    exit 0
else
    echo "⚠️ 一部のテストが失敗しました。"
    echo "システムの実装を確認してください。"
    exit 1
fi