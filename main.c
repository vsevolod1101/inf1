#include <stdio.h>
#define TOTAL_MEMORY_SIZE 256
#define MEM_ADDRESS_ERR_HANDLER_ADDRESS 0
#define INIT_PC 1
#define INIT_ACC 0

// После кода команды (1, 2 или 3) в массиве сразу идёт адрес ячейки-операнда.
#define PC_SHIFT 1
int shift_pc(int *cpu_state, int *memory); 
int init_shift_pc(int *cpu_state, int *memory);
int load(int *cpu_state, int *memory);
int add(int *cpu_state, int *memory);
int sub(int *cpu_state, int *memory);
int mainloop_error_handler(int *cpu_state, int *memory);
int check_operand_link_on_memory_range(int *cpu_state, int *memory);
int check_link_err_handler(int *memory);

enum Opcode {
    HALT,
    LOAD,
    ADD,
    SUB    
};
enum Cpu_state {
    PC,
    ACC,
    IR,
    LINK_TO_ERR_HANDLER,
    INTERRUPTION,
    ERR_CODE
};
enum Interraption_state {
    IN_PROCESS,
    INTERRAPTED
};
enum Err_code {
    NO_ERR,
    SIG_FAULT
};
enum Err_handler_code {
    NO_HANDLED,
    HANDLED
};
int VIRT_MEM_SIZE; // load_memory присваивает постоянное значение
int load_memory(int *memory);
int init_cpu_state(int *cpu_state, int *memory);

int main(void)
{
    // инициализация
    int memory[TOTAL_MEMORY_SIZE];
    int cpu_state[8];
    VIRT_MEM_SIZE = load_memory(memory); // подразумевается гарантия корректного ввода, в обратном случае необходима валидация
    if (!init_cpu_state(cpu_state, memory)) {
        return 1;
    }

    // mainloop
    while (1) {
        printf("pc=%d", cpu_state[PC]);
        cpu_state[IR] = memory[cpu_state[PC]];
        printf(": команда %d", cpu_state[IR]);
        switch (cpu_state[IR])
        {
            case HALT: // стоп
                goto endmainloop;
            case LOAD: // загрузить в аккумулятор значение из ячейки памяти по адресу (прямая адресация)
                cpu_state[ERR_CODE] = load(cpu_state, memory);    
                break;
            case ADD: // прибавить к аккумулятору значение из ячейки памяти
                cpu_state[ERR_CODE] = add(cpu_state, memory);
                break;
            case SUB: // вычесть из аккумулятора значение из ячейки памяти
                cpu_state[ERR_CODE] = sub(cpu_state, memory); 
                break;
        }
        printf(", операнд=%d", memory[cpu_state[PC] + PC_SHIFT]);
        if (!mainloop_error_handler(cpu_state, memory)) {
            printf("Программа завершена с ошибкой.\n");
            return 1;
        }
    }
    endmainloop:
    printf(", acc=%d\n", cpu_state[ACC]);
    printf("Программа завершена без ошибки\n");
    return 0;
}

int init_cpu_state(int *cpu_state, int *memory)
{
    cpu_state[PC] = INIT_PC; // счётчик команд (pc); инициализирующее значение 1 обусловлено резервацией ядреса 0
    if (!init_shift_pc(cpu_state, memory)) {
        printf("Ошибка инициализации: не найдено валидных команд");
        return 0;
    }
    cpu_state[ACC] = INIT_ACC; // аккумулятор (acc), хранит текущий результат
    cpu_state[IR] = HALT; // регистр команд (ir), хранит текущий опкод
    if (!check_link_err_handler(memory)) {
        printf("Ошибка инициализации: невалидный адрес обработчка ошибок");
        return 0;
    }
    cpu_state[LINK_TO_ERR_HANDLER] = memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS]; // вектор прерываний (адрес обработчика ошибок)
    cpu_state[INTERRUPTION] = IN_PROCESS; // флаг остановки (0 = работает, 1 = остановлен)
    cpu_state[ERR_CODE] = NO_ERR; // код последней ошибки (0 = нет ошибки)
    cpu_state[6] = 0; // резерв для будущих расширений
    cpu_state[7] = 0; // резерв для будущих расширений
    return 1;
}

