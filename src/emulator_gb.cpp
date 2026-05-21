#include <iostream>
#include <cstdint>

struct Registers {
    uint8_t A = 0x01;
    uint8_t B = 0x00;
    uint8_t C = 0x13;
    uint8_t D = 0x00;
    uint8_t E = 0xD8;
    uint8_t H = 0x01;
    uint8_t L = 0x4D;
    uint8_t F = 0xB0;
    uint16_t PC = 0x0100;
    uint16_t SP = 0xFFFE;
    bool halted = false;//オペコード0ｘ76実装時に追加20260508
    bool IME = false;//オペコード0xD9実装時に追加20260514
};

// ============================================
// フラグ定数
// ============================================
const uint8_t FLAG_Z = 0x80;
const uint8_t FLAG_N = 0x40;
const uint8_t FLAG_H = 0x20;
const uint8_t FLAG_C = 0x10;


// ============================================
// フラグ操作
// ============================================
void setFlag(Registers& reg, uint8_t flag, bool value) {
    if (value) reg.F |= flag;
    else       reg.F &= ~flag;
}

// ============================================
// フラグ操作2
// ============================================
bool getFlag(Registers& reg, uint8_t flag) {
    return (reg.F & flag) != 0;
}
// ============================================
// メモリの定義16bit
// ============================================
struct aMemory {
    uint8_t data[65536] = {};
};


struct Memory {
    uint8_t rom[0x8000];      // 0x0000〜0x7FFF ROM
    uint8_t vram[0x2000];     // 0x8000〜0x9FFF VRAM
    uint8_t extram[0x2000];   // 0xA000〜0xBFFF 外部RAM
    uint8_t ram[0x2000];      // 0xC000〜0xDFFF 内部RAM
    uint8_t oam[0xA0];        // 0xFE00〜0xFE9F OAM
    uint8_t io[0x80];         // 0xFF00〜0xFF7F IOポート
    uint8_t hram[0x7F];       // 0xFF80〜0xFFFE スタック領域
    uint8_t ie;               // 0xFFFF 割り込みレジスタ
};

uint8_t readMemory(Memory& mem, uint16_t address) {
    if (address <= 0x7FFF) {
        return mem.rom[address];
    
    } else if (address <= 0x9FFF) {
        return mem.vram[address-0x8000];

    } else if (address <= 0xBFFF) {
        return mem.extram[address - 0xA000]; 

    } else if (address <= 0xDFFF) {
        return mem.ram[address - 0xC000];

    } else if (address <= 0xFE9F) {
        return mem.oam[address - 0xFE00];
    
    } else if (address <= 0xFF7F) {
        return mem.io[address - 0xFF00];

    } else if (address <= 0xFFFE) {
        return mem.hram[address - 0xFF80];
    
    } else if (address <= 0xFFFF) {
        return  mem.ie;
    }
}

void writeMemory(Memory& mem, uint16_t address, uint8_t value) {
    if (address <= 0x7FFF) {
        mem.rom[address] = value;

    } else if (address <= 0x9FFF) {
        mem.vram[address - 0x8000] = value;

    } else if (address <= 0xBFFF) {
        mem.extram[address - 0xA000] = value;

    } else if (address <= 0xDFFF) {
        mem.ram[address - 0xC000] = value;

    } else if (address <= 0xFE9F) {
        mem.oam[address - 0xFE00] = value;

    } else if (address <= 0xFF7F) {
        mem.io[address - 0xFF00] = value;

    } else if (address <= 0xFFFE) {
        mem.hram[address - 0xFF80] = value;

    } else if (address == 0xFFFF) {
        mem.ie = value;
    }
}


void executeNOP(Registers& reg) {
    reg.PC += 1;
}

void executeLD_B_n(Registers& reg, Memory& mem) {
    uint8_t value = readMemory(mem, reg.PC+1);
    reg.B = value;
    reg.PC += 2;
}


//LD BC, n16
void execute0x01(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC + 1);
    uint8_t hi = readMemory(mem, reg.PC + 2);

    reg.B = hi;
    reg.C = lo;
    
    reg.PC += 3;
}

void execute0x02(Registers& reg, Memory& mem) {
    uint8_t hi = reg.B;
    uint8_t lo = reg.C;

    uint16_t bc = (hi << 8) | lo;
    
    writeMemory(mem, bc, reg.A);
    reg.PC += 1;
}


void execute0x06(Registers& reg, Memory& mem) {
    uint8_t hi = mem.data[reg.PC+1];
    reg.B = hi;

    reg.PC +=2;
}

//LD A, [BC]
void execute0x0A(Registers& reg, Memory& mem) {
    uint16_t bc = (reg.B << 8) | reg.C;
    reg.A = mem.data[bc];

    reg.PC +=1;
}

void execute0x0E(Registers& reg, Memory& mem) {
    uint8_t hi = mem.data[reg.PC+1];
    reg.C = hi;
 
    reg.PC += 2;
}

// LD DE, n16
void execute0x11(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC+1];
    uint8_t hi = mem.data[reg.PC+2];

    reg.D = hi;
    reg.E = lo;

    reg.PC += 3;
}

