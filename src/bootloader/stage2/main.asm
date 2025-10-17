bits 16

section .text

extern cstart_
global entry

entry:
    cli

    ; Set VGA text mode first
    mov ax, 0x0003
    int 0x10

    ; Show progress
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x0753   ; 'S' for Start

    ; Save boot drive (DL contains boot drive from stage1)
    mov [boot_drive], dl

    ; Setup stack
    mov ax, ds
    mov ss, ax
    mov sp, 0x1000
    mov bp, sp
    mov word [es:2], 0x0742   ; 'B'

    ; Enable A20 before switching to protected mode
    call EnableA20

        ; Debug: mark G (about to load GDT)
        mov ax, 0xB800
        mov es, ax
        mov word [es:4], 0x0747   ; 'G'

        ; Compute runtime GDT base and write it into gdt_descriptor (dd)
        call get_gdt_base

        ; Load GDT
        lgdt [gdt_descriptor]

        ; Debug: mark L (GDT loaded)
        mov word [es:6], 0x074C   ; 'L'

    ; Switch to protected mode: set PE in CR0
    mov eax, cr0
    or eax, 1                 ; Set PE bit
    mov cr0, eax

    mov word [es:4], 0x0750   ; 'P'

    ; Far jump to switch to 32-bit mode
    jmp CODE_SEG:protected_mode

    ; If we reach here, the far jump FAILED
    jmp hang16

bits 32
protected_mode:

    cli
    ; Setup 32-bit segments (use the data selector)
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup 32-bit stack
    mov esp, 0x90000
    mov ebp, esp

    ; Clear BSS area
    mov edi, __bss_start
    mov ecx, __end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; Push boot drive and call C entry
    xor edx, edx
    mov dl, [boot_drive]
    push edx
    call cstart_
    add esp, 4

hang32:
    cli
    hlt
    jmp hang32

bits 16
hang16:
    cli
    hlt
    jmp hang16

; === Data section ===
boot_drive: db 0

; Temporary storage for A20 routines
a20_temp: db 0

; === GDT ===
align 8
gdt_start:
    dq 0x0000000000000000    ; Null descriptor
gdt_code:
    dq 0x00CF9A000000FFFF    ; 32-bit code segment
gdt_data:
    dq 0x00CF92000000FFFF    ; 32-bit data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    ; GDTR base must point at the actual GDT runtime address.
    ; gdt_start is at offset 0xA0 in the VMA, stage1 loads stage2 at 0x20000,
    ; so the runtime address is 0x20000 + 0xA0 = 0x200A0.
    dd 0x000200A0

; Segment selectors
CODE_SEG equ gdt_code - gdt_start  ; 0x08
DATA_SEG equ gdt_data - gdt_start  ; 0x10

; === A20 / 8042 keyboard controller helpers ===
EnableA20:
    ; 16-bit context
    call A20WaitInput
    mov al, KbdControllerDisableKeyboard
    out KbdControllerCommandPort, al

    ; Request read output port
    call A20WaitInput
    mov al, KbdControllerReadCtrlOutputPort
    out KbdControllerCommandPort, al

    call A20WaitOutput
    in al, KbdControllerDataPort
    mov [a20_temp], al

    ; Request write output port
    call A20WaitInput
    mov al, KbdControllerWriteCtrlOutputPort
    out KbdControllerCommandPort, al

    call A20WaitInput
    mov al, [a20_temp]
    or al, 0x02            ; set A20 (bit 1)
    out KbdControllerDataPort, al

    ; Re-enable keyboard
    call A20WaitInput
    mov al, KbdControllerEnableKeyboard
    out KbdControllerCommandPort, al

    ret

A20WaitInput:
    in al, KbdControllerCommandPort
    test al, 2
    jnz A20WaitInput
    ret

A20WaitOutput:
    in al, KbdControllerCommandPort
    test al, 1
    jz A20WaitOutput
    ret

LoadGDT:
    lgdt [gdt_descriptor]
    ret

KbdControllerDataPort               equ 0x60
KbdControllerCommandPort            equ 0x64
KbdControllerDisableKeyboard        equ 0xAD
KbdControllerEnableKeyboard         equ 0xAE
KbdControllerReadCtrlOutputPort     equ 0xD0
KbdControllerWriteCtrlOutputPort    equ 0xD1

; Symbols expected by C runtime / linker
extern __bss_start
extern __end