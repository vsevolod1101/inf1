#include <stdio.h>

#define LIMIT_MEMORY_SIZE 256 // объём памяти, измеряющаяся в единиах размера массива С.
#define ADDRESS_ERR_HANDLER_ADDRESS 0 // адрес адреса обработчка ошибок
#define PC_INIT 1 // исходное значение PC
#define PC_SHIFT 2 // стартовое смещение PC после выполнения команды
#define PC_LOCAL_SHIFT 1 // После кода команды (1, 2 или 3) в массиве сразу идёт адрес ячейки-операнда.
#define INIT_ACC 0 // исходное значение ACC
#define COND_JUMP_AT_ACC_VAL 0 // значение ACC относительно условного перехода 
#define LOAD_MEMORY_START_POINT_ADDRESS 0 // стартовый адресс загрузки данных в память
#define Q_CPU_STATES 8 // количество состояний CPU
#define FUTURE_ADDON_STUB 0 // заглушка для неопределенных состояний CPU
// некоторые текстовые литералы, использующиеся в логах
#define REPEAT_INTARRUTION_MSG " (repeat interruption)" 
#define EXIT_SUCCESS_MSG "Program is completed without error\n"
#define EXIT_FAILURE_MSG "Program is completed with error\n"

// коды команд

#define OP_HALT 0
#define OP_LOAD 1
#define OP_ADD 2
#define OP_SUB 3
#define OP_MUL 4
#define OP_DIV 5
#define OP_JUMP 6
#define OP_JUMP_IF_ZERO 7

#define OP__COUNT 8

// коды состояний CPU
#define CS_PC 0
#define CS_ACC 1
#define CS_IR 2
#define CS_LINK_TO_ERR_HANDLER 3
#define CS_INTERRUPTION 4
#define CS_ERR_CODE 5
#define CS_FUTUTE_ADDON_0 6
#define CS_FUTUTE_ADDON_1 7

// коды состояний прерывания
#define INTRS_IN_PROCESS 0
#define INTRS_INTERRAPTED 1

// коды ошибок
#define E_NO 0
#define E_INPUT 1
#define E_CACHE_MISSING 2
#define E_OP_SEG_FAULT 3
#define E_PC_SEG_FAULT 4
#define E_NO_COMMAND 5
#define E_INVALID_COMMAND 6
#define E_COMMAND_END_RANGE 7
#define E_JUMP_SEG_FAULT 8
#define E_DIVISION_ZERO 9

int load_memory(int *q_virt_memory_entries, int *memory);
int init_cpu_state(int *cpu_state, int *memory, int q_virt_memory_entries);
int shift_pc(int *pc, int *memory, int q_virt_memory_entries);
int _accessing_cp_data(int *cpu_state, int *memory);
int _accessing_cp_address(int *cpu_state, int *memory);
int is_command(int content);
int is_address_valid(int address, int limit);
int check_pc(int *pc, int *memory, int q_virt_memory_entries);
void exec_command(int *cpu_state, int *memory, int q_virt_memory_entries);
void print_cpu_state(int *cpu_state, int *memory);
void print_err(int *cpu_state, int *memory);



int main(void)
{
    // Initialization
    int memory[LIMIT_MEMORY_SIZE];
    int cpu_state[Q_CPU_STATES];
    int q_virt_memory_entries = LOAD_MEMORY_START_POINT_ADDRESS;
    cpu_state[CS_ERR_CODE] = load_memory(&q_virt_memory_entries, memory);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
        printf("Initialization error:\n");
        print_err(cpu_state, memory);
        goto exit_failure;
    }
    printf("Memory loaded\n");
    cpu_state[CS_ERR_CODE] = init_cpu_state(cpu_state, memory, q_virt_memory_entries);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
        printf("Initialization error:\n");
        print_err(cpu_state, memory);
        goto exit_failure;
    }
    printf("CPU ready\n");
    printf("Initialization successful\n");

    // mainloop
    printf("CPU state:\n");
    while (cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS) {
        cpu_state[CS_IR] = memory[cpu_state[CS_PC]];
        if (cpu_state[CS_IR] == OP_HALT) break;
        exec_command(cpu_state, memory, q_virt_memory_entries);
        if (cpu_state[CS_ERR_CODE] != E_NO) {
            print_err(cpu_state, memory);
            cpu_state[CS_INTERRUPTION] = INTRS_INTERRAPTED;
        }    
    }

    // Termination
    if (cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS) {
        printf(EXIT_SUCCESS_MSG);
        return 0;
    }

    // error handler action
    cpu_state[CS_PC] = cpu_state[CS_LINK_TO_ERR_HANDLER];
    cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
            print_err(cpu_state, memory);
    } else {
        printf("Error handler action:\n");
        cpu_state[CS_IR] = memory[cpu_state[CS_PC]];
        if (cpu_state[CS_IR] == OP_HALT) {
            printf("HALT\n");
            goto exit_failure;
        }
        exec_command(cpu_state, memory, q_virt_memory_entries);
    }

    exit_failure:
    printf(EXIT_FAILURE_MSG);
    return 1;
}