// LD [DE], A
void execute0x12(Registers& reg, Memory& mem) {
    uint8_t hi = reg.D;
    uint8_t lo = reg.E;

    uint16_t de = (hi << 8) | lo;

    mem.data[de] = reg.A;
    reg.PC += 1;
}

//LD D, n8
void execute0x16(Registers& reg, Memory& mem) {
    uint8_t value = mem.data[reg.PC+1];
    reg.D = value;

    reg.PC += 2;
}

//LD A, [DE]
void execute0x1A(Registers& reg, Memory& mem) {
    uint16_t de = (reg.D << 8) | reg.E;
    reg.A = mem.data[de];

    reg.PC +=1;
}

//LD E, n8
void execute0x1E(Registers& reg, Memory& mem) {
    uint8_t value = mem.data[reg.PC +1];

    reg.E = value;
    reg.PC += 2;
}

//LD HL, n16
void execute0x21(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC+1];
    uint8_t hi = mem.data[reg.PC+2];

    reg.H = hi;
    reg.L = lo;

    reg.PC += 3;
}

//LD [HL+], A
void execute0x22(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    
    mem.data[hl] = reg.A;
    hl +=1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC +=1;
}

//LD H, n8
void execute0x26(Registers& reg, Memory& mem) {
    uint8_t value = mem.data[reg.PC+1];

    reg.H = value;

    reg.PC += 2;
}

//LD A, [HL+]
void execute0x2A(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;

    reg.A = mem.data[hl];
    hl +=1;
    
    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}

//LD L, n8
void execute0x2E(Registers& reg, Memory& mem) {
    uint8_t value = mem.data[reg.PC+1];

    reg.L = value;
    reg.PC += 2;
}

//LD [HL-], A
void execute0x32(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.A;

    hl -= 1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}

//LD [HL], n8
void execute0x36(Registers& reg, Memory& mem) {
    uint8_t value = mem.data[reg.PC + 1];
    uint16_t ld = (reg.H << 8) | reg.L;

    mem.data[ld] = value;

    reg.PC += 2;
}

//LD A, [HL-]
void execute0x3A(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.A = mem.data[hl];

    hl -= 1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}


//LD A, n8
void execute0x3E(Registers& reg, Memory& mem){
    uint8_t value = mem.data[reg.PC+1];

    reg.A = value;

    reg.PC += 2;
}


//LD B, B NOP同様に意味のない命令だが実装は必須
void execute0x40(Registers& reg, Memory& mem) {
    reg.B = reg.B;
    reg.PC += 1;
}

//LD B, C
void execute0x41(Registers& reg, Memory& mem) {
    reg.B = reg.C;
    reg.PC += 1;
}

//LD B, D
void execute0x42(Registers& reg, Memory& mem) {
    reg.B = reg.D;
    reg.PC += 1;
}

//LD B, E
void execute0x43(Registers& reg, Memory& mem) {
    reg.B = reg.E;
    reg.PC += 1;
}

//LD B, H
void execute0x44(Registers& reg, Memory& mem) {
    reg.B = reg.H;
    reg.PC += 1;
}

//LD B, L
void execute0x45(Registers& reg, Memory& mem) {
    reg.B = reg.L;
    reg.PC += 1;

}

//LD B, [HL]
void execute0x46(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.B = mem.data[hl];

    reg.PC += 1;
}

//LD B, A
void execute0x47(Registers& reg, Memory& mem) {
    reg.B = reg.A;
    reg.PC += 1;
}

//LD C, B
void execute0x48(Registers& reg, Memory& mem) {
    reg.C = reg.B;
    reg.PC += 1;
}

//LD C, C
void execute0x49(Registers& reg, Memory& mem) {
    reg.C = reg.C;
    reg.PC += 1;
}

//LD C, D
void execute0x4A(Registers& reg, Memory& mem) {
    reg.C = reg.D;
    reg.PC += 1;
}

// LD C, E
void execute0x4B(Registers& reg, Memory& mem)  {
    reg.C = reg.E;
    reg.PC += 1;
}

//LD C, H
void execute0x4C(Registers& reg, Memory& mem) {
    reg.C = reg.H;
    reg.PC += 1;
}

//LD C, L
void execute0x4D(Registers& reg, Memory& mem) {
    reg.C = reg.L;
    reg.PC += 1;
}

//LD C, [HL]
void execute0x4E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.C = mem.data[hl];

    reg.PC += 1;
}

//LD C, A
void execute0x4F(Registers& reg, Memory& mem) {
    reg.C = reg.A;
    
    reg.PC += 1;
}

//LD D, B
void execute0x50(Registers& reg, Memory& mem) {
    reg.D = reg.B;
    reg.PC += 1;
}

//LD D, C
void execute0x51(Registers& reg, Memory& mem) {
    reg.D = reg.C;
    reg.PC += 1;
}

//LD D, D
void execute0x52(Registers& reg, Memory& mem) {
    reg.D = reg.D;
    reg.PC += 1;
}

//LD D, E
void execute0x53(Registers& reg, Memory& mem) {
    reg.D = reg.E;
    reg.PC += 1;
}

