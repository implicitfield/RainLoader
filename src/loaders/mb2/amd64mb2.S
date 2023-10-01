.section .text
.global JumpToAMD64MB2Kernel
JumpToAMD64MB2Kernel:
    mov	ebx, edx
    push	rcx

    // Setup bootloader magic and jump to kernel
    mov	eax, 0x36d76289
    ret
