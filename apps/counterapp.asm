; counter_app.asm
; Assemble with: nasm -f bin counter_app.asm -o counter_app.bin
BITS 32
org 0x1000    ; Address where the kernel loads the app

global _start
_start:
    mov ecx, 0       ; Start counter at 0
.loop:
    push ecx         ; Save counter value
    call print_counter
    pop ecx
    inc ecx
    cmp ecx, 10
    jl .loop
.halt:
    jmp .halt        ; Halt

; Prints the current counter (in ECX) as a single digit at the next available screen position.
; For simplicity, this app writes sequentially and does not preserve the previous output.
print_counter:
    ; Convert ECX (0-9) to ASCII ('0'-'9')
    add cl, '0'
    ; Calculate VGA buffer offset: assume 80 columns per line and fixed position for demo.
    ; This example always writes to the first free cell in a static buffer area.
    mov edi, vga_cursor
    mov [edi], cx
    add edi, 2
    mov vga_cursor, edi
    ret

section .data
; Reserve space for VGA cursor pointer (initialized to 0xb8000)
vga_cursor dd 0xb8000