//LD D, H
void execute0x54(Registers& reg, Memory& mem) {
    reg.D = reg.H;
    reg.PC += 1;
}

//LD D, L
void execute0x55(Registers& reg, Memory& mem) {
    reg.D = reg.L;
    reg.PC += 1;
}

//LD D, [HL]
void execute0x56(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.D = mem.data[hl];

    reg.PC += 1;
}

// LD D, A
void execute0x57(Registers& reg, Memory& mem) {
    reg.D = reg.A;

    reg.PC += 1;
}

// LD E, B
void execute0x58(Registers& reg, Memory& mem) {
    reg.E = reg.B;

    reg.PC += 1;
}

// LD E, C
void execute0x59(Registers& reg, Memory& mem) {
    reg.E = reg.C;

    reg.PC += 1;
}


// LD E, D
void execute0x5A(Registers& reg, Memory& mem) {
    reg.E = reg.D;

    reg.PC += 1;
}


// LD E, E
void execute0x5B(Registers& reg, Memory& mem) {
    reg.E = reg.E;

    reg.PC += 1;
}

// LD E, H
void execute0x5C(Registers& reg, Memory& mem) {
    reg.E = reg.H;

    reg.PC += 1;
}

//LD E, L
void execute0x5D(Registers& reg, Memory& mem) {
    reg.E = reg.L;

    reg.PC += 1;
}

// LD E, [HL]
void execute0x5E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.E = mem.data[hl];

    reg.PC += 1;
}

// LD E, A
void execute0x5F(Registers& reg, Memory& mem) {
    reg.E = reg.A;

    reg.PC += 1;
}

// LD H, B
void execute0x60(Registers& reg, Memory& mem) {
    reg.H = reg.B;

    reg.PC += 1;
}

//LD H, C
void execute0x61(Registers& reg, Memory& mem) {
    reg.H = reg.C;

    reg.PC += 1;
}

//LD H, D
void execute0x62(Registers& reg, Memory& mem) {
    reg.H = reg.D;

    reg.PC += 1;
}

//LD H, E
void execute0x63(Registers& reg, Memory& mem) {
    reg.H = reg.E;

    reg.PC += 1;
}

//LD H, H
void execute0x64(Registers& reg, Memory& mem) {
    reg.H = reg.H;

    reg.PC += 1;
}

//LD H, L
void execute0x65(Registers& reg, Memory& mem) {
    reg.H = reg.L;

    reg.PC += 1;
}

//LD H, [HL]
void execute0x66(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.H = mem.data[hl];

    reg.PC += 1;
}

//LD H, A
void execute0x67(Registers& reg, Memory& mem) {
    reg.H = reg.A;

    reg.PC += 1;
}

//LD L, B
void execute0x68(Registers& reg, Memory& mem) {
    reg.L = reg.B;

    reg.PC += 1;
}

//LD L, C
void execute0x69(Registers& reg, Memory& mem) {
    reg.L = reg.C;

    reg.PC += 1;
}

//LD L, D
void execute0x6A(Registers& reg, Memory& mem) {
    reg.L = reg.D;

    reg.PC += 1;
}

//LD L, E
void execute0x6B(Registers& reg, Memory& mem) {
    reg.L = reg.E;

    reg.PC += 1;
}

//LD L, H
void execute0x6C(Registers& reg, Memory& mem) {
    reg.L = reg.H;

    reg.PC += 1;
}

//LD L, L
void execute0x6D(Registers& reg, Memory& mem) {
    reg.L = reg.L;

    reg.PC += 1;
}

//LD L, [HL]
void execute0x6E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H  << 8) | reg.L;
    reg.L = mem.data[hl];

    reg.PC += 1;
}

//LD L, A
void execute0x6F(Registers& reg, Memory& mem) {
    reg.L = reg.A;
    reg.PC += 1;
}

//LD [HL], B
void execute0x70(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.B;
    reg.PC += 1;
}

//LD [HL], C
void execute0x71(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.C;
    reg.PC += 1;
}

//LD [HL], D
void execute0x72(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.D;
    reg.PC += 1;
}

//LD [HL], E
void execute0x73(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.E;
    reg.PC += 1;
}

//LD [HL], H
void execute0x74(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.H;
    reg.PC += 1;
}

//LD [HL], L
void execute0x75(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.L;
    reg.PC += 1;
}

//LD [HL], L
void execute0x76(Registers& reg) {
    reg.halted = true;
    reg.PC += 1;
}

//LD [HL], A
void execute0x77(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] = reg.A;
    reg.PC += 1;
}

//LD A, B
void execute0x78(Registers& reg, Memory& mem) {
    reg.A = reg.B;
    reg.PC += 1;
}

//LD A, C
void execute0x79(Registers& reg, Memory& mem) {
    reg.A = reg.C;
    reg.PC += 1;
}

//LD A, D
void execute0x7A(Registers& reg, Memory& mem) {
    reg.A = reg.D;
    reg.PC += 1;
}

//LD A, E
void execute0x7B(Registers& reg, Memory& mem) {
    reg.A = reg.E;  
    reg.PC += 1;
}