void print_cpu_state(int *cpu_state, int *memory) { // логирование состояния CPU
    switch (cpu_state[CS_IR]) {
        case OP_JUMP:
        case OP_JUMP_IF_ZERO:
            printf("PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> JUMP_ADDRESS(%d); ACC(%d).\n", cpu_state[CS_PC], cpu_state[CS_IR], cpu_state[CS_PC] + PC_LOCAL_SHIFT, _accessing_cp_address(cpu_state, memory), cpu_state[CS_ACC]);
            break;
        default:
            printf("PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> OPERATION_ADDRESS(%d) -> OPERAND(%d); ACC(%d).\n", cpu_state[CS_PC], cpu_state[CS_IR], cpu_state[CS_PC] + PC_LOCAL_SHIFT, _accessing_cp_address(cpu_state, memory), _accessing_cp_data(cpu_state, memory), cpu_state[CS_ACC]);
            break;
    }
}

void print_err(int *cpu_state, int *memory) { // логирование ошибок
    switch (cpu_state[CS_ERR_CODE]){
        case E_JUMP_SEG_FAULT:
            printf("PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> JUMP_PC(%d). Error: JUMP segmentation fault%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC]+PC_SHIFT, memory[cpu_state[CS_PC]+PC_SHIFT], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG );
            break;
        case E_PC_SEG_FAULT:
            printf("PC(%d). Error: PC segmentation fault%s\n", cpu_state[CS_PC], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG );
            break;
        case E_NO_COMMAND:
            printf("Error: CPU not found any command%s\n", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_COMMAND_END_RANGE:
            printf("PC(%d) -> COMMAND(%d). Error: found command in the end of memory%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_OP_SEG_FAULT:
            printf("PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> OPERAND ADDRESS(%d). Error: operand segmentation fault%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC] + PC_LOCAL_SHIFT, memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_INVALID_COMMAND:
            printf("PC(%d) -> COMMAND(%d). Error: PC pointing to invalid command%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_INPUT:
            printf("Input error\n");
            break;
        case E_CACHE_MISSING:
            printf("Input out of memory\n");
            break;
        case E_DIVISION_ZERO:
            printf("PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> OPERATION_ADDRESS(%d) -> OPERAND(%d). Error: zero division\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC] + PC_LOCAL_SHIFT, _accessing_cp_address(cpu_state, memory), _accessing_cp_data(cpu_state, memory));
            break;
    }
}

int load_memory(int *q_virt_memory_entries, int *memory) { // заполнение памяти данными
    int data;
    int scanf_rc;
    printf("Loading data into memory...\n");
    do {
        scanf_rc = scanf("%d", &data);
        if (!scanf_rc) return E_INPUT;
        if (scanf_rc == EOF) break;
        memory[*q_virt_memory_entries] = data;
        printf("memory[%d]=%d\n", *q_virt_memory_entries, data);
        ++*q_virt_memory_entries;
    }
    while (*q_virt_memory_entries < LIMIT_MEMORY_SIZE);
    if (scanf_rc == EOF) return E_NO;
    scanf_rc = scanf("%d", &data);
    if (scanf_rc != EOF) return E_CACHE_MISSING;
    return E_NO; // случай, когда данных пришло ровно столько столько выделено места
}

int init_cpu_state(int *cpu_state, int *memory, int q_virt_memory_entries) // инициализация CPU
{
    int rc;
    cpu_state[CS_PC] = PC_INIT; // счётчик команд (pc)
    rc = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
    if (rc != E_NO) return rc;
    cpu_state[CS_ACC] = INIT_ACC; // аккумулятор (acc), хранит текущий результат
    cpu_state[CS_IR] = OP_HALT; // регистр команд (ir), хранит текущий опкод
    cpu_state[CS_LINK_TO_ERR_HANDLER] = memory[ADDRESS_ERR_HANDLER_ADDRESS]; // вектор прерываний (адрес обработчика ошибок)
    cpu_state[CS_INTERRUPTION] = INTRS_IN_PROCESS; // флаг остановки (0 = работает, 1 = остановлен)
    cpu_state[CS_FUTUTE_ADDON_0] = FUTURE_ADDON_STUB; // резерв для будущих расширений
    cpu_state[CS_FUTUTE_ADDON_1] = FUTURE_ADDON_STUB; // резерв для будущих расширений
    return E_NO;
}


void exec_command(int *cpu_state, int *memory, int q_virt_memory_entries) { // выполнение команд
    switch (cpu_state[CS_IR])
    {
        case OP_LOAD: // загрузить в аккумулятор значение из ячейки памяти по адресу (прямая адресация)
            cpu_state[CS_ACC] = _accessing_cp_data(cpu_state, memory);
            print_cpu_state(cpu_state, memory);
            cpu_state[CS_PC] += PC_SHIFT;
            cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            break;
        case OP_ADD: // прибавить к аккумулятору значение из ячейки памяти
            cpu_state[CS_ACC] += _accessing_cp_data(cpu_state, memory);
            print_cpu_state(cpu_state, memory);
            cpu_state[CS_PC] += PC_SHIFT;
            cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            break;
        case OP_SUB: // вычесть из аккумулятора значение из ячейки памяти
            cpu_state[CS_ACC] -= _accessing_cp_data(cpu_state, memory);
            print_cpu_state(cpu_state, memory);
            cpu_state[CS_PC] += PC_SHIFT;
            cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            break;
        case OP_MUL: // умножить аккумулятор на значение из ячейки памяти
            cpu_state[CS_ACC] *= _accessing_cp_data(cpu_state, memory);
            print_cpu_state(cpu_state, memory);
            cpu_state[CS_PC] += PC_SHIFT;
            cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            break;
        case OP_DIV: // разделить аккумулятор на значение из ячейки памяти
            if (_accessing_cp_data(cpu_state, memory) != 0) {
                cpu_state[CS_ACC] /= _accessing_cp_data(cpu_state, memory);
                print_cpu_state(cpu_state, memory);
                cpu_state[CS_PC] += PC_SHIFT;
                cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            } else {
                cpu_state[CS_ERR_CODE] = E_DIVISION_ZERO;
                print_cpu_state(cpu_state, memory);
            }
            break;
        case OP_JUMP: // безусловный переход
            print_cpu_state(cpu_state, memory);
            cpu_state[CS_PC] = _accessing_cp_address(cpu_state, memory);
            cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            break;
        case OP_JUMP_IF_ZERO: // условный переход
            if (cpu_state[CS_ACC] == COND_JUMP_AT_ACC_VAL) {
                print_cpu_state(cpu_state, memory);
                cpu_state[CS_PC] = _accessing_cp_address(cpu_state, memory);
                cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            } else {
                printf("Condition jump skipped");
                cpu_state[CS_PC] += PC_SHIFT;
                cpu_state[CS_ERR_CODE] = shift_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
            }
            break;
    }
}

int _accessing_cp_data(int *cpu_state, int *memory) {
    return memory[memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT]];
}

int _accessing_cp_address(int *cpu_state, int *memory) {
    return memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT];
}

int shift_pc(int *pc, int *memory, int q_virt_memory_entries) // попытка переключения на следующую команду; pc: ожидается обновлённый pc, указывающий не на выполненную команду
{   int rc;
    for (; *pc < q_virt_memory_entries; ++*pc) {
        rc = check_pc(pc, memory, q_virt_memory_entries);
        if (rc == E_NO) return E_NO;
        if (rc == E_INVALID_COMMAND) continue;
        if (rc != E_NO) return rc;
    }
    return E_NO_COMMAND;
}

int check_pc(int *pc, int *memory, int q_virt_memory_entries) { // полная проверка PC
    int rc;
    rc = is_address_valid(*pc, q_virt_memory_entries);
    if (rc != 1) return E_PC_SEG_FAULT;
    if (memory[*pc] == OP_HALT) return E_NO;
    if (is_command(memory[*pc]) == 1) {
        if (*pc == q_virt_memory_entries - 1) return E_COMMAND_END_RANGE;
        rc = is_address_valid(memory[*pc + PC_LOCAL_SHIFT], q_virt_memory_entries);
        if (rc != 1) {
            switch (memory[*pc]) {
                case OP_JUMP:
                case OP_JUMP_IF_ZERO:
                    return E_JUMP_SEG_FAULT;
            }
            return E_OP_SEG_FAULT;
        }
        return E_NO;
    }
    return E_INVALID_COMMAND;
}

int is_command(int content) {
    for (int i = 0; i < OP__COUNT; ++i) {
        if (i == content) return 1;
    }
    return 0;
}

int is_address_valid(int address, int limit) {
    return (address < LOAD_MEMORY_START_POINT_ADDRESS || address >= limit) ? 0 : 1;
}