#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>


__UINT16_TYPE__ memory[__UINT16_MAX__];


enum {
    R_R0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

__UINT16_TYPE__ reg[R_COUNT];


enum {
    OP_BR, //opcode 0000
    OP_ADD, //opcode 0001
    OP_LD, //opcode 0010
    OP_ST, //opcode 0011
    OP_JSR, //opcode 0100
    OP_AND, //opcode 0101
    OP_LDR, //opcode 0110
    OP_STR, //opcode 0111
    OP_RTI, //opcode 1000
    OP_NOT, //opcode 1001
    OP_LDI, //opcode 1010
    OP_STI, //opcode 1011
    OP_JMP, //opcode 1100
    OP_RESERVED,//this opcode(mean 1101) reserved
    OP_LEA, //opcode 1110
    OP_TRAP, //opcode 1111
};

enum {
    TRAP_GETC  = 0x20,
    TRAP_OUT   = 0x21,
    TRAP_PUTS  = 0x22,
    TRAP_IN    = 0x23,
    TRAP_PUTSP = 0x24,
    TRAP_HALT  = 0x25
};

enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

__UINT16_TYPE__ sign_extend(__UINT16_TYPE__ x, int bit_count) //used in some operation such as add,...
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

__UINT16_TYPE__ littlerEndian(__UINT16_TYPE__ x)
{
    return (x << 8) | (x >> 8);
}

__UINT16_TYPE__ check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

void mem_write(__UINT16_TYPE__ address, __UINT16_TYPE__ value)
{
    memory[address] = value;
}

__UINT16_TYPE__ mem_read(__UINT16_TYPE__ address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

void update_flags(__UINT16_TYPE__ r) //due to specification we must update flags after some instruction
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

void br_oper(__UINT16_TYPE__ instr)
{
    int n = (instr >> 11) & 0x1; //and with N flag
    int z = (instr >> 10) & 0x1; //and with Z flag
    int p = (instr >> 9) & 0x1; //and with P flag

    int instr_flag = (n << 2) + (z << 1) + (p << 0);

    if(reg[R_COND] & instr_flag){
        reg[R_PC] += sign_extend(instr & 0x1ff, 9);
    }

}

void add_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ sr1_reg = (instr >> 6) & 0x7;
    int type = (instr >> 5) & 0x1; //determine if is imm or has second source

    if(type) { //imm
        reg[des_reg] = reg[sr1_reg] + sign_extend(instr & 0x1f, 5);
    } else { //has second source
        __UINT16_TYPE__ sr2_reg = instr & 0x7;
        reg[des_reg] = reg[sr1_reg] + reg[sr2_reg];
    }
    update_flags(des_reg);
}

void ld_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = mem_read(reg[R_PC] + offset);
    update_flags(des_reg);
}

void st_oper(__UINT16_TYPE__ instr) 
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x1ff, 9);

    mem_write(reg[R_PC] + offset, reg[sr_reg]);
}

void jsr_oper(__UINT16_TYPE__ instr)
{
    int type = (instr >> 11) & 0x1; //determine is jsr or jsrr

    reg[R_R7] = reg[R_PC];

    if(type) { //jsr
        __UINT16_TYPE__ offset = instr & 0x7ff;
        reg[R_PC] += sign_extend(offset, 11);
    } else { //jsrr
        __UINT16_TYPE__ base_r = (instr >> 6) & 0x7;
        reg[R_PC] = reg[base_r];
    }  
}

void and_oper(__UINT16_TYPE__ instr)
{
    int type = (instr >> 5) & 0x1;
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ sr1_reg = (instr >> 6) & 0x7;

    if(type) { //imm
        __UINT16_TYPE__ imm = sign_extend(instr & 0x1f, 5);
        reg[des_reg] = reg[sr1_reg] & imm;
    } else { //has sr2
        __UINT16_TYPE__ sr2_reg = instr & 0x7;
        reg[des_reg] = reg[sr1_reg] & reg[sr2_reg];
    }
    update_flags(des_reg);
}

