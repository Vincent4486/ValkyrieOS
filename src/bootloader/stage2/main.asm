bits 16

section _ENTRY class=CODE

extern _cstart_
global entry

; GDT for 32-bit protected mode
gdt_start:
    dq 0x0000000000000000  ; Null descriptor
gdt_code:
    dq 0x00CF9A000000FFFF  ; Code segment (32-bit, 4GB limit)
gdt_data:
    dq 0x00CF92000000FFFF  ; Data segment (32-bit, 4GB limit)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size
    dd gdt_start               ; GDT address

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

entry:
    cli
    ; Setup stack (16-bit for now)
    mov ax, ds
    mov ss, ax
    mov sp, 0
    mov bp, sp
    sti

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Switch to protected mode
    mov eax, cr0
    or al, 1       ; Set PE bit
    mov cr0, eax

    ; Far jump to reload CS with 32-bit code segment
    jmp CODE_SEG:protected_mode

bits 32
protected_mode:
    ; Reload segment registers with data segment
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup 32-bit stack
    mov esp, 0x90000  ; Example stack address (adjust as needed)
    mov ebp, esp

    ; Expect boot drive in dl (from real mode), send as argument
    xor edx, edx
    mov dl, [boot_drive]  ; You'll need to save this earlier
    push edx
    call _cstart_

    cli
    hlt

bits 16
boot_drive db 0  ; Temporary storage for boot drive