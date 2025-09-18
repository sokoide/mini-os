// io.h — フリースタンディング環境向けの最小限のポートI/Oヘルパ
#pragma once
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1,%0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" ::"a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    // 伝統的に 0x80 ポートへの書き込みで短い待ち時間を作る手法
    __asm__ volatile("outb %%al,$0x80" ::"a"(0));
}
