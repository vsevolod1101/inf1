#include <stdio.h>
#include <stdlib.h>
#define LIMIT_MEMORY_SIZE 256
#define ADDRESS_ERR_HANDLER_ADDRESS 0 // адрес адреса обработчка ошибок
#define PC_INIT_SHIFT 1
#define PC_SHIFT 2
#define PC_LOCAL_SHIFT 1 // После кода команды (1, 2 или 3) в массиве сразу идёт адрес ячейки-операнда.
#define INIT_ACC 0

#define EXIT_SUCCESS_MSG "Программа завершена без ошибки"
#define EXIT_FAILURE_MSG "Программа завершена с ошибкой"

enum Opcode {
    OP_HALT,
    OP_LOAD,
    OP_ADD,
    OP_SUB,
    OP_JUMP    
};
enum Cpu_state {
    CS_PC,
    CS_ACC,
    CS_IR,
    CS_LINK_TO_ERR_HANDLER,
    CS_INTERRUPTION,
    CS_ERR_CODE
};
enum Interruption_state {
    INTRS_IN_PROCESS,
    INTRS_INTERRAPTED
};
typedef enum Err_code {
    E_NO,
    E_OP_SEG_FAULT,
    E_PC_SEG_FAULT,
    E_NO_COMMAND,
    E_COMMAND_END_RANGE,
} err_t;

int load_memory(int *memory);
err_t init_cpu_state(int *cpu_state, int *memory, int memory_limit);
err_t shift_pc(int *pc, int *memory, int memory_limit);
void print_err(int *cpu_state, int *memory);
int is_command(int content);
err_t check_address(int address, int memory_limit);
err_t check_pc(int *pc, int *memory, int memory_limit);
void exec_command(int *memory, int *cpu_state);
int _accessing_cp_data(int *memory, int *cpu_state);


int main(void)
{
    // Initialization
    int memory[LIMIT_MEMORY_SIZE];
    int cpu_state[8];
    int memory_limit = load_memory(memory);
    cpu_state[CS_ERR_CODE] = init_cpu_state(cpu_state, memory, memory_limit);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
        fprintf(stderr, "Ошибка инициализации. Error code: %d\n", cpu_state[CS_ERR_CODE]);
        goto exit_failure;
    }

    // mainloop
    while (cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS) {
        cpu_state[CS_IR] = memory[cpu_state[CS_PC]];
        if (cpu_state[CS_IR] == OP_HALT) break;
        exec_command(memory, cpu_state);
        printf("pc=%d: command %d, operand address=%d, acc=%d\n", cpu_state[CS_PC], cpu_state[CS_IR], memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT], cpu_state[CS_ACC]);
        if (cpu_state[CS_IR] == OP_JUMP)
            cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, memory_limit);
        else
            cpu_state[CS_PC] += PC_SHIFT;
            cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, memory_limit);
        if (cpu_state[CS_ERR_CODE] != E_NO) {
            print_err(cpu_state, memory);
            cpu_state[CS_INTERRUPTION] = INTRS_INTERRAPTED;
        }    
    }

    // Termination
    if (cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS) {
        puts(EXIT_SUCCESS_MSG);
        return EXIT_SUCCESS;
    }

    cpu_state[CS_PC] = cpu_state[CS_LINK_TO_ERR_HANDLER];
    cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, memory_limit);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
            print_err(cpu_state, memory);
    } else {
        cpu_state[CS_IR] = memory[cpu_state[CS_PC]];
        if (cpu_state[CS_IR] == OP_HALT) goto exit_failure;
        exec_command(memory, cpu_state);
        printf("Действие обработчика ошибок: pc=%d: command %d, operand address=%d, acc=%d\n", cpu_state[CS_PC], cpu_state[CS_IR], memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT], cpu_state[CS_ACC]);
    }

    exit_failure:
    puts(EXIT_FAILURE_MSG);
    return EXIT_FAILURE;
}

