bits 16

section _TEXT class=CODE

; Video functions - only keeping the ones actually used
global _x86_VideoSetMode
global _x86_VideoWriteCharTeletype

; Set video mode
_x86_VideoSetMode:
    push bp
    mov bp, sp
    
    mov ax, [bp+2]      ; video mode
    int 0x10
    
    mov sp, bp
    pop bp
    ret

; Teletype output
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
