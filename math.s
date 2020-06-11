.data
    .quad 1
    .quad 2
.text
_memcpy:
    movq 0(%rsi), %rax
    movq %rax, 0(%rdi)
    addq $8, %rsi
    addq $8, %rdi
    subq $8, %rdx
    jg _memcpy
    jmp *%r15
_memreturn:
    movq 0(%rbp), %rcx
    movq 8(%rbp), %rbx
    movq %rax, %rsi
    movq 0(%rsi), %rdx
    addq %rdx, %rsi
    leaq 8(%rbp), %rdi
    addq $8, %rdx
_memreturn_loop:
    movq 0(%rsi), %rax
    movq %rax, 0(%rdi)
    subq $8, %rsi
    subq $8, %rdi
    subq $8, %rdx
    jg _memreturn_loop
    leaq 8(%rdi), %rax
    movq %rax, %rsp
    movq %rcx, %rbp
    jmp *%rbx
.global _start
_start:
    movq %rsp, %rbp
    movq $0, %rdi
    callq exit