//LD A, H
void execute0x7C(Registers& reg, Memory& mem) {
    reg.A = reg.H;
    reg.PC += 1;
}

//LD A, L
void execute0x7D(Registers& reg, Memory& mem) {
    reg.A = reg.L;
    reg.PC += 1;
}

//LD A, [HL]
void execute0x7E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.A = mem.data[hl];
    reg.PC += 1;
}


void execute0x7F(Registers& reg, Memory& mem) {
    reg.A = reg.A;
    reg.PC += 1;
}

void execute0x80(Registers& reg) {
    uint16_t result = reg.A + reg.B;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.B & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

void execute0x81(Registers& reg) {
    uint16_t result = reg.A + reg.C;
    
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.C & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// ADD A,D
void execute0x82(Registers& reg) {
    uint16_t result = reg.A + reg.D;
    
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.D & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADD A,E
void execute0x83(Registers& reg) {
    uint16_t result = reg.A + reg.E;
    
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.E & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADD A,H
void execute0x84(Registers& reg) {
    uint16_t result = reg.A + reg.H;
    
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.H & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADD A,L
void execute0x85(Registers& reg) {
    uint16_t result = reg.A + reg.L;
    
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.L & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADD A,(HL)
void execute0x86(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t result = reg.A + mem.data[hl];

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (mem.data[hl] & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADD A,A
void execute0x87(Registers& reg) {
    uint16_t result = reg.A + reg.A;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.A & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,B
void execute0x88(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.B + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.B & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,C
void execute0x89(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.C + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.C & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,D
void execute0x8A(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.D + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.D & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,E
void execute0x8B(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.E + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.E & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,H
void execute0x8C(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.H + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.H & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,L
void execute0x8D(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.L + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.L & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,(HL)
void execute0x8E(Registers& reg, Memory& mem) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t hl = (reg.H << 8) | reg.L;

    uint16_t result = reg.A + mem.data[hl] + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (mem.data[hl] & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//ADC A,A
void execute0x8F(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A + reg.A + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.A & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 1;
}


//SUB B 
void execute0x90(Registers& reg) {
    uint16_t result = reg.A - reg.B;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.B & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.B);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB C
void execute0x91(Registers& reg) {
    uint16_t result = reg.A - reg.C;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.C & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.C);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB D
void execute0x92(Registers& reg) {
    uint16_t result = reg.A - reg.D;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.D & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.D);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB E
void execute0x93(Registers& reg) {
    uint16_t result = reg.A - reg.E;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.E & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.E);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB H
void execute0x94(Registers& reg) {
    uint16_t result = reg.A - reg.H;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.H & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.H);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB L
void execute0x95(Registers& reg) {
    uint16_t result = reg.A - reg.L;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.L & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.L);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//SUB (HL)
void execute0x96(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t result = reg.A - mem.data[hl];

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (mem.data[hl] & 0x0F));
    setFlag(reg, FLAG_C, reg.A < mem.data[hl]);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SUB A
void execute0x97(Registers& reg) {
    uint16_t result = reg.A - reg.A;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.A & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.A);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,B
void execute0x98(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.B - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.B & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.B + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,C
void execute0x99(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.C - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.C & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.C + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,D
void execute0x9A(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.D - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.D & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.D + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,E
void execute0x9B(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.E - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.E & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.E + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,H
void execute0x9C(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.H - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.H & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.H + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,L
void execute0x9D(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.L - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.L & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.L + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

// SBC A,(HL)
void execute0x9E(Registers& reg, Memory& mem) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;

    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t result = reg.A - mem.data[hl] - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (mem.data[hl]  & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < mem.data[hl]  + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}


// SBC A,A
void execute0x9F(Registers& reg) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint16_t result = reg.A - reg.A - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (reg.A  & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < reg.A  + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//AND A, B
void execute0xA0(Registers& reg) {
    uint8_t result = reg.A & reg.B;


    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// AND C
void execute0xA1(Registers& reg) {
    uint8_t result = reg.A & reg.C;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND D
void execute0xA2(Registers& reg) {
    uint8_t result = reg.A & reg.D;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND E
void execute0xA3(Registers& reg) {
    uint8_t result = reg.A & reg.E;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND H
void execute0xA4(Registers& reg) {
    uint8_t result = reg.A & reg.H;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND L
void execute0xA5(Registers& reg) {
    uint8_t result = reg.A & reg.L;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND (HL)
void execute0xA6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t result = reg.A & mem.data[hl];

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//AND A
void execute0xA7(Registers& reg) {
    uint8_t result = reg.A & reg.A;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR B
void execute0xA8(Registers& reg) {
    uint8_t result = reg.A ^ reg.B;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}


// XOR C
void execute0xA9(Registers& reg) {
    uint8_t result = reg.A ^ reg.C;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR D
void execute0xAA(Registers& reg) {
    uint8_t result = reg.A ^ reg.D;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR E
void execute0xAB(Registers& reg) {
    uint8_t result = reg.A ^ reg.E;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR H
void execute0xAC(Registers& reg) {
    uint8_t result = reg.A ^ reg.H;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR L
void execute0xAD(Registers& reg) {
    uint8_t result = reg.A ^ reg.L;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR (HL)
void execute0xAE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t result = reg.A ^ mem.data[hl];

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// XOR A
void execute0xAF(Registers& reg) {
    uint8_t result = reg.A ^ reg.A;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR B
void execute0xB0(Registers& reg) {
    uint8_t result = reg.A | reg.B;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR C
void execute0xB1(Registers& reg) {
    uint8_t result = reg.A | reg.C;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR D
void execute0xB2(Registers& reg) {
    uint8_t result = reg.A | reg.D;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR E
void execute0xB3(Registers& reg) {
    uint8_t result = reg.A | reg.E;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR H
void execute0xB4(Registers& reg) {
    uint8_t result = reg.A | reg.H;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR L
void execute0xB5(Registers& reg) {
    uint8_t result = reg.A | reg.L;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR (HL)
void execute0xB6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t result = reg.A | mem.data[hl];

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

//OR A
void execute0xB7(Registers& reg) {
    uint8_t result = reg.A | reg.A;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 1;
}

// CP B
void execute0xB8(Registers& reg) {
    uint8_t result = reg.A - reg.B;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.B & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.B);

    reg.PC += 1;
}


// CP C
void execute0xB9(Registers& reg) {
    uint8_t result = reg.A - reg.C;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.C & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.C);

    reg.PC += 1;
}

// CP D
void execute0xBA(Registers& reg) {
    uint8_t result = reg.A - reg.D;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.D & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.D);

    reg.PC += 1;
}

// CP E
void execute0xBB(Registers& reg) {
    uint8_t result = reg.A - reg.E;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.E & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.E);

    reg.PC += 1;
}

// CP H
void execute0xBC(Registers& reg) {
    uint8_t result = reg.A - reg.H;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.H & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.H);

    reg.PC += 1;
}

// CP L
void execute0xBD(Registers& reg) {
    uint8_t result = reg.A - reg.L;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.L & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.L);

    reg.PC += 1;
}

// CP (HL)
void execute0xBE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t result = reg.A - mem.data[hl];

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (mem.data[hl] & 0x0F));
    setFlag(reg, FLAG_C, reg.A < mem.data[hl]);

    reg.PC += 1;
}

// CP A
void execute0xBF(Registers& reg) {
    uint8_t result = reg.A - reg.A;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (reg.A & 0x0F));
    setFlag(reg, FLAG_C, reg.A < reg.A);

    reg.PC += 1;
}

// RET NZ
void execute0xC0(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.SP];
        uint8_t hi = mem.data[reg.SP + 1];
        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC += 1;
    }
}

//POP BC
void execute0xC1(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP+1];

    reg.C = lo;
    reg.B = hi;

    reg.SP += 2;
    reg.PC += 1;
}

// JP NZ, a16
void execute0xC2(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];
        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// JP a16
void execute0xC3(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC + 1];
    uint8_t hi = mem.data[reg.PC + 2];
    reg.PC = (hi << 8) | lo;
}


// CALL NZ, a16
void execute0xC4(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];

        uint16_t next = reg.PC + 3;
        mem.data[reg.SP - 1] = (next >> 8);
        mem.data[reg.SP - 2] = (next & 0xFF);
        reg.SP -= 2;

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}


// PUSH BC
void execute0xC5(Registers& reg, Memory& mem) {
    reg.SP -=1;
    mem.data[reg.SP] = reg.B;
    reg.SP -=1;
    mem.data[reg.SP] = reg.C;

    reg.PC += 1;
}


//ADD A,d8
void execute0xC6(Registers& reg, Memory& mem) {
    uint16_t result = reg.A + mem.data[reg.PC+1];

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (mem.data[reg.PC+1] & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);
    reg.A = result & 0xFF;
    reg.PC += 2;
}


//RST 00H
void execute0xC7(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    mem.data[reg.SP - 1] = (next >> 8) & 0xFF;
    mem.data[reg.SP - 2] = next & 0xFF;
    reg.SP -= 2;

    reg.PC = 0x0000;
}

//RET Z
void execute0xC8(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.SP];
        uint8_t hi = mem.data[reg.SP + 1];
        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC += 1;
    }
}

// RET
void execute0xC9(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP + 1];
    reg.PC = (hi << 8) | lo;
    reg.SP += 2;
}

// JP Z, a16
void execute0xCA(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];
        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// PREFIX CB
void execute0xCB(Registers& reg, Memory& mem) {
    reg.PC += 1;
    uint8_t opcode = mem.data[reg.PC];
    // CB命令のswitchに渡す処理をここに追加する
    executeCB(reg, mem, opcode);
}

// CALL NZ, a16
void execute0xCC(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];

        uint16_t next = reg.PC + 3;
        mem.data[reg.SP - 1] = (next >> 8);
        mem.data[reg.SP - 2] = (next & 0xFF);
        reg.SP -= 2;

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// CALL Z, a16
void execute0xCC(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];

        uint16_t next = reg.PC + 3;
        mem.data[reg.SP - 1] = (next >> 8);
        mem.data[reg.SP - 2] = (next & 0xFF);
        reg.SP -= 2;

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// CALL a16
void execute0xCD(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC + 1];
    uint8_t hi = mem.data[reg.PC + 2];

    uint16_t next = reg.PC + 3;
    mem.data[reg.SP - 1] = (next >> 8);
    mem.data[reg.SP - 2] = (next & 0xFF);
    reg.SP -= 2;

    reg.PC = (hi << 8) | lo;
}

// ADC A,d8
void execute0xCE(Registers& reg, Memory& mem) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint8_t d8 = mem.data[reg.PC+1];
    uint16_t result = reg.A + d8 + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (d8 & 0x0F) + carry) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.A = result & 0xFF;
    reg.PC += 2;
}

//RST 08H
void execute0xCF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    mem.data[reg.SP - 1] = (reg.PC >> 8) & 0xFF; // 上位バイト
    mem.data[reg.SP - 2] = reg.PC & 0xFF;         // 下位バイト
    reg.SP -= 2;

    reg.PC = 0x0008;

}

//RET NC
void execute0xD0(Registers& reg, Memory& mem) {
    if(!getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.SP];
        uint8_t hi = mem.data[reg.SP+1];
        
        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC += 1;
    }
}

//POP DE
void execute0xD1(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP+1];

    reg.E = lo;
    reg.D = hi;

    reg.SP += 2;
    reg.PC += 1;
}

//JP NC,a16
void execute0xD2(Registers& reg, Memory& mem) {
    if(!getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.PC + 1];
        uint8_t hi = mem.data[reg.PC + 2];

        reg.PC = (hi << 8) | lo;
    } else {
    reg.PC += 3;
    }
}


//CALL NC,a16
void execute0xD4(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.PC+1];
        uint8_t hi = mem.data[reg.PC+2];

        uint16_t next = reg.PC + 3;

        reg.PC = (hi << 8) | lo;
        reg.SP -= 2;

        mem.data[reg.SP - 1] = (next >> 8);
        mem.data[reg.SP - 2] = (next& 0xFF);
    }else {
        reg.PC +=3;
    }
}


//PUSH DE
void execute0xD5(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    mem.data[reg.SP] = reg.D;
    reg.SP -= 1;
    mem.data[reg.SP] = reg.E;

    reg.PC += 1;
}


//SUB d8
void execute0xD6(Registers& reg, Memory& mem) {
    uint8_t d8 = mem.data[reg.PC+1];
    uint16_t result = reg.A - d8;


    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (d8 & 0x0F));
    setFlag(reg, FLAG_C, reg.A < d8);

    reg.A = result & 0xFF;
    reg.PC += 2;
}

//RST 10H
void execute0xD7(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    mem.data[reg.SP -1] = (next >> 8) & 0xFF;
    mem.data[reg.SP -2] = (next & 0xFF);

    reg.SP -=2;
    reg.PC = 0x0010;
}

//RET C
void execute0xD8(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.SP];
        uint8_t hi = mem.data[reg.SP+1];

        reg.PC = (hi << 8) | lo;
        reg.SP -= 2;
    } else {
        reg.PC +=1;
    }   
}

// RETI
void execute0xD9(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP + 1];
    reg.PC = (hi << 8) | lo;
    reg.SP += 2;

    reg.IME = true;  // 割り込みフラグを有効に戻す
}

//JP C,a16
void execute0xDA(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.PC+1];
        uint8_t hi = mem.data[reg.PC+2];

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}


//CALL C,a16
void execute0xDC(Registers& reg, Memory& mem) {
    if(getFlag(reg, FLAG_C)) {
        uint8_t lo = mem.data[reg.PC +1];
        uint8_t hi = mem.data[reg.PC +2];

        uint16_t next = reg.PC +3;
        mem.data[reg.SP-1] = next >> 8;
        mem.data[reg.SP-2] = next& 0xFF;
        reg.SP -= 2;
        reg.PC = (hi << 8) | lo;

    } else {    
        reg.PC += 3;
    }
}

//SBC A,d8
void execute0xDE(Registers& reg, Memory& mem) {
    uint8_t d8 = mem.data[reg.PC];
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;

    uint16_t result = reg.A - d8 - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (d8 & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < d8 + carry);

    reg.A = result & 0xFF;
    reg.PC += 1;
}

//RST 18H
void execute0xDF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC+1;
    
    mem.data[reg.SP -1] = (next >> 8) & 0xFF;
    mem.data[reg.SP -2] = next & 0xFF;
    reg.SP -= 2;

    reg.PC = 0x0012;
}


//LDH [a8], A
void execute0xE0(Registers& reg, Memory& mem) {
    uint8_t offset = mem.data[reg.PC + 1];
    uint16_t address = 0xFF00 + offset;
    mem.data[address] = reg.A;
    reg.PC += 2;
}

//POP HL
void execute0xE1(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP+1];

    reg.L = lo;
    reg.H = hi;

    reg.SP += 2;
    reg.PC += 1;
}

//LDH [C], A
void execute0xE2(Registers& reg, Memory& mem) {
    uint16_t address = 0xFF00 + reg.C;
    mem.data[address] = reg.A;
    reg.PC += 1;
}

//PUSH HL
void execute0xE2(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    mem.data[reg.SP] = reg.H;
    reg.SP -= 1;
    mem.data[reg.SP] = reg.L;
    reg.PC += 1;
}
// AND A, n8
void execute0xE6(Registers& reg, Memory& mem) {
    uint8_t n8 = mem.data[reg.PC + 1];
    uint8_t result = reg.A & n8;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 2;
}


// RST 20H
void execute0xE7(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;

    mem.data[reg.SP - 1] = (next >> 8) & 0xFF;
    mem.data[reg.SP - 2] = next & 0xFF;
    reg.SP -= 2;

    reg.PC = 0x0020;
}


//ADD SP,r8
void execute0xE8(Registers& reg, Memory& mem) {
    uint16_t result = reg.SP + mem.data[reg.PC + 1];

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.SP & 0x0F) + (mem.data[reg.PC] & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);

    reg.SP = result & 0xFF;
    reg.PC += 2;
}

void execute0xE9(Registers& reg) {
    reg.PC = (reg.H << 8) | reg.L;
}


//LD (a16),A
void execute0xEA(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC];
    uint8_t hi = mem.data[reg.PC+1];

    uint16_t bc = (hi << 8) | lo;

    mem.data[bc] = reg.A;
    reg.PC +=3;
}

//XOR d8
void execute0xEE(Registers& reg, Memory& mem) {
        uint8_t result = reg.A ^ mem.data[reg.PC];

        setFlag(reg, FLAG_Z, result == 0);
        setFlag(reg, FLAG_N, false);
        setFlag(reg, FLAG_H, false);
        setFlag(reg, FLAG_C, false);

        reg.A = result;
        reg.PC +=2;
}


//RST 28H
void execute0xEF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    mem.data[reg.SP -1] = (next >> 8) & 0xFF;
    mem.data[reg.SP -2] = next & 0xFF;
    reg.SP -= 2;

    reg.PC = 0x0028;
}


//LDH A,(a8)
void execute0xF0(Registers& reg, Memory& mem) {
    uint8_t offset = mem.data[reg.PC + 1];
    uint16_t address = 0xFF00 + offset;
    reg.A = mem.data[address];
    reg.PC += 2;
}

//POP AF
void execute0xF1(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.SP];
    uint8_t hi = mem.data[reg.SP+1];

    reg.F = lo;
    reg.A = hi;

    reg.PC +=1;
    reg.SP +=2;
}

//LD A,(C)
void execute0xF2(Registers& reg, Memory& mem) {
    uint16_t address = 0xFF00 + reg.C;
    reg.A = mem.data[address];
}


//PUSH AF
void execute0xF5(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    mem.data[reg.SP] = reg.A;
    reg.SP -= 1;
    mem.data[reg.SP] = reg.F;

    reg.PC +=1;
}

//OR d8
void execute0xF6(Registers& reg, Memory& mem) {
    uint8_t d8 = mem.data[reg.PC+1];
    uint8_t result = reg.A | d8;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.A = result;
    reg.PC += 2;
}


//RST 30H
void execute0xF7(Registers& reg, Memory& mem){
    uint16_t next = reg.PC +1;

    mem.data[reg.SP -1] = (next >> 8) & 0xFF;
    mem.data[reg.SP -2] = next & 0xEF;

    reg.SP -=2;
    reg.PC = 0x0030;
}

// LD HL, SP+r8
void execute0xF8(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)mem.data[reg.PC + 1];
    uint16_t result = reg.SP + r8;

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.SP & 0x0F) + (r8 & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, ((reg.SP & 0xFF) + (r8 & 0xFF)) > 0xFF);

    reg.H = result >> 8;
    reg.L = result & 0xFF;
    reg.PC += 2;
}

//LD SP,HL
void execute0xF9(Registers& reg) {
    reg.SP = (reg.H << 8) | reg.L;
    reg.PC += 1;
}

// LD A, (a16)
void execute0xFA(Registers& reg, Memory& mem) {
    uint8_t lo = mem.data[reg.PC + 1];
    uint8_t hi = mem.data[reg.PC + 2];
    uint16_t address = (hi << 8) | lo;

    reg.A = mem.data[address];
    reg.PC += 3;
}

// EI
void execute0xFB(Registers& reg) {
    reg.IME = true;
    reg.PC += 1;
}

//CP d8
void execute0xFE(Registers& reg, Memory& mem) {
    uint8_t d8 = mem.data[reg.PC+1];
    uint8_t result = reg.A - d8;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (d8 & 0x0F));
    setFlag(reg, FLAG_C, reg.A < d8);

    reg.PC += 2;
}

//RST 38H
void execute0xFF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    mem.data[reg.SP - 1] = (next >> 8) & 0xFF;
    mem.data[reg.SP - 2] = next & 0xFF;
    reg.SP -= 2;

    reg.PC = 0x0038;
}


///プレフィックスコード


//     set命令
//SET 0,B
void executeCB0xC0(Registers& reg) {
    reg.B |= 0x01;
    reg.PC += 2;
}


//SET 0,C
void executeCB0xC1(Registers& reg) {
    reg.C |= 0x01;
    reg.PC += 2;
}

//SET 0,D
void executeCB0xC2(Registers& reg) {
    reg.D |= 0x01;
    reg.PC += 2;
}

// SET 0,E
void executeCB0xC3(Registers& reg) {
    reg.E |= 0x01;
    reg.PC += 2;
}

//SET 0,H
void executeCB0xC4(Registers& reg) {
    reg.H |= 0x01;
    reg.PC += 2;
}

//SET 0,L
void executeCB0xC5(Registers& reg) {
    reg.L |= 0x01;
    reg.PC += 2;
}

////SET 0,(HL)
void executeCB0xC6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] |= 0x01;
    reg.PC += 2;
}

//SET 0,A
void executeCB0xC7(Registers& reg) {
    reg.A |= 0x01;
    reg.PC += 2;
}

//SET 1,B
void executeCB0xC8(Registers& reg) {
    reg.B |= 0x02;
    reg.PC += 2;
}

//SET 1,C
void executeCB0xC9(Registers& reg) {
    reg.C |= 0x02;
    reg.PC += 2;
}

//SET 1,D
void executeCB0xCA(Registers& reg) {
    reg.D |= 0x02;
    reg.PC += 2;
}

//SET 1,E
void executeCB0xCB(Registers& reg) {
    reg.E |= 0x02;
    reg.PC += 2;
}

//SET 1,H
void executeCB0xCC(Registers& reg) {
    reg.H |= 0x02;
    reg.PC += 2;
}

//SET 1,L
void executeCB0xCD(Registers& reg) {
    reg.L |= 0x02;
    reg.PC += 2;
}

//SET 1,(HL)
void executeCB0xCE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    mem.data[hl] |= 0x02;
    reg.PC += 2;
}

//SET 1,A
void executeCB0xCF(Registers& reg) {
    reg.A |= 0x02;
    reg.PC += 2;
}

//SET 2,B
void executeCB0xD0(Registers& reg) {

}







void step(Registers& reg, Memory& mem) {
    uint8_t opcode = mem.data[reg.PC];

    switch (opcode) {
        case 0x00:
            executeNOP(reg);
            break;

        case 0x01:
            execute0x01(reg, mem);
            break;

        case 0x02:
            execute0x02(reg, mem);
            break;

        case 0x04:  // INC B
        {
            uint8_t prev = reg.B;
            reg.B += 1;
            // Zフラグ: 結果が0なら1
            if (reg.B == 0) reg.F |= 0x80; else reg.F &= ~0x80;
            // Nフラグ: 0にリセット
            reg.F &= ~0x40;
            // Hフラグ: 下位4ビットがオーバーフローした場合に1
            if ((prev & 0x0F) == 0x0F) reg.F |= 0x20; else reg.F &= ~0x20;
            reg.PC += 1;
            break;
        }

        case 0x06:
            executeLD_B_n(reg, mem);
            break;

        case 0x0A:
            execute0x0A(reg, mem);
            break;

        case 0x0E:
            execute0x0E(reg, mem);
            break;

        case 0x11:
            execute0x11(reg, mem);
            break;

        case 0x12:
            execute0x12(reg, mem);
            break;

        case 0x16:
            execute0x16(reg, mem);
            break;

        case 0x1A:
            execute0x1A(reg, mem);
            break;

        case 0x1E:
            execute0x1E(reg, mem); 
            break;
        
        case 0x22:
            execute0x22(reg, mem);
            break;

        case  0x2A:
            execute0x2A(reg, mem);
            break;

        case 0x2E:
            execute0x2E(reg, mem);
            break;

        case 0x32:
            execute0x32(reg, mem);
            break;

        case 0x36:
            execute0x36(reg, mem);
            break;

        case 0x3A:
            execute0x3A(reg, mem);
            break;
        
        case 0x3E:
            execute0x3E(reg, mem);
            break;
        
        case 0x40:
            execute0x40(reg, mem);
            break;

        case 0x41:
            execute0x41(reg, mem);
            break;

        case 0x42:
            execute0x42(reg, mem);
            break;

        case 0x43:
            execute0x43(reg, mem);
            break;


        




        default:
            std::cout << "未実装の命令: 0x" << std::hex << (int)opcode << std::endl;
            reg.PC += 1;
            break;
    }
}

int main() {
    Registers reg;
    Memory mem;

    reg.PC = 0x0100;
    mem.data[0x0100] = 0x00;  // NOP
    mem.data[0x0101] = 0x06;  // LD B, n
    mem.data[0x0102] = 0x42;  // Bに入れる値
    mem.data[0x0103] = 0x00;  // NOP

    std::cout << "初期状態:" << std::endl;
    std::cout << "  PC = 0x" << std::hex << reg.PC << std::endl;
    std::cout << "  B  = 0x" << std::hex << (int)reg.B << std::endl;

    for (int i = 0; i < 3; i++) {
        step(reg, mem);
        std::cout << "ステップ" << std::dec << (i + 1) << ":" << std::endl;
        std::cout << "  PC = 0x" << std::hex << reg.PC << std::endl;
        std::cout << "  B  = 0x" << std::hex << (int)reg.B << std::endl;
    }

    return 0;
}