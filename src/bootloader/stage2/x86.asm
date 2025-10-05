bits 16

section _TEXT class=CODE

global _x86_VideoWriteCharTeletype
_x86_VideoWriteCharTeletype:
    push bp
    mov bp, sp

    push bx

    mov ah, 0Eh         ; teletype function
    mov al, [bp+2]      ; character to write
    mov bh, [bp+4]      ; page number

    int 0x10

    pop bx

    mov sp, bp
    pop bp
    ret