void ldr_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ base_reg = (instr >> 6) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x3f, 6);

    reg[des_reg] = mem_read(reg[base_reg] + offset);
    update_flags(des_reg);
}

void str_oper(__UINT16_TYPE__ instr) 
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ base_reg = (instr >> 6) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x3f, 6);

    mem_write(reg[base_reg] + offset, reg[sr_reg]);
}

void rti_oper(__UINT16_TYPE__ instr)
{
    abort();
}

void not_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ sr_reg = (instr >> 6) & 0x7;
    
    reg[des_reg] = ~reg[sr_reg];
    update_flags(des_reg);
}

void ldi_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = mem_read(mem_read(reg[R_PC] + offset));
    update_flags(des_reg);
}

void sti_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x1ff, 9);

    mem_write(mem_read(reg[R_PC] + offset), reg[sr_reg]);
}

void jmp_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ base_reg = (instr >> 6) & 0x7;
    reg[R_PC] = reg[base_reg];
}

void lea_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = reg[R_PC] + offset;
}

void trap_oper(__UINT16_TYPE__ instr)
{
    __UINT8_TYPE__ trapvec8 = instr & 0xff;

    switch (trapvec8)
    {
    case TRAP_GETC:{
        reg[R_R0] = (__UINT16_TYPE__)getchar();
    }
        break;
    case TRAP_OUT:{
        printf("%c", (char)reg[R_R0]);
    }
        break;
    case TRAP_PUTS:{
        __UINT16_TYPE__* c = memory + reg[R_R0];
        while (*c)
        {
            putc((char)*c, stdout);
            ++c;
        }
        fflush(stdout);
    }
        break;
    case TRAP_IN:{
        printf("Enter a character: ");
        char c = getchar();
        putc(c, stdout);
        reg[R_R0] = (__UINT16_TYPE__)c;
    }
        break;
    case TRAP_PUTSP:{
        __UINT16_TYPE__* c = memory + reg[R_R0];
        while (*c)
        {
            char char1 = (*c) & 0xFF;
            putc(char1, stdout);
            char char2 = (*c) >> 8;
            if (char2) putc(char2, stdout);
            ++c;
        }
        fflush(stdout);
    }
        break;
    case TRAP_HALT:{
        puts("HALT");
        fflush(stdout);
    }
        break;
    default:
        break;
    }
}

void read_image_file(FILE *file) 
{
    __UINT16_TYPE__ origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = littlerEndian(origin);

    __UINT16_TYPE__ max_read = UINT16_MAX - origin;
    __UINT16_TYPE__* p = memory + origin;
    __SIZE_TYPE__ read = fread(p, sizeof(__UINT16_TYPE__), max_read, file);

    while (read-- > 0)
    {
        *p = littlerEndian(*p);
        ++p;
    }
}

int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}


int main(int argc, const char* argv[])
{
    /* Load Arguments */
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    
    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    /* Setup */
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();


    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        //printf("oper: %d \n", op);

        switch (op)
        {
            case OP_ADD:
                add_oper(instr);
                break;
            case OP_AND:
                and_oper(instr);
                break;
            case OP_NOT:
                not_oper(instr);
                break;
            case OP_BR:
                br_oper(instr);
                break;
            case OP_JMP:
                jmp_oper(instr);
                break;
            case OP_JSR:
                jsr_oper(instr);
                break;
            case OP_LD:
                ld_oper(instr);
                break;
            case OP_LDI:
                ldi_oper(instr);
                break;
            case OP_LDR:
                ldr_oper(instr);
                break;
            case OP_LEA:
                lea_oper(instr);
                break;
            case OP_ST:
                st_oper(instr);
                break;
            case OP_STI:
                sti_oper(instr);
                break;
            case OP_STR:
                str_oper(instr);
                break;
            case OP_TRAP:
                trap_oper(instr);
                break;
            case OP_RESERVED:
            case OP_RTI:
            default:
                /* BAD OPCODE */
                abort();
                break;
        }
    }
    /* Shutdown */
    restore_input_buffering();

}