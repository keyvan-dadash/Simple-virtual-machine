



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
    op_TRAP, //opcode 1111
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

__UINT16_TYPE__ sign_extend(__UINT16_TYPE__ x, int bit_count) //used in some operation such as add,...
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
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
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = mem_read(reg[R_PC] + pc_offset);
    update_flags(des_reg);
}

void st_oper(__UINT16_TYPE__ instr) 
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x1ff, 9);

    mem_write(reg[R_PC] + pc_offset, reg[sr_reg]);
}

void jsr_oper(__UINT16_TYPE__ instr)
{
    int type = (instr >> 11) & 0x1; //determine is jsr or jsrr

    reg[R_R7] = reg[R_PC];

    if(type) { //jsr
        __UINT16_TYPE__ pc_offset = instr & 0x7ff;
        reg[R_PC] += sign_extend(pc_offset, 11);
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
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x3f, 6);

    reg[des_reg] = mem_read(reg[base_reg] + pc_offset);
    update_flags(des_reg);
}

void str_oper(__UINT16_TYPE__ instr) 
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ base_reg = (instr >> 6) & 0x7;
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x3f, 6);

    mem_write(reg[base_reg] + pc_offset, reg[sr_reg]);
}

void rti_oper(__UINT16_TYPE__ instr)
{
    abort();
}

void not_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ sr_reg = (instr >> 6) & 0x7;
    
    reg[des_reg] = !reg[sr_reg];
    update_flags(des_reg);
}

void ldi_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = mem_read(mem_read(reg[R_PC] + pc_offset));
    update_flags(des_reg);
}

void sti_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ sr_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x1ff, 9);

    mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr_reg]);
}

void jmp_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ base_reg = (instr >> 6) & 0x7;
    reg[R_PC] = reg[base_reg];
}

void lea_oper(__UINT16_TYPE__ instr)
{
    __UINT16_TYPE__ des_reg = (instr >> 9) & 0x7;
    __UINT16_TYPE__ pc_offset = sign_extend(instr & 0x1ff, 9);

    reg[des_reg] = reg[pc_offset] + pc_offset;
}

void trap_oper(__UINT16_TYPE__ instr)
{

}