int load_memory(int *memory)
{   
    int data;
    int cell_address = 0;
    while (scanf("%d", &data) != -1) {
        memory[cell_address] = data;
        ++cell_address;
    }
    return cell_address + 1;
}

int init_shift_pc(int *cpu_state, int *memory) 
{
    // попытка обнаружения первой команды с операндом
    // игнорируются ведущие команды остановки
    for (int pc = cpu_state[PC]; pc < VIRT_MEM_SIZE - 1; ++pc) // учитывается архитектура обработки команд (следующая ячейка - адресс операнда)
        switch (memory[pc]) 
        {
             case LOAD:
             case ADD:
             case SUB:
                cpu_state[PC] = pc;
                return 1;
        }
    return 0;
}

int shift_pc(int *cpu_state, int *memory) // попытка переключения на следующую команду
{
    for (int pc = cpu_state[PC] + 1; pc < VIRT_MEM_SIZE; ++pc)
        switch (memory[pc]) 
        {
            case HALT:
            case LOAD:
            case ADD: 
            case SUB:
                cpu_state[PC] = pc;
                if (pc == VIRT_MEM_SIZE - 1 && memory[pc] != HALT) return 0; // проверка последней команды в концевой ячейке памяти на предмет потребности в дополнительных ячейках памяти) return SIG_FAULT;
            return 1;
        }
    return 0;
}

int check_link_err_handler(int *memory)
{
    if (memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS] < 0 || memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS] >= VIRT_MEM_SIZE)
        return 0;
    switch (memory[memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS]]) 
    {
        case HALT:
        case LOAD:
        case ADD: 
        case SUB:
            if (memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS] == VIRT_MEM_SIZE - 1 && memory[memory[MEM_ADDRESS_ERR_HANDLER_ADDRESS]] != HALT) return 0;
            return 1;
    }
    return 0;
}

int load(int *cpu_state, int *memory)
{   
    if (!check_operand_link_on_memory_range(cpu_state, memory))
        return SIG_FAULT;
    cpu_state[ACC] = memory[memory[cpu_state[PC] + PC_SHIFT]];
    return NO_ERR;
}

int add(int *cpu_state, int *memory)
{   
    if (!check_operand_link_on_memory_range(cpu_state, memory))
        return SIG_FAULT;
    cpu_state[ACC] += memory[memory[cpu_state[PC] + PC_SHIFT]];
    return NO_ERR;
}

int sub(int *cpu_state, int *memory)
{   
    if (!check_operand_link_on_memory_range(cpu_state, memory))
        return SIG_FAULT;
    cpu_state[ACC] -= memory[memory[cpu_state[PC] + PC_SHIFT]];
    return NO_ERR;
}

int check_operand_link_on_memory_range(int *cpu_state, int *memory) 
{
    int link = memory[cpu_state[PC] + PC_SHIFT];
    return (link >= 0 && link < VIRT_MEM_SIZE);
}

int mainloop_error_handler(int *cpu_state, int *memory)
{   
    char *err_msg;
    switch (cpu_state[ERR_CODE])
    {
        case SIG_FAULT:
            err_msg = "выход за границы памяти";
            break;
    }
    if (cpu_state[ERR_CODE] != NO_ERR) {
        if (cpu_state[INTERRUPTION] == INTERRAPTED) {
            printf("pc=%d: команда %d, операнд=%d, ОШИБКА:=%s (повторное прерывание)\n", cpu_state[PC], cpu_state[IR], memory[cpu_state[PC] + PC_SHIFT], err_msg);
            return 0;
        }
        cpu_state[INTERRUPTION] = INTERRAPTED;
        printf(", ОШИБКА:=%s\n", err_msg);
        cpu_state[PC] = cpu_state[LINK_TO_ERR_HANDLER];
        return 1;
    }
    if (!shift_pc(cpu_state, memory)) return 0;
    printf(", acc=%d\n", cpu_state[ACC]);  
    return 1;
}