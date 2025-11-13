global task_switch
task_switch:
    push ebp
    mov ebp, esp

    push ebx
    push esi
    push edi

    mov eax, esp;
    and eax, 0xfffff000; current

    ;[eax]表示 “以eax为地址的内存单元”，即当前任务结构体的stack成员（因为task_t的第一个成员就是stack，地址偏移为 0）。
    mov [eax], esp; 保存当前esp到当前任务的stack（后续切换回时使用）

    mov eax, [ebp + 8]; next
    mov esp, [eax]; esp = a->stack（0x1fec，任务a的栈顶）

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
