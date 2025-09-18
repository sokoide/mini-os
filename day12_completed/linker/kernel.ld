/* Day 12 完成版 - リンカスクリプト */
/* 機能: カーネルメモリレイアウト定義・BSS初期化サポート */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(kernel_entry)

SECTIONS
{
    /* カーネルは1MBから開始 */
    . = 0x00010000;
    
    /* テキストセクション（実行可能コード） */
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    }
    
    /* 読み取り専用データセクション */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* 初期化済みデータセクション */
    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    }
    
    /* 未初期化データセクション（BSS） */
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /* カーネル終端マーカー */
    __kernel_end = .;
    
    /* デバッグセクション（実行時には不要） */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}