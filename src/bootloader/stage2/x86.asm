bits 32

section .text

global cursor_x
global cursor_y
cursor_x dd 11
cursor_y dd 11

;
; void x86_div64_32(uint64_t dividend, uint32_t divisor, uint64_t* quotientOut, uint32_t* remainderOut);
;
global x86_div64_32
x86_div64_32:

    ; make new call frame
    push ebp             ; save old call frame
    mov ebp, esp          ; initialize new call frame

    push ebx

    ; divide upper 32 bits
    mov eax, [ebp + 8]   ; eax <- upper 32 bits of dividend
    mov ecx, [ebp + 12]  ; ecx <- divisor
    xor edx, edx
    div ecx             ; eax - quot, edx - remainder

    ; store upper 32 bits of quotient
    mov ebx, [ebp + 16]
    mov [ebx + 4], eax

    ; divide lower 32 bits
    mov eax, [ebp + 4]   ; eax <- lower 32 bits of dividend
                        ; edx <- old remainder
    div ecx

    ; store results
    mov [ebx], eax
    mov ebx, [ebp + 20]
    mov [ebx], edx

    pop ebx

    ; restore old call frame
    mov esp, ebp
    pop ebp
    ret

;
; void x86_Video_WriteCharTeletype(char c, uint8_t page);
;
global x86_Video_WriteCharTeletype
x86_Video_WriteCharTeletype:
    
    ; make new call frame
    push ebp             ; save old call frame
    mov ebp, esp          ; initialize new call frame

    ; [ebp + 0] - old call frame
    ; [ebp + 4] - return address
    ; [ebp + 8] - first argument (character)
    ; [ebp + 12] - second argument (page) - ignored for direct VGA

    mov al, [ebp + 8]    ; al = character
    cmp al, 10           ; newline
    je .newline
    cmp al, 13           ; carriage return
    je .carr_return

    ; write character
    mov ah, 0x0F         ; white on black
    mov edi, 0xB8000     ; VGA text memory
    mov ebx, [cursor_y]
    imul ebx, 80
    add ebx, [cursor_x]
    mov [edi + ebx*2], ax
    inc dword [cursor_x]

    ; check wrap
    cmp dword [cursor_x], 80
    jl .done
    mov dword [cursor_x], 0
    inc dword [cursor_y]

    jmp .scroll_check

.newline:
    mov dword [cursor_x], 0
    inc dword [cursor_y]
    jmp .scroll_check

.carr_return:
    mov dword [cursor_x], 0
    jmp .done

.scroll_check:
    cmp dword [cursor_y], 25
    jl .done

    ; scroll up
    mov esi, 0xB8000 + 160  ; second line
    mov edi, 0xB8000
    mov ecx, 24*80*2 / 4    ; 24 lines, 80 chars, 2 bytes each, /4 for dwords
    rep movsd

    ; clear last line
    mov edi, 0xB8000 + 24*160
    mov ecx, 80
    mov ax, 0x0F20  ; space with attribute
    rep stosw

    mov dword [cursor_y], 24

.done:
    ; restore old call frame
    mov esp, ebp
    pop ebp
    ret

;
; bool x86_Disk_Reset(uint8_t drive);
;
global x86_Disk_Reset
x86_Disk_Reset:
    ; Stub: always fail in protected mode
    mov eax, 0
    ret

;
; bool x86_Disk_Read(uint8_t drive,
;                    uint16_t cylinder,
;                    uint16_t sector,
;                    uint16_t head,
;                    uint8_t count,
;                    void* dataOut);
;
global x86_Disk_Read
x86_Disk_Read:
    ; Stub: always fail in protected mode
    mov eax, 0
    ret

;
; bool x86_Disk_GetDriveParams(uint8_t drive,
;                              uint8_t* driveTypeOut,
;                              uint16_t* cylindersOut,
;                              uint16_t* sectorsOut,
;                              uint16_t* headsOut);
;
global x86_Disk_GetDriveParams
x86_Disk_GetDriveParams:
    ; Stub: always fail in protected mode
    mov eax, 0
    ret
