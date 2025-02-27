; hello_app.asm
; Assemble with: nasm -f bin hello_app.asm -o hello_app.bin
BITS 32
org 0x1000    ; This is the address where the kernel loads the app

global _start
_start:
    mov esi, hello_msg   ; pointer to the message
    call print_string
.halt:
    jmp .halt            ; Infinite loop (halt)

; Prints a null-terminated string to VGA text mode (0xb8000) using attribute 0x07.
print_string:
    mov edi, 0xb8000     ; VGA text buffer address
.next_char:
    lodsb                ; Load byte from [esi] into AL and increment ESI
    cmp al, 0
    je .done
    mov ah, 0x07         ; Light grey on black
    mov [edi], ax
    add edi, 2           ; Advance to next character cell
    jmp .next_char
.done:
    ret

section .data
hello_msg db "Hello, App! Running as an external program.", 0