void print_err(int *cpu_state, int *memory) {
    switch (cpu_state[CS_ERR_CODE]){
        case E_PC_SEG_FAULT:
            fprintf(stderr, "pc=%d: Ошибка: %s%s\n", cpu_state[CS_PC], "Pointer command segmentation fault 😿", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : " (повторное прерывание)" );
            break;
        case E_NO_COMMAND:
            fprintf(stderr, "pc=%d: Ошибка: %s%s\n", cpu_state[CS_PC], "счётчик команд не обнаружил следующую команду", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : " (повторное прерывание)" );
            break;
        case E_COMMAND_END_RANGE:
            fprintf(stderr, "pc=%d: command %d, Ошибка: %s%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], "обнаружена команда в конце памяти", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : " (повторное прерывание)" );
            break;
        case E_OP_SEG_FAULT:
            fprintf(stderr, "pc=%d: command %d, operand address=%d, Ошибка: %s%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT], "Operand segmentation fault 😿", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : " (повторное прерывание)" );
            break;
    }
}

err_t init_cpu_state(int *cpu_state, int *memory, int memory_limit)
{
    err_t rc;
    cpu_state[CS_PC] = PC_INIT_SHIFT; // счётчик команд (pc)
    rc = shift_pc(&cpu_state[CS_PC], memory, memory_limit);
    if (rc != E_NO) return rc;
    cpu_state[CS_ACC] = INIT_ACC; // аккумулятор (acc), хранит текущий результат
    cpu_state[CS_IR] = OP_HALT; // регистр команд (ir), хранит текущий опкод
    cpu_state[CS_LINK_TO_ERR_HANDLER] = memory[ADDRESS_ERR_HANDLER_ADDRESS]; // вектор прерываний (адрес обработчика ошибок)
    cpu_state[CS_INTERRUPTION] = INTRS_IN_PROCESS; // флаг остановки (0 = работает, 1 = остановлен)
    cpu_state[6] = 0; // резерв для будущих расширений
    cpu_state[7] = 0; // резерв для будущих расширений
    return E_NO;
}


void exec_command(int *memory, int *cpu_state) {
    switch (cpu_state[CS_IR])
    {
        case OP_LOAD: // загрузить в аккумулятор значение из ячейки памяти по адресу (прямая адресация)
            cpu_state[CS_ACC] = _accessing_cp_data(memory, cpu_state);
            break;
        case OP_ADD: // прибавить к аккумулятору значение из ячейки памяти
            cpu_state[CS_ACC] += _accessing_cp_data(memory, cpu_state);
            break;
        case OP_SUB: // вычесть из аккумулятора значение из ячейки памяти
            cpu_state[CS_ACC] -= _accessing_cp_data(memory, cpu_state);
            break;
        case OP_JUMP: // безусловный переход
            cpu_state[CS_PC] = _accessing_cp_data(memory, cpu_state);
            break;
    }
}

int is_command(int content) {
    switch (content) 
    {
        case OP_LOAD:
        case OP_ADD: 
        case OP_SUB:
        case OP_JUMP:
            return 1;
    }
    return 0;
}

int _accessing_cp_data(int *memory, int *cpu_state) {
    return memory[memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT]];
}

int load_memory(int *memory)
{   
    int data;
    int cell_address = 0;
    while ((scanf("%d", &data) != EOF) && (cell_address < LIMIT_MEMORY_SIZE)) {
        memory[cell_address] = data;
        ++cell_address;
    }
    return cell_address;
}

err_t shift_pc(int *pc, int *memory, int memory_limit) // попытка переключения на следующую команду; pc: ожидается обновлённый pc, указывающий не на выполненную команду
{   err_t rc;
    for (; *pc < memory_limit; ++*pc) {
        rc = check_pc(pc, memory, memory_limit);
        if (rc == E_NO) return E_NO;
        if (rc == E_NO_COMMAND) continue;
        if (rc != E_NO) return rc;
    }
    return E_NO_COMMAND;
}

err_t check_pc(int *pc, int *memory, int memory_limit) {
    err_t rc;
    rc = check_address(*pc, memory_limit);
    if (rc != E_NO) return E_PC_SEG_FAULT;
    if (memory[*pc] == OP_HALT) return E_NO;
    if (is_command(memory[*pc])) {
        if (*pc == memory_limit - 1) return E_COMMAND_END_RANGE;
        rc = check_address(memory[*pc + PC_LOCAL_SHIFT], memory_limit);
        if (rc != E_NO) return rc;
        return E_NO;
    }
    return E_NO_COMMAND;
}

err_t check_address(int address, int memory_limit) {
    return (address < 0 || address >= memory_limit) ? E_OP_SEG_FAULT : E_NO;
}


