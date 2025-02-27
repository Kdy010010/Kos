; bootloader.asm
; 이 부트로더는 512바이트 부트섹터로 컴파일되어야 하며,
; NASM을 사용해 다음과 같이 빌드할 수 있습니다:
;   nasm -f bin bootloader.asm -o bootloader.bin

[org 0x7C00]           ; BIOS가 부트섹터를 0x7C00 주소에 로드

start:
    cli                ; 인터럽트 비활성화
    xor ax, ax
    mov ds, ax         ; DS, ES, SS를 0으로 초기화
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00     ; 스택 포인터 설정

    ; 부트메시지 출력
    mov si, boot_msg
    call print_string

    ; 부트 드라이브 번호를 BIOS에서 받아옴 (dl 레지스터에 있음)
    mov [boot_drive], dl

    ; 디스크로부터 커널(여기서는 2번째 섹터 1개)을 메모리 0x0000:0x8000에 로드
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x8000     ; 커널 로드 주소
    mov ah, 0x02       ; INT 13h, 함수 02h: 디스크 읽기
    mov al, 1          ; 읽을 섹터 수 (1개)
    mov ch, 0          ; 실린더 0
    mov cl, 2          ; 섹터 번호 2 (부트섹터 이후)
    mov dh, 0          ; 헤드 0
    mov dl, [boot_drive] ; 부트 드라이브 번호 사용
    int 0x13
    jc disk_error    ; 에러 발생 시

    ; 커널 실행 (0x0000:0x8000에 로드된 커널로 점프)
    jmp 0x0000:0x8000

disk_error:
    mov si, err_msg
    call print_string
    jmp $            ; 무한 루프

; 문자열 출력 루틴 (BIOS teletype 모드 사용)
print_string:
    mov ah, 0x0E
.print_loop:
    lodsb            ; DS:SI의 바이트를 AL로 로드하고 SI 증가
    cmp al, 0
    je .done
    int 0x10         ; BIOS 영상 서비스: 문자 출력
    jmp .print_loop
.done:
    ret

boot_msg db "부트로더 시작중... 커널 로딩중...", 0
err_msg  db "디스크 읽기 오류!", 0
boot_drive db 0

; 부트섹터의 총 크기를 510바이트로 채운 후, 부트 시그니처 (0xAA55) 추가
times 510 - ($ - $$) db 0
dw 0xAA55
