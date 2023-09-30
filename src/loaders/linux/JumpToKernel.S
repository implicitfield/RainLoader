// Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent

.section .text

.global JumpToKernel
JumpToKernel:
    // Set up for executing kernel. BP in %esi, entry point on the stack
    // (64-bit when the 'ret' will use it as 32-bit, but we're little-endian)
    mov		rsi, rdx
    push	rcx

    // Jump into the compatibility mode CS
    push	0x10
    lea		rax, [rip + JumpToKernel.0]
    push	rax
    retfq

.code32
JumpToKernel.0:
    mov		eax, 0x18
    mov		ds, eax
    mov		es, eax
    mov		fs, eax
    mov		gs, eax
    mov		ss, eax

    // Disable paging
    mov		eax, cr0
    btc		eax, 0x1f
    mov		cr0, eax

    // Disable long mode in EFER
    mov		ecx, 0xc0000080
    rdmsr
    btc		eax, 0x8
    wrmsr

    // Disable PAE
    mov		eax, cr4
    btc		eax, 0x5
    mov		cr4, eax

    xor		ebp, ebp
    xor		edi, edi
    xor		ebx, ebx
    ret

.code64
.global JumpToUefiKernel
JumpToUefiKernel:
    mov		rdi, rcx
    mov		rsi, rdx
    mov		rdx, r8
    xor		rax, rax
    mov		eax, dword ptr [r8 + 0x264]
    add		r9, rax
    add		r9, 0x200
    call	r9
    ret
