	global memcpy_asm
	global memcmp_asm
	global memset_asm

	extern mem_fault_handler

	section .text

; void *memcpy_asm(void *dst, const void *src, size_t n)
; cdecl: dst @ [ebp+8], src @ [ebp+12], n @ [ebp+16]
memcpy_asm:
	push ebp
	mov ebp, esp
	push esi
	push edi

	mov edi, [ebp+8]    ; dst
	mov esi, [ebp+12]   ; src
	mov ecx, [ebp+16]   ; n

	; if n == 0, nothing to do
	test ecx, ecx
	jz .memcpy_done

	; if src or dst is NULL, do nothing (safe behavior)
	test esi, esi
	jz .memcpy_done
	test edi, edi
	jz .memcpy_done

	; detect 32-bit wrap for src and dst: if (addr + n) < addr then it's an overflow
	mov eax, esi
	add eax, ecx
	cmp eax, esi
	jb .memcpy_fault

	mov eax, edi
	add eax, ecx
	cmp eax, edi
	jb .memcpy_fault

	; compute src_end = esi + ecx
	mov eax, esi
	add eax, ecx

	; if dst < src or dst >= src_end => forward copy
	cmp edi, esi
	jb .forward_copy
	cmp edi, eax
	jae .forward_copy

	; overlapping with dst > src => backward copy (memmove semantics)
	; set pointers to last byte
	lea esi, [esi + ecx - 1]
	lea edi, [edi + ecx - 1]
	std
	; try aligned dword reverse copy if both aligned
	mov eax, esi
	test eax, 3
	jnz .rev_byte_copy
	mov eax, edi
	test eax, 3
	jnz .rev_byte_copy
	; both aligned: use movsd for bulk
	mov eax, ecx
	shr ecx, 2
	rep movsd
	mov ecx, eax
	and ecx, 3
	rep movsb
	cld
	jmp .memcpy_done

.rev_byte_copy:
	rep movsb
	cld


.forward_copy:
	; attempt aligned forward copy for speed when both pointers are dword-aligned
	mov eax, esi
	test eax, 3
	jnz .fwd_byte_copy
	mov eax, edi
	test eax, 3
	jnz .fwd_byte_copy
	; both aligned: use movsd for bulk copy
	mov eax, ecx
	shr ecx, 2
	rep movsd
	mov ecx, eax
	and ecx, 3
	rep movsb
	jmp .memcpy_done

.fwd_byte_copy:
	cld
	rep movsb

.memcpy_done:
	mov eax, [ebp+8]    ; return dst
	pop edi
	pop esi
	pop ebp
	ret

.memcpy_fault:
	; push arguments and call mem_fault_handler(dst, n, 1)
	push dword 1
	push dword [ebp+16] ; n
	push dword [ebp+8]  ; dst
	call mem_fault_handler
	add esp, 12
	jmp .memcpy_done

; int memcmp_asm(const void *s1, const void *s2, size_t n)
; returns (unsigned char)s1[i] - (unsigned char)s2[i]
; cdecl: s1@[ebp+8], s2@[ebp+12], n@[ebp+16]
memcmp_asm:
	push ebp
	mov ebp, esp
	push esi
	push edi

	mov esi, [ebp+8]    ; s1
	mov edi, [ebp+12]   ; s2
	mov ecx, [ebp+16]   ; n

	; if n == 0 -> equal
	test ecx, ecx
	jz .mc_equal

	; if either pointer NULL -> treat as equal (safe, avoids crash)
	test esi, esi
	jz .mc_equal
	test edi, edi
	jz .mc_equal

	; detect wrap for s1 and s2: if (addr + n) < addr then unsafe -> treat as equal
	mov eax, esi
	add eax, ecx
	cmp eax, esi
	jb .mc_equal
	mov eax, edi
	add eax, ecx
	cmp eax, edi
	jb .mc_fault

	cld
	repe cmpsb
	jne .mc_mismatch

.mc_equal:
	mov eax, 0
	jmp .mc_done

.mc_mismatch:
	; esi and edi have advanced past mismatch by 1
	movzx eax, byte [esi-1]
	movzx edx, byte [edi-1]
	sub eax, edx

.mc_done:
	pop edi
	pop esi
	pop ebp
	ret

.mc_fault:
	; call mem_fault_handler(s1, n, 2)
	push dword 2
	push dword [ebp+16]
	push dword [ebp+8]
	call mem_fault_handler
	add esp, 12
	jmp .mc_equal

; void *memset_asm(void *ptr, int value, size_t n)
; cdecl: ptr@[ebp+8], value@[ebp+12], n@[ebp+16]
memset_asm:
	push ebp
	mov ebp, esp
	push edi

	mov edi, [ebp+8]    ; ptr
	mov ecx, [ebp+16]   ; n

	; if n == 0, nothing to do
	test ecx, ecx
	jz .ms_done

	; if ptr is NULL, do nothing
	test edi, edi
	jz .ms_done


	; detect wrap: if (ptr + n) < ptr -> unsafe -> call fault handler
	mov eax, edi
	add eax, ecx
	cmp eax, edi
	jb .ms_fault

	mov al, byte [ebp+12] ; value (low byte)
	cld
	rep stosb

.ms_done:
	mov eax, [ebp+8]    ; return ptr
	pop edi
	pop ebp
	ret

.ms_fault:
	; call mem_fault_handler(ptr, n, 3)
	push dword 3
	push dword [ebp+16]
	push dword [ebp+8]
	call mem_fault_handler
	add esp, 12
	jmp .ms_done

