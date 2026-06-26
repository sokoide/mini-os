// Day 03 完成版 - kernel.c（freestanding C の動作確認）
//
// この日（Day 03）の目的は、アセンブリで書いたブート処理から初めて C 言語の
// 関数 kmain() に制御を移し、「C コードが実行できること」を確認することです。
// まだ VGA ドライバ（Day 04）もシリアル出力（Day 05）もありません。
// そのため、VRAM（0xB8000）の先頭に直接文字を書き込んで、C コードが動いている
// ことを視覚的に確認します。
//
// 本格的な VGA ドライバ（vga_putc / vga_puts / 色 / スクロール）は Day 04 で導入します。

#include <stdint.h>

// VGA テキストモードのフレームバッファ先頭アドレス
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;

// 文字(下位 8 bit) + 属性(上位 8 bit) を 1 ワードに合成する。
// 属性 0x0F = 白文字 / 黒背景。
static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void kmain(void) {
    // VRAM 先頭に "C!" を直接書き込む。
    // 画面に表示されれば、C のコードが実行されている証拠。
    VGA_MEM[0] = vga_entry('C', 0x0F);
    VGA_MEM[1] = vga_entry('!', 0x0F);

    // 以降は停止（割り込みはまだ未導入のため hlt で待機）
    while (1) {
        __asm__ volatile("hlt");
    }
}
