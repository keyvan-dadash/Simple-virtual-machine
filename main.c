


typedef enum {
    PSH,
    POP,
    ADD,
    SET,
    HLT
} InstructionSet;

typedef enum {
    A,
    B,
    C,
    D,
    E,
    F,
    NUM_OF_REGISTERS
} Registers;

int registers[NUM_OF_REGISTERS];

const int program[] = {
    PSH, 5,
    PSH, 6,
    ADD,
    POP,
    HLT
};

int ip = 0;
int sp = -1;
int stack[256];
int running = 1;

int fetch() {
    return program[ip];
}

void eval(int intr) {
    //printf("got %d\n", intr);
    switch (intr)
    {
    case HLT: {
        running = 0;
        break;
    }
    case PSH: {
        sp++;
        stack[sp] = program[++ip];
        ip++;
        break;
    }
    case POP: {
        printf("popped %d\n", stack[sp--]);
        ip++;
        break;
    }
    case ADD: {
        int a = stack[sp];
        sp--;
        int b = stack[sp];
        stack[sp] = a + b;
        ip++;
        break;
    }
    default:
        break;
    }
}
int main(int argc, char const *argv[])
{
    while (running)
    {
        int x = fetch();
        if (x == HLT) break;
        eval(x);
    }
    
}