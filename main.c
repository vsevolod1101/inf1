#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
#define EXIT_SUCCESS_MSG "Program is completed without error"
#define EXIT_FAILURE_MSG "Program is completed with error"

// коды команд
enum Opcode {
    OP_HALT, // 0
    OP_LOAD, // 1
    OP_ADD, // 2
    OP_SUB, // 3
    OP_MUL, // 4
    OP_DIV, // 5
    OP_JUMP, // 6
    OP_JUMP_IF_ZERO, // 7

    OP__COUNT
};
// коды состояний CPU
enum Cpu_state {
    CS_PC,
    CS_ACC,
    CS_IR,
    CS_LINK_TO_ERR_HANDLER,
    CS_INTERRUPTION,
    CS_ERR_CODE,
    CS_FUTUTE_ADDON_0,
    CS_FUTUTE_ADDON_1,
};
// коды состояний прерывания
enum Interruption_state {
    INTRS_IN_PROCESS,
    INTRS_INTERRAPTED
};
// коды ошибок
typedef enum Err_code {
    E_NO,
    E_INPUT,
    E_CACHE_MISSING,
    E_OP_SEG_FAULT,
    E_PC_SEG_FAULT,
    E_NO_COMMAND,
    E_INVALID_COMMAND,
    E_COMMAND_END_RANGE,
    E_JUMP_SEG_FAULT,
    E_DIVISION_ZERO,
} err_t;

typedef enum Boolean {
    FALSE,
    TRUE
} bool;

err_t load_memory(int *q_virt_memory_entries, int *memory);
err_t init_cpu_state(int *cpu_state, int *memory, int q_virt_memory_entries);
err_t shift_pc(int *pc, int *memory, int q_virt_memory_entries);
int _accessing_cp_data(int *cpu_state, int *memory);
int _accessing_cp_address(int *cpu_state, int *memory);
bool is_command(int content);
bool is_address_valid(int address, int limit);
err_t check_pc(int *pc, int *memory, int q_virt_memory_entries);
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
        fprintf(stderr, "Initialization error:\n");
        print_err(cpu_state, memory);
        goto exit_failure;
    }
    puts("Memory loaded");
    cpu_state[CS_ERR_CODE] = init_cpu_state(cpu_state, memory, q_virt_memory_entries);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
        fprintf(stderr, "Initialization error:\n");
        print_err(cpu_state, memory);
        goto exit_failure;
    }
    puts("CPU ready");
    puts("Initialization successful");

    // mainloop
    puts("CPU state:");
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
        puts(EXIT_SUCCESS_MSG);
        return EXIT_SUCCESS;
    }

    // error handler action
    cpu_state[CS_PC] = cpu_state[CS_LINK_TO_ERR_HANDLER];
    cpu_state[CS_ERR_CODE] = check_pc(&cpu_state[CS_PC], memory, q_virt_memory_entries);
    if (cpu_state[CS_ERR_CODE] != E_NO) {
            print_err(cpu_state, memory);
    } else {
        puts("Error handler action:");
        cpu_state[CS_IR] = memory[cpu_state[CS_PC]];
        if (cpu_state[CS_IR] == OP_HALT) {
            puts("HALT");
            goto exit_failure;
        }
        exec_command(cpu_state, memory, q_virt_memory_entries);
    }

    exit_failure:
    puts(EXIT_FAILURE_MSG);
    return EXIT_FAILURE;
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
            fprintf(stderr, "PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> JUMP_PC(%d). Error: JUMP segmentation fault%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC]+PC_SHIFT, memory[cpu_state[CS_PC]+PC_SHIFT], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG );
            break;
        case E_PC_SEG_FAULT:
            fprintf(stderr, "PC(%d). Error: PC segmentation fault%s\n", cpu_state[CS_PC], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG );
            break;
        case E_NO_COMMAND:
            fprintf(stderr, "Error: CPU not found any command%s\n", cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_COMMAND_END_RANGE:
            fprintf(stderr, "PC(%d) -> COMMAND(%d). Error: found command in the end of memory%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_OP_SEG_FAULT:
            fprintf(stderr, "PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> OPERAND ADDRESS(%d). Error: operand segmentation fault%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC] + PC_LOCAL_SHIFT, memory[cpu_state[CS_PC] + PC_LOCAL_SHIFT], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_INVALID_COMMAND:
            fprintf(stderr, "PC(%d) -> COMMAND(%d). Error: PC pointing to invalid command%s\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_INTERRUPTION] == INTRS_IN_PROCESS ? "" : REPEAT_INTARRUTION_MSG);
            break;
        case E_INPUT:
            fprintf(stderr, "Input error\n");
            break;
        case E_CACHE_MISSING:
            fprintf(stderr, "Input out of memory\n");
            break;
        case E_DIVISION_ZERO:
            fprintf(stderr, "PC(%d) -> COMMAND(%d); PC+SHIFT(%d) -> OPERATION_ADDRESS(%d) -> OPERAND(%d). Error: zero division\n", cpu_state[CS_PC], memory[cpu_state[CS_PC]], cpu_state[CS_PC] + PC_LOCAL_SHIFT, _accessing_cp_address(cpu_state, memory), _accessing_cp_data(cpu_state, memory));
            break;
    }
}

err_t load_memory(int *q_virt_memory_entries, int *memory) { // заполнение памяти данными
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

err_t init_cpu_state(int *cpu_state, int *memory, int q_virt_memory_entries) // инициализация CPU
{
    err_t rc;
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
                puts("Condition jump skipped");
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

err_t shift_pc(int *pc, int *memory, int q_virt_memory_entries) // попытка переключения на следующую команду; pc: ожидается обновлённый pc, указывающий не на выполненную команду
{   err_t rc;
    for (; *pc < q_virt_memory_entries; ++*pc) {
        rc = check_pc(pc, memory, q_virt_memory_entries);
        if (rc == E_NO) return E_NO;
        if (rc == E_INVALID_COMMAND) continue;
        if (rc != E_NO) return rc;
    }
    return E_NO_COMMAND;
}

err_t check_pc(int *pc, int *memory, int q_virt_memory_entries) { // полная проверка PC
    bool rc;
    rc = is_address_valid(*pc, q_virt_memory_entries);
    if (rc != TRUE) return E_PC_SEG_FAULT;
    if (memory[*pc] == OP_HALT) return E_NO;
    if (is_command(memory[*pc]) == TRUE) {
        if (*pc == q_virt_memory_entries - 1) return E_COMMAND_END_RANGE;
        rc = is_address_valid(memory[*pc + PC_LOCAL_SHIFT], q_virt_memory_entries);
        if (rc != TRUE) {
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

bool is_command(int content) {
    for (int i = 0; i < OP__COUNT; ++i) {
        if (i == content) return TRUE;
    }
    return FALSE;
}

bool is_address_valid(int address, int limit) {
    return (address < LOAD_MEMORY_START_POINT_ADDRESS || address >= limit) ? FALSE : TRUE;
}