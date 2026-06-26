; test_entry.s - Test binary entry point
; Modified from kernel_entry.s to call test functions instead of kernel_main

[bits   32]
	[global _start]; Linker entry point
	[extern test_main]; Test main function from test_pic.c
	[extern stack_top]; Stack defined in linker script

	section .text

_start:
	;   Debug: Test entry point reached
	mov dword [0xb801c], 0x0748; 'H' - Test entry reached

	;   Protected mode segment setup confirmation
	mov ax, 0x10; Data segment selector
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	;   Debug: Segment setup complete
	mov dword [0xb8020], 0x0749; 'I' - Segment setup complete

	;   Stack setup
	mov esp, 0x00300000
	mov ebp, 0

	;   Debug: Stack setup complete
	mov dword [0xb8024], 0x074a; 'J' - Stack setup complete

	;   Debug: BSS clear start
	mov dword [0xb8028], 0x074b; 'K' - BSS clear start

	;      Clear BSS section (important!)
	extern __bss_start
	extern __bss_end
	mov    edi, __bss_start
	mov    ecx, __bss_end
	sub    ecx, edi
	xor    eax, eax
	cld
	rep    stosb

	;   Debug: BSS clear complete
	mov dword [0xb802c], 0x074c; 'L' - BSS clear complete

	;   Debug: test_main call before
	mov dword [0xb8030], 0x074d; 'M' - test_main call before

	;    Call test main function
	call test_main

	;   Debug: returned from test_main (normally shouldn't reach here)
	mov dword [0xb8034], 0x074e; 'N' - returned from test_main

	; Halt if tests complete (normally shouldn't reach here)

.halt:
	hlt
	jmp .halt
