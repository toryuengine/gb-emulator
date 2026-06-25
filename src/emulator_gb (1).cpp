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

//LD [BC], A
void execute0x02(Registers& reg, Memory& mem) {
    uint8_t hi = reg.B;
    uint8_t lo = reg.C;

    uint16_t bc = (hi << 8) | lo;
    writeMemory(mem, bc , reg.A);
    reg.PC += 1;
}

//INC BC
void execute0x03(Registers& reg) {
    uint16_t bc = (reg.B << 8) | reg.C;
    bc += 1;
    reg.B = bc >> 8;
    reg.C = bc & 0xFF;
    reg.PC += 1;
}

//INC B
void execute0x04(Registers& reg) {
    uint8_t prev = reg.B;
    reg.B += 1;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC B
void execute0x05(Registers& reg) {
    uint8_t prev = reg.B;
    reg.B -= 1;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD B, n8
void execute0x06(Registers& reg, Memory& mem) {
    reg.B = readMemory(mem, reg.PC+1);
    reg.PC +=2;
}

//RLCA
void execute0x07(Registers& reg) {
    uint8_t bit7 = (reg.A >> 7) & 1;
    reg.A = (reg.A << 1) | bit7;

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 1;
}

// LD [a16], SP
void execute0x08(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC + 1);
    uint8_t hi = readMemory(mem, reg.PC + 2);
    uint16_t address = (hi << 8) | lo;

    writeMemory(mem, address, reg.SP & 0xFF);
    writeMemory(mem, address + 1, reg.SP >> 8);

    reg.PC += 3;
}

//ADD HL, BC
void execute0x09(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t bc = (reg.B << 8) | reg.C;
    uint32_t result = hl + bc;

    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((hl & 0x0FFF) + (bc & 0x0FFF)) > 0x0FFF);
    setFlag(reg, FLAG_C, result > 0xFFFF);

    reg.H = (result >> 8) & 0xFF;
    reg.L = result & 0xFF;
    reg.PC += 1;
}

//LD A, [BC]
void execute0x0A(Registers& reg, Memory& mem) {
    uint16_t bc = (reg.B << 8) | reg.C;
    reg.A = readMemory(mem, bc);
    reg.PC +=1;
}

//DEC BC
void execute0x0B(Registers& reg) {
    uint16_t bc = (reg.B << 8) | reg.C;
    bc -= 1;
    reg.B = bc >> 8;
    reg.C = bc & 0xFF;
    reg.PC += 1;
}

//INC C
void execute0x0C(Registers& reg) {
    uint8_t prev = reg.C;
    reg.C += 1;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC C
void execute0x0D(Registers& reg) {
    uint8_t prev = reg.C;
    reg.C -= 1;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD C, n8
void execute0x0E(Registers& reg, Memory& mem) {
    reg.C = readMemory(mem, reg.PC+1);
    reg.PC += 2;
}

//RRCA
void execute0x0F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    reg.A = (reg.A >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 1;
}

// STOP
void execute0x10(Registers& reg) {
    reg.halted = true;
    reg.PC += 2;
}

// LD DE, n16
void execute0x11(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC+1);
    uint8_t hi = readMemory(mem, reg.PC+2);

    reg.D = hi;
    reg.E = lo;

    reg.PC += 3;
}

// LD [DE], A
void execute0x12(Registers& reg, Memory& mem) {
    uint8_t hi = reg.D;
    uint8_t lo = reg.E;

    uint16_t de = (hi << 8) | lo;

    writeMemory(mem, de, reg.A);
    reg.PC += 1;
}

//INC DE
void execute0x13(Registers& reg, Memory& mem) {
    uint16_t de = (reg.D << 8) | reg.E;
    de += 1;

    reg.D = (de >> 8);
    reg.E = de & 0xFF;

    reg.PC +=1;
}

//INC D
void execute0x14(Registers& reg) {
    uint8_t prev = reg.D;
    reg.D += 1;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC D
void execute0x15(Registers& reg) {
    uint8_t prev = reg.D;
    reg.D -= 1;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD D, n8
void execute0x16(Registers& reg, Memory& mem) {
    uint8_t value = readMemory(mem, reg.PC+1);
    reg.D = value;

    reg.PC += 2;
}

// RLA
void execute0x17(Registers& reg) {
    uint8_t bit7 = (reg.A >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.A = (reg.A << 1) | carry;

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 1;
}

//JR r8
void execute0x18(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC + 1);
    reg.PC += 2;
    reg.PC += r8;
}

//ADD HL,DE
void execute0x19(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t de = (reg.D << 8) | reg.E;
    uint32_t result = hl + de;

    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((hl & 0x0FFF) + (de & 0x0FFF)) > 0x0FFF);
    setFlag(reg, FLAG_C, result > 0xFFFF);

    reg.H = (result >> 8) & 0xFF;
    reg.L = result & 0xFF;
    reg.PC += 1;
}

//LD A, [DE]
void execute0x1A(Registers& reg, Memory& mem) {
    uint16_t de = (reg.D << 8) | reg.E;
    reg.A = readMemory(mem, de);

    reg.PC +=1;
}

//DEC DE
void execute0x1B(Registers& reg) {
    uint16_t de = (reg.D << 8) | reg.E;
    de -= 1;
    reg.D = de >> 8;
    reg.E = de & 0xFF;
    reg.PC += 1;
}


//INC E
void execute0x1C(Registers& reg, Memory& mem) {
    uint8_t prev = reg.E;
    reg.E += 1;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC E
void execute0x1D(Registers& reg) {
    uint8_t prev = reg.E;
    reg.E -= 1;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD E,d8
void execute0x1E(Registers& reg, Memory& mem) {
    reg.E = readMemory(mem, reg.PC+1);
    reg.PC += 2;
}

//RRA
void execute0x1F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.A = (reg.A >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 1;
}

//JR NZ,r8
void execute0x20(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC + 1);
    reg.PC += 2;
    if (!getFlag(reg, FLAG_Z)) {
        reg.PC += r8;
    }
}

//LD HL, n16
void execute0x21(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC+1);
    uint8_t hi = readMemory(mem, reg.PC+2);

    reg.H = hi;
    reg.L = lo;

    reg.PC += 3;
}

//LD [HL+], A
void execute0x22(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    
    writeMemory(mem, hl, reg.A);
    hl +=1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC +=1;
}

//INC HL
void execute0x23(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    hl += 1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC +=1;
}

//INC H
void execute0x24(Registers& reg) {
    uint8_t prev = reg.H;
    reg.H += 1;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC H
void execute0x25(Registers& reg) {
    uint8_t prev = reg.H;
    reg.H -= 1;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD H, n8
void execute0x26(Registers& reg, Memory& mem) {
    reg.H = readMemory(mem, reg.PC+1);
    reg.PC += 2;
}

//DAA
void execute0x27(Registers& reg) {
    uint8_t a = reg.A;
    bool n = getFlag(reg, FLAG_N);
    bool h = getFlag(reg, FLAG_H);
    bool c = getFlag(reg, FLAG_C);

    if (!n) {
        if (c || a > 0x99) { a += 0x60; setFlag(reg, FLAG_C, true); }
        if (h || (a & 0x0F) > 0x09) { a += 0x06; }
    } else {
        if (c) { a -= 0x60; }
        if (h) { a -= 0x06; }
    }

    reg.A = a;
    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_H, false);

    reg.PC += 1;
}

//JR Z,r8
void execute0x28(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC+1);
    reg.PC += 2;
    if (getFlag(reg, FLAG_Z)) {
        reg.PC += r8;
    }
}

//ADD HL,HL
void execute0x29(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint16_t hl2 = (reg.H << 8) | reg.L;
    uint32_t result = hl + hl2;

    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((hl & 0x0FFF) + (hl2 & 0x0FFF)) > 0x0FFF);
    setFlag(reg, FLAG_C, result > 0xFFFF);

    reg.H = (result >> 8) & 0xFF;
    reg.L = result & 0xFF;
    reg.PC += 1;
}

//LD A, [HL+]
void execute0x2A(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;

    reg.A = readMemory(mem, hl);
    hl +=1;
    
    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}

//DEC HL
void execute0x2B(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    hl -= 1;
    reg.H = hl >> 8;
    reg.L = hl & 0xFF;
    reg.PC += 1;
}

//INC L
void execute0x2C(Registers& reg) {
    uint8_t prev = reg.L;
    reg.L += 1;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC L
void execute0x2D(Registers& reg) {
    uint8_t prev = reg.L;
    reg.L -= 1;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD L, n8
void execute0x2E(Registers& reg, Memory& mem) {
    reg.L = readMemory(mem, reg.PC + 1);
    reg.PC += 2;
}

// CPL
void execute0x2F(Registers& reg) {
    reg.A = ~reg.A;

    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, true);

    reg.PC += 1;
}

//JR NC,r8
void execute0x30(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC + 1);
    reg.PC += 2;

    if (!getFlag(reg, FLAG_C)) {
        reg.PC += r8;
    }
}

//LD SP,d16
void execute0x31(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC+1);
    uint8_t hi = readMemory(mem, reg.PC+2);

    reg.SP = (hi << 8) | lo;
    reg.PC += 3;
}

//LD [HL-], A
void execute0x32(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.A);

    hl -= 1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}

//INC SP
void execute0x33(Registers& reg) {
    reg.SP += 1;//本来はマイナス演算だけどスタックポイントは逆
    reg.PC += 1;
}

//INC (HL)
void execute0x34(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t prev = readMemory(mem, hl);
    uint8_t value = prev + 1;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    writeMemory(mem, hl, value);
    reg.PC += 1;
}

// DEC (HL)
void execute0x35(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t prev = readMemory(mem, hl);
    uint8_t value = prev - 1;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    writeMemory(mem, hl, value);
    reg.PC += 1;
}

// LD [HL], n8
void execute0x36(Registers& reg, Memory& mem) {
    uint8_t value = readMemory(mem, reg.PC + 1);
    uint16_t hl = (reg.H << 8) | reg.L;

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

// SCF
void execute0x37(Registers& reg) {
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, true);

    reg.PC += 1;
}

// JR C,r8
void execute0x38(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC+1);
    reg.PC += 2;
    if (getFlag(reg, FLAG_C)) {
        reg.PC += r8;
    }
}

//ADD HL,SP
void execute0x39(Registers& reg) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint32_t result = hl + reg.SP;

    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((hl & 0x0FFF) + (reg.SP & 0x0FFF)) > 0x0FFF);
    setFlag(reg, FLAG_C, result > 0xFFFF);

    reg.H = (result >> 8) & 0xFF;
    reg.L = result & 0xFF;
    reg.PC += 1;
}


//LD A, [HL-]
void execute0x3A(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    reg.A = readMemory(mem, hl);

    hl -= 1;

    reg.H = (hl >> 8);
    reg.L = hl & 0xFF;

    reg.PC += 1;
}

// DEC SP
void execute0x3B(Registers& reg) {
    reg.SP -= 1;
    reg.PC += 1;
}

//INC A
void execute0x3C(Registers& reg) {
    uint8_t prev = reg.A;
    reg.A += 1;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x0F);

    reg.PC += 1;
}

//DEC A
void execute0x3D(Registers& reg) {
    uint8_t prev = reg.A;
    reg.A -= 1;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (prev & 0x0F) == 0x00);

    reg.PC += 1;
}

//LD A, n8
void execute0x3E(Registers& reg, Memory& mem){
    uint8_t value = readMemory(mem, reg.PC+1);

    reg.A = value;

    reg.PC += 2;
}

// CCF
void execute0x3F(Registers& reg) {
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, !getFlag(reg, FLAG_C));

    reg.PC += 1;
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
    reg.B = readMemory(mem, hl);

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
    reg.C = readMemory(mem, hl);

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
    reg.D = readMemory(mem, hl);

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
    reg.E = readMemory(mem, hl);

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
    reg.H = readMemory(mem, hl);

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
    reg.L = readMemory(mem, hl);

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
    writeMemory(mem, hl, reg.B);
    reg.PC += 1;
}

//LD [HL], C
void execute0x71(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.C);
    reg.PC += 1;
}

//LD [HL], D
void execute0x72(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.D);
    reg.PC += 1;
}

//LD [HL], E
void execute0x73(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.E);
    reg.PC += 1;
}

//LD [HL], H
void execute0x74(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.H);
    reg.PC += 1;
}

//LD [HL], L
void execute0x75(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    writeMemory(mem, hl, reg.L);
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
    writeMemory(mem, hl, reg.A);
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
    reg.A = readMemory(mem, hl);
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
    uint8_t value = readMemory(mem, hl);
    uint16_t result = reg.A + value;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (value & 0x0F)) > 0x0F);
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

    uint8_t value = readMemory(mem, hl);

    uint16_t result = reg.A + value + carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (value & 0x0F) + carry) > 0x0F);
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
    uint16_t result = reg.A - readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (readMemory(mem, hl) & 0x0F));
    setFlag(reg, FLAG_C, reg.A < readMemory(mem, hl));

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
    uint8_t value = readMemory(mem, hl);

    uint16_t result = reg.A - value - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (value  & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < value  + carry);

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
    uint8_t value = readMemory(mem, hl);
    uint8_t result = reg.A & value;

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
    uint8_t value = readMemory(mem, hl);
    uint8_t result = reg.A ^ value;

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
    uint8_t value = readMemory(mem, hl);
    uint8_t result = reg.A | value;

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
    uint8_t value = readMemory(mem, hl);
    uint8_t result = reg.A - value;

    setFlag(reg, FLAG_Z, result == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, (reg.A & 0x0F) < (value& 0x0F));
    setFlag(reg, FLAG_C, reg.A < value);

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
        uint8_t lo = readMemory(mem, reg.SP);
        uint8_t hi = readMemory(mem, reg.SP+1);
        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC += 1;
    }
}

//POP BC
void execute0xC1(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP+1);

    reg.C = lo;
    reg.B = hi;

    reg.SP += 2;
    reg.PC += 1;
}

// JP NZ, a16
void execute0xC2(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);
        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// JP a16
void execute0xC3(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC + 1);
    uint8_t hi = readMemory(mem, reg.PC + 2);
    reg.PC = (hi << 8) | lo;
}


// CALL NZ, a16
void execute0xC4(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_Z)) {
        uint8_t lo = readMemory(mem, reg.PC + 1);
        uint8_t hi = readMemory(mem, reg.PC + 2);

        uint16_t next = reg.PC + 3;

        reg.SP -=1;
        writeMemory(mem, reg.SP, (next >> 8));
        reg.SP -=1;
        writeMemory(mem, reg.SP, (next & 0xFF));
        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}


// PUSH BC
void execute0xC5(Registers& reg, Memory& mem) {
    reg.SP -=1;
    writeMemory(mem, reg.SP, reg.B);
    reg.SP -=1;
    writeMemory(mem, reg.SP, reg.C);
    reg.PC += 1;
}


//ADD A,d8
void execute0xC6(Registers& reg, Memory& mem) {
    uint8_t value = readMemory(mem, reg.PC+1);
    uint16_t result = reg.A + value;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (value & 0x0F)) > 0x0F);
    setFlag(reg, FLAG_C, result > 0xFF);
    reg.A = result & 0xFF;
    reg.PC += 2;
}


//RST 00H
void execute0xC7(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;

    reg.SP -= 1;
    writeMemory(mem, reg.SP, (next >> 8) & 0xFF);
    reg.SP -= 1;
    writeMemory(mem, reg.SP, next & 0xFF);
    reg.PC = 0x0000;
}

//RET Z
void execute0xC8(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = readMemory(mem, reg.SP);
        reg.SP += 1;
        uint8_t hi = readMemory(mem, reg.SP);
        reg.PC = (hi << 8) | lo;
        reg.SP += 1;
    } else {
        reg.PC += 1;
    }
}

// RET
void execute0xC9(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP+1);
    reg.PC = (hi << 8) | lo;
    reg.SP += 2;
}

// JP Z, a16
void execute0xCA(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);
        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

void executeCB(Registers& reg, Memory& mem, uint8_t opcode);

// PREFIX CB
void execute0xCB(Registers& reg, Memory& mem) {
    reg.PC += 1;
    uint8_t opcode = readMemory(mem, reg.PC);
    // CB命令のswitchに渡す処理をここに追加する
    executeCB(reg, mem, opcode);
}

// CALL Z, a16
void execute0xCC(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_Z)) {
        uint8_t lo = readMemory(mem, reg.PC + 1);
        uint8_t hi = readMemory(mem, reg.PC + 2);

        uint16_t next = reg.PC + 3;
        writeMemory(mem, reg.SP - 1, (next >> 8));
        writeMemory(mem, reg.SP - 2, (next & 0xFF));
        reg.SP -= 2;

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

// CALL a16
void execute0xCD(Registers& reg, Memory& mem) {
    
    uint8_t lo = readMemory(mem, reg.PC+1);
    uint8_t hi = readMemory(mem, reg.PC + 2);

    uint16_t next = reg.PC + 3;
    writeMemory(mem, reg.SP - 1, (next >> 8));
    writeMemory(mem, reg.SP - 2, (next & 0xFF));
    reg.SP -= 2;

    reg.PC = (hi << 8) | lo;
}

// ADC A,d8
void execute0xCE(Registers& reg, Memory& mem) {
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    uint8_t d8 = readMemory(mem, reg.PC+1);
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
    reg.SP -= 1;
    writeMemory(mem, reg.SP, (next >> 8) & 0xFF);
    reg.SP -= 1;
    writeMemory(mem, reg.SP, next & 0xFF);

    reg.PC = 0x0008;

}

//RET NC
void execute0xD0(Registers& reg, Memory& mem) {
    if(!getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.SP);
        uint8_t hi = readMemory(mem, reg.SP+1);
        
        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC += 1;
    }
}

//POP DE
void execute0xD1(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP+1);

    reg.E = lo;
    reg.D = hi;

    reg.SP += 2;
    reg.PC += 1;
}

//JP NC,a16
void execute0xD2(Registers& reg, Memory& mem) {
    if(!getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);
        reg.PC = (hi << 8) | lo;
    } else {
    reg.PC += 3;
    }
}

//CALL NC,a16
void execute0xD4(Registers& reg, Memory& mem) {
    if (!getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);
        uint16_t next = reg.PC + 3;

        writeMemory(mem, reg.SP - 1, (next >> 8));
        writeMemory(mem, reg.SP - 2, (next & 0xFF));
        reg.SP -= 2;

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}

//PUSH DE
void execute0xD5(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.D);
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.E);
    reg.PC += 1;
}


//SUB d8
void execute0xD6(Registers& reg, Memory& mem) {
    uint8_t d8 = readMemory(mem, reg.PC+1);
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
    writeMemory(mem, reg.SP-1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP-2, (next & 0xFF));
    reg.SP -=2;
    reg.PC = 0x0010;
}

//RET C
void execute0xD8(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.SP);
        uint8_t hi = readMemory(mem, reg.SP+1);

        reg.PC = (hi << 8) | lo;
        reg.SP += 2;
    } else {
        reg.PC +=1;
    }
}

// RETI
void execute0xD9(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP +1);
    reg.PC = (hi << 8) | lo;
    reg.SP += 2;

    reg.IME = true;  // 割り込みフラグを有効に戻す
}

//JP C,a16
void execute0xDA(Registers& reg, Memory& mem) {
    if (getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);

        reg.PC = (hi << 8) | lo;
    } else {
        reg.PC += 3;
    }
}


//CALL C,a16
void execute0xDC(Registers& reg, Memory& mem) {
    if(getFlag(reg, FLAG_C)) {
        uint8_t lo = readMemory(mem, reg.PC+1);
        uint8_t hi = readMemory(mem, reg.PC+2);

        uint16_t next = reg.PC +3;
        writeMemory(mem, reg.SP-1, next >> 8);
        writeMemory(mem, reg.SP-2, next & 0xFF);
        reg.SP -= 2;
        reg.PC = (hi << 8) | lo;

    } else {    
        reg.PC += 3;
    }
}

//SBC A,d8
void execute0xDE(Registers& reg, Memory& mem) {
    uint8_t d8 = readMemory(mem, reg.PC+1);
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;

    uint16_t result = reg.A - d8 - carry;

    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);
    setFlag(reg, FLAG_N, true);
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) < (d8 & 0x0F) + carry));
    setFlag(reg, FLAG_C, reg.A < d8 + carry);

    reg.A = result & 0xFF;
    reg.PC += 2;
}

//RST 18H
void execute0xDF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC+1;
    
    writeMemory(mem, reg.SP-1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP-2, next & 0xFF);
    reg.SP -= 2;

    reg.PC = 0x0018;
}


//LDH [a8], A
void execute0xE0(Registers& reg, Memory& mem) {
    uint8_t offset = readMemory(mem, reg.PC + 1);
    uint16_t address = 0xFF00 + offset;
    writeMemory(mem, address, reg.A);
    reg.PC += 2;
}

//POP HL
void execute0xE1(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP+1);

    reg.L = lo;
    reg.H = hi;

    reg.SP += 2;
    reg.PC += 1;
}

//LDH [C], A
void execute0xE2(Registers& reg, Memory& mem) {
    uint16_t address = 0xFF00 + reg.C;
    writeMemory(mem, address, reg.A);
    reg.PC += 1;
}

//PUSH HL
void execute0xE5(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.H);
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.L);
    reg.PC += 1;
}
// AND A, n8
void execute0xE6(Registers& reg, Memory& mem) {
    uint8_t n8 = readMemory(mem, reg.PC + 1);
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

    writeMemory(mem, reg.SP - 1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP - 2, next & 0xFF);
    reg.SP -= 2;

    reg.PC = 0x0020;
}


//ADD SP,r8
void execute0xE8(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC + 1);
    uint16_t result = reg.SP + r8;

    setFlag(reg, FLAG_Z, false);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, ((reg.SP ^ r8 ^ result) & 0x10) != 0);
    setFlag(reg, FLAG_C, ((reg.SP ^ r8 ^ result) & 0x100) != 0);

    reg.SP = result;
    reg.PC += 2;
}

void execute0xE9(Registers& reg) {
    reg.PC = (reg.H << 8) | reg.L;
}


//LD (a16),A
void execute0xEA(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.PC + 1);
    uint8_t hi = readMemory(mem, reg.PC + 2);

    uint16_t bc = (hi << 8) | lo;

    writeMemory(mem, bc, reg.A);
    reg.PC += 3;
}

//XOR d8
void execute0xEE(Registers& reg, Memory& mem) {
        uint8_t result = reg.A ^ readMemory(mem, reg.PC + 1);

        setFlag(reg, FLAG_Z, result == 0);
        setFlag(reg, FLAG_N, false);
        setFlag(reg, FLAG_H, false);
        setFlag(reg, FLAG_C, false);

        reg.A = result;
        reg.PC += 2;
}


//RST 28H
void execute0xEF(Registers& reg, Memory& mem) {
    uint16_t next = reg.PC + 1;
    writeMemory(mem, reg.SP -1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP -2, next & 0xFF);
    reg.SP -= 2;

    reg.PC = 0x0028;
}


//LDH A,(a8)
void execute0xF0(Registers& reg, Memory& mem) {
    uint8_t offset = readMemory(mem, reg.PC + 1);
    uint16_t address = 0xFF00 + offset;
    reg.A = readMemory(mem, address);
    reg.PC += 2;
}

//POP AF
void execute0xF1(Registers& reg, Memory& mem) {
    uint8_t lo = readMemory(mem, reg.SP);
    uint8_t hi = readMemory(mem, reg.SP+1);

    reg.F = lo;
    reg.A = hi;

    reg.PC +=1;
    reg.SP +=2;
}

//LD A,(C)
void execute0xF2(Registers& reg, Memory& mem) {
    uint16_t address = 0xFF00 + reg.C;
    reg.A = readMemory(mem, address);
    reg.PC += 1;
}


//PUSH AF
void execute0xF5(Registers& reg, Memory& mem) {
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.A);
    reg.SP -= 1;
    writeMemory(mem, reg.SP, reg.F);

    reg.PC +=1;
}

//OR d8
void execute0xF6(Registers& reg, Memory& mem) {
    uint8_t d8 = readMemory(mem, reg.PC+1);
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
    uint16_t next = reg.PC + 1;

    writeMemory(mem, reg.SP - 1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP - 2, next & 0xFF);

    reg.SP -= 2;
    reg.PC = 0x0030;
}

// LD HL, SP+r8
void execute0xF8(Registers& reg, Memory& mem) {
    int8_t r8 = (int8_t)readMemory(mem, reg.PC + 1);
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
    uint8_t lo = readMemory(mem, reg.PC + 1);
    uint8_t hi = readMemory(mem, reg.PC + 2);
    uint16_t address = (hi << 8) | lo;

    reg.A = readMemory(mem, address);
    reg.PC += 3;
}

// EI
void execute0xFB(Registers& reg) {
    reg.IME = true;
    reg.PC += 1;
}

//CP d8
void execute0xFE(Registers& reg, Memory& mem) {
    uint8_t d8 = readMemory(mem, reg.PC+1);
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
    writeMemory(mem, reg.SP - 1, (next >> 8) & 0xFF);
    writeMemory(mem, reg.SP - 2, next & 0xFF);
    reg.SP -= 2;

    reg.PC = 0x0038;
}


///プレフィックスコード-prefix

//RLC B
void executeCB0x00(Registers& reg) {
    uint8_t bit7 = (reg.B >> 7) & 1;
    reg.B = (reg.B << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC C
void executeCB0x01(Registers& reg) {
    uint8_t bit7 = (reg.C >> 7) & 1;
    reg.C = (reg.C << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC D
void executeCB0x02(Registers& reg) {
    uint8_t bit7 = (reg.D >> 7) & 1;
    reg.D = (reg.D << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC E
void executeCB0x03(Registers& reg) {
    uint8_t bit7 = (reg.E >> 7) & 1;
    reg.E = (reg.E << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC H
void executeCB0x04(Registers& reg) {
    uint8_t bit7 = (reg.H >> 7) & 1;
    reg.H = (reg.H << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC L
void executeCB0x05(Registers& reg) {
    uint8_t bit7 = (reg.L >> 7) & 1;
    reg.L = (reg.L << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RLC (HL)
void executeCB0x06(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    uint8_t bit7 = (value >> 7) & 1;
    value = (value << 1) | bit7;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RLC A
void executeCB0x07(Registers& reg) {
    uint8_t bit7 = (reg.A >> 7) & 1;
    reg.A = (reg.A << 1) | bit7;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

// RRC B
void executeCB0x08(Registers& reg) {
    uint8_t bit0 = reg.B & 1;
    reg.B = (reg.B >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC C
void executeCB0x09(Registers& reg) {
    uint8_t bit0 = reg.C & 1;
    reg.C = (reg.C >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC D
void executeCB0x0A(Registers& reg) {
    uint8_t bit0 = reg.D & 1;
    reg.D = (reg.D >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC E
void executeCB0x0B(Registers& reg) {
    uint8_t bit0 = reg.E & 1;
    reg.E = (reg.E >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC H
void executeCB0x0C(Registers& reg) {
    uint8_t bit0 = reg.H & 1;
    reg.H = (reg.H >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC L
void executeCB0x0D(Registers& reg) {
    uint8_t bit0 = reg.L & 1;
    reg.L = (reg.L >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//RRC (HL)
void executeCB0x0E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    uint8_t bit0 = value & 1;
    value = (value >> 1) | (bit0 << 7);
    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RRC A
void executeCB0x0F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    reg.A = (reg.A >> 1) | (bit0 << 7);

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RL B
void executeCB0x10(Registers& reg) {
    uint8_t bit7 = (reg.B >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.B = (reg.B << 1) | carry;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}


// RL C
void executeCB0x11(Registers& reg) {
    uint8_t bit7 = (reg.C >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.C = (reg.C << 1) | carry;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

// RL D
void executeCB0x12(Registers& reg) {
    uint8_t bit7 = (reg.D >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.D = (reg.D << 1) | carry;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

// RL E
void executeCB0x13(Registers& reg) {
    uint8_t bit7 = (reg.E >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.E = (reg.E << 1) | carry;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

// RL H
void executeCB0x14(Registers& reg) {
    uint8_t bit7 = (reg.H >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.H = (reg.H << 1) | carry;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RL L
void executeCB0x15(Registers& reg) {
    uint8_t bit7 = (reg.L >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.L = (reg.L << 1) | carry;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//RL (HL)
void executeCB0x16(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    uint8_t bit7 = (value >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    value = (value << 1) | carry;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RL A
void executeCB0x17(Registers& reg) {
    uint8_t bit7 = (reg.A >> 7) & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.A = (reg.A << 1) | carry;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

// RR B
void executeCB0x18(Registers& reg) {
    uint8_t bit0 = reg.B & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.B = (reg.B >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR C
void executeCB0x19(Registers& reg) {
    uint8_t bit0 = reg.C & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.C = (reg.C >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR D
void executeCB0x1A(Registers& reg) {
    uint8_t bit0 = reg.D & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.D = (reg.D >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR E
void executeCB0x1B(Registers& reg) {
    uint8_t bit0 = reg.E & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.E = (reg.E >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR H
void executeCB0x1C(Registers& reg) {
    uint8_t bit0 = reg.H & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.H = (reg.H >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR L
void executeCB0x1D(Registers& reg) {
    uint8_t bit0 = reg.L & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.L = (reg.L >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// RR (HL)
void executeCB0x1E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    uint8_t bit0 = value & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    value = (value >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RR A
void executeCB0x1F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    uint8_t carry = getFlag(reg, FLAG_C) ? 1 : 0;
    reg.A = (reg.A >> 1) | (carry << 7);

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}



//SLA B
void executeCB0x20(Registers& reg) {
    uint8_t bit7 = (reg.B >> 7) & 1;
    reg.B = reg.B << 1;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//SLA C
void executeCB0x21(Registers& reg) {
    uint8_t bit7 = (reg.C >> 7) & 1;
    reg.C = reg.C << 1;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//SLA D
void executeCB0x22(Registers& reg) {
    uint8_t bit7 = (reg.D >> 7) & 1;
    reg.D = reg.D << 1;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//SLA E
void executeCB0x23(Registers& reg) {
    uint8_t bit7 = (reg.E >> 7) & 1;
    reg.E = reg.E << 1;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);

    reg.PC += 2;
}

//SLA H
void executeCB0x24(Registers& reg) {
    uint8_t bit7 = (reg.H >> 7) & 1;
    reg.H = reg.H << 1;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);
    
    reg.PC += 2;
}

//SLA L
void executeCB0x25(Registers& reg) {
    uint8_t bit7 = (reg.L >> 7) & 1;
    reg.L = reg.L << 1;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);
    
    reg.PC += 2;
}

//SLA (HL)
void executeCB0x26(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    uint8_t bit7 = (value >> 7) & 1;
    value = value << 1;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);
    
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SLA A
void executeCB0x27(Registers& reg) {
    uint8_t bit7 = (reg.A >> 7) & 1;
    reg.A = reg.A << 1;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit7);
    
    reg.PC += 2;
}

// SRA B
void executeCB0x28(Registers& reg) {
    uint8_t bit0 = reg.B & 1;
    uint8_t bit7 = reg.B & 0x80;
    reg.B = (reg.B >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// SRA C
void executeCB0x29(Registers& reg) {
    uint8_t bit0 = reg.C & 1;
    uint8_t bit7 = reg.C & 0x80;
    reg.C = (reg.C >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// SRA D
void executeCB0x2A(Registers& reg) {
    uint8_t bit0 = reg.D & 1;
    uint8_t bit7 = reg.D & 0x80;
    reg.D = (reg.D >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// SRA E
void executeCB0x2B(Registers& reg) {
    uint8_t bit0 = reg.E & 1;
    uint8_t bit7 = reg.E & 0x80;
    reg.E = (reg.E >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

// SRA H
void executeCB0x2C(Registers& reg) {
    uint8_t bit0 = reg.H & 1;
    uint8_t bit7 = reg.H & 0x80;
    reg.H = (reg.H >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRA L
void executeCB0x2D(Registers& reg) {
    uint8_t bit0 = reg.L & 1;
    uint8_t bit7 = reg.L & 0x80;
    reg.L = (reg.L >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRA (HL)
void executeCB0x2E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    uint8_t bit0 = value & 1;
    uint8_t bit7 = value & 0x80;
    value = (value >> 1) | bit7;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SRA A
void executeCB0x2F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    uint8_t bit7 = reg.A & 0x80;
    reg.A = (reg.A >> 1) | bit7;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SWAP B
void executeCB0x30(Registers& reg) {
    reg.B = ((reg.B & 0x0F) << 4) | ((reg.B & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP C
void executeCB0x31(Registers& reg) {
    reg.C = ((reg.C & 0x0F) << 4) | ((reg.C & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP D
void executeCB0x32(Registers& reg) {
    reg.D = ((reg.D & 0x0F) << 4) | ((reg.D & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP E
void executeCB0x33(Registers& reg) {
    reg.E = ((reg.E & 0x0F) << 4) | ((reg.E & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP H
void executeCB0x34(Registers& reg) {
    reg.H = ((reg.H & 0x0F) << 4) | ((reg.H & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP L
void executeCB0x35(Registers& reg) {
    reg.L = ((reg.L & 0x0F) << 4) | ((reg.L & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SWAP (HL)
void executeCB0x36(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SWAP A
void executeCB0x37(Registers& reg) {
    reg.A = ((reg.A & 0x0F) << 4) | ((reg.A & 0xF0) >> 4);

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, false);

    reg.PC += 2;
}

//SRL B
void executeCB0x38(Registers& reg) {
    uint8_t bit0 = reg.B & 1;
    reg.B = reg.B >> 1;

    setFlag(reg, FLAG_Z, reg.B == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL C
void executeCB0x39(Registers& reg) {
    uint8_t bit0 = reg.C & 1;
    reg.C = reg.C >> 1;

    setFlag(reg, FLAG_Z, reg.C == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL D
void executeCB0x3A(Registers& reg) {
    uint8_t bit0 = reg.D & 1;
    reg.D = reg.D >> 1;

    setFlag(reg, FLAG_Z, reg.D == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL E
void executeCB0x3B(Registers& reg) {
    uint8_t bit0 = reg.E & 1;
    reg.E = reg.E >> 1;

    setFlag(reg, FLAG_Z, reg.E == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL H
void executeCB0x3C(Registers& reg) {
    uint8_t bit0 = reg.H & 1;
    reg.H = reg.H >> 1;

    setFlag(reg, FLAG_Z, reg.H == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL L
void executeCB0x3D(Registers& reg) {
    uint8_t bit0 = reg.L & 1;
    reg.L = reg.L >> 1;

    setFlag(reg, FLAG_Z, reg.L == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}

//SRL (HL)
void executeCB0x3E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    uint8_t bit0 = value & 1;
    value = value >> 1;

    setFlag(reg, FLAG_Z, value == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SRL A
void executeCB0x3F(Registers& reg) {
    uint8_t bit0 = reg.A & 1;
    reg.A = reg.A >> 1;

    setFlag(reg, FLAG_Z, reg.A == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, false);
    setFlag(reg, FLAG_C, bit0);

    reg.PC += 2;
}





//BIT 0,B
void executeCB0x40(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}


//BIT 0,C
void executeCB0x41(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 0,D
void executeCB0x42(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 0,E
void executeCB0x43(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 0,H
void executeCB0x44(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 0,L
void executeCB0x45(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//RES 0,(HL)
void executeCB0x46(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    setFlag(reg, FLAG_Z, (value & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);
    reg.PC += 2;
}

//BIT 0,A
void executeCB0x47(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x01) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,B
void executeCB0x48(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,C
void executeCB0x49(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,D
void executeCB0x4A(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,E
void executeCB0x4B(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,H
void executeCB0x4C(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,L
void executeCB0x4D(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 1,(HL)
void executeCB0x4E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (value & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);
    reg.PC += 2;
}

//BIT 1, A
void executeCB0x4F(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x02) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,B
void executeCB0x50(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,C
void executeCB0x51(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,D
void executeCB0x52(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,E
void executeCB0x53(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,H
void executeCB0x54(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,L
void executeCB0x55(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 2,(HL)
void executeCB0x56(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (value & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);

    reg.PC += 2;
}

//BIT 2,A
void executeCB0x57(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x04) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,B
void executeCB0x58(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,C
void executeCB0x59(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,D
void executeCB0x5A(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,E
void executeCB0x5B(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,H
void executeCB0x5C(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,L
void executeCB0x5D(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;
}

//BIT 3,(HL)
void executeCB0x5E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (value & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);
    reg.PC += 2;
}

//BIT 3,A
void executeCB0x5F(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x08) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,B
void executeCB0x60(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,C
void executeCB0x61(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,D
void executeCB0x62(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,E
void executeCB0x63(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,H
void executeCB0x64(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,L
void executeCB0x65(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 4,(HL)
void executeCB0x66(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (value & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//BIT 4,A
void executeCB0x67(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x10) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,B
void executeCB0x68(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,C
void executeCB0x69(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,D
void executeCB0x6A(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,E
void executeCB0x6B(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,H
void executeCB0x6C(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,L
void executeCB0x6D(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 5,(HL)
void executeCB0x6E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    setFlag(reg, FLAG_Z, (value & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);
    reg.PC += 2;;
}

//BIT 5,A
void executeCB0x6F(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x20) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,B
void executeCB0x70(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,C
void executeCB0x71(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,D
void executeCB0x72(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,E
void executeCB0x73(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,H
void executeCB0x74(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,L
void executeCB0x75(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,(HL)
void executeCB0x76(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);

    setFlag(reg, FLAG_Z, (value & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 6,A
void executeCB0x77(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x40) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,B
void executeCB0x78(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.B & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,C
void executeCB0x79(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.C & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,D
void executeCB0x7A(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.D & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,E
void executeCB0x7B(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.E & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,H
void executeCB0x7C(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.H & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,L
void executeCB0x7D(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.L & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}

//BIT 7,(HL)
void executeCB0x7E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    setFlag(reg, FLAG_Z, (value & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    // writeMemory(mem, hl, value);
    reg.PC += 2;;
}

//BIT 7,A
void executeCB0x7F(Registers& reg) {
    setFlag(reg, FLAG_Z, (reg.A & 0x80) == 0);
    setFlag(reg, FLAG_N, false);
    setFlag(reg, FLAG_H, true);

    reg.PC += 2;;
}


//RES 0,B
void executeCB0x80(Registers& reg) {
    reg.B &= ~0x01;
    reg.PC += 2;
}

//RES 0,C
void executeCB0x81(Registers& reg) {
    reg.C &= ~0x01;
    reg.PC += 2;
}

//RES 0,D
void executeCB0x82(Registers& reg) {
    reg.D &= ~0x01;
    reg.PC += 2;
}

//RES 0,E
void executeCB0x83(Registers& reg) {
    reg.E &= ~0x01;
    reg.PC += 2;
}

//RES 0,H
void executeCB0x84(Registers& reg) {
    reg.H &= ~0x01;
    reg.PC += 2;
}

//RES 0,L
void executeCB0x85(Registers& reg) {
    reg.L &= ~0x01;
    reg.PC += 2;
}

//RES 0,(HL)
void executeCB0x86(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x01;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 0,A
void executeCB0x87(Registers& reg) {
    reg.A &= ~0x01;
    reg.PC += 2;
}

//RES 1,B
void executeCB0x88(Registers& reg) {
    reg.B &= ~0x02;
    reg.PC += 2;
}

//RES 1,C
void executeCB0x89(Registers& reg) {
    reg.C &= ~0x02;
    reg.PC += 2;
}

//RES 1,D
void executeCB0x8A(Registers& reg) {
    reg.D &= ~0x02;
    reg.PC += 2;
}

//RES 1,E
void executeCB0x8B(Registers& reg) {
    reg.E &= ~0x02;
    reg.PC += 2;
}

//RES 1,H
void executeCB0x8C(Registers& reg) {
    reg.H &= ~0x02;
    reg.PC += 2;
}

//RES 1,L
void executeCB0x8D(Registers& reg) {
    reg.L &= ~0x02;
    reg.PC += 2;
}

//RES 1,(HL)
void executeCB0x8E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x02;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 1,A
void executeCB0x8F(Registers& reg) {
    reg.A &= ~0x02;
    reg.PC += 2;
}

//RES 2,B
void executeCB0x90(Registers& reg) {
    reg.B &= ~0x04;
    reg.PC += 2;
}

//RES 2,C
void executeCB0x91(Registers& reg) {
    reg.C &= ~0x04;
    reg.PC += 2;
}

//RES 2,D
void executeCB0x92(Registers& reg) {
    reg.D &= ~0x04;
    reg.PC += 2;
}

//RES 2,E
void executeCB0x93(Registers& reg) {
    reg.E &= ~0x04;
    reg.PC += 2;
}

//RES 2,H
void executeCB0x94(Registers& reg) {
    reg.H &= ~0x04;
    reg.PC += 2;
}

//RES 2,L
void executeCB0x95(Registers& reg) {
    reg.L &= ~0x04;
    reg.PC += 2;
}

//RES 2,(HL)
void executeCB0x96(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x04;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 2,A
void executeCB0x97(Registers& reg) {
    reg.A &= ~0x04;
    reg.PC += 2;
}

//RES 3,B
void executeCB0x98(Registers& reg) {
    reg.B &= ~0x08;
    reg.PC += 2;
}

//RES 3,C
void executeCB0x99(Registers& reg) {
    reg.C &= ~0x08;
    reg.PC += 2;
}

//RES 3,D
void executeCB0x9A(Registers& reg) {
    reg.D &= ~0x08;
    reg.PC += 2;
}

//RES 3,E
void executeCB0x9B(Registers& reg) {
    reg.E &= ~0x08;
    reg.PC += 2;
}

//RES 3,H
void executeCB0x9C(Registers& reg) {
    reg.H &= ~0x08;
    reg.PC += 2;
}

//RES 3,L
void executeCB0x9D(Registers& reg) {
    reg.L &= ~0x08;
    reg.PC += 2;
}

//RES 3,(HL)
void executeCB0x9E(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x08;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 3,A
void executeCB0x9F(Registers& reg) {
    reg.A &= ~0x08;
    reg.PC += 2;
}

//RES 4,B
void executeCB0xA0(Registers& reg) {
    reg.B &= ~0x10;
    reg.PC += 2;
}

//RES 4,C
void executeCB0xA1(Registers& reg) {
    reg.C &= ~0x10;
    reg.PC += 2;
}

//RES 4,D
void executeCB0xA2(Registers& reg) {
    reg.D &= ~0x10;
    reg.PC += 2;
}

//RES 4,E
void executeCB0xA3(Registers& reg) {
    reg.E &= ~0x10;
    reg.PC += 2;
}

//RES 4,H
void executeCB0xA4(Registers& reg) {
    reg.H &= ~0x10;
    reg.PC += 2;
}

//RES 4,L
void executeCB0xA5(Registers& reg) {
    reg.L &= ~0x10;
    reg.PC += 2;
}

//RES 4,(HL)
void executeCB0xA6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x10;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 4,A
void executeCB0xA7(Registers& reg) {
    reg.A &= ~0x10;
    reg.PC += 2;
}

//RES 5,B
void executeCB0xA8(Registers& reg) {
    reg.B &= ~0x20;
    reg.PC += 2;
}

//RES 5,C
void executeCB0xA9(Registers& reg) {
    reg.C &= ~0x20;
    reg.PC += 2;
}

//RES 5,D
void executeCB0xAA(Registers& reg) {
    reg.D &= ~0x20;
    reg.PC += 2;
}

//RES 5,E
void executeCB0xAB(Registers& reg) {
    reg.E &= ~0x20;
    reg.PC += 2;
}

//RES 5,H
void executeCB0xAC(Registers& reg) {
    reg.H &= ~0x20;
    reg.PC += 2;
}

//RES 5,L
void executeCB0xAD(Registers& reg) {
    reg.L &= ~0x20;
    reg.PC += 2;
}

//RES 5,(HL)
void executeCB0xAE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x20;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 5,A
void executeCB0xAF(Registers& reg) {
    reg.A &= ~0x20;
    reg.PC += 2;
}


//RES 6,B
void executeCB0xB0(Registers& reg) {
    reg.B &= ~0x40;
    reg.PC += 2;
}

//RES 6,C
void executeCB0xB1(Registers& reg) {
    reg.C &= ~0x40;
    reg.PC += 2;
}

//RES 6,D
void executeCB0xB2(Registers& reg) {
    reg.D &= ~0x40;
    reg.PC += 2;
}

//RES 6,E
void executeCB0xB3(Registers& reg) {
    reg.E &= ~0x40;
    reg.PC += 2;
}

//RES 6,H
void executeCB0xB4(Registers& reg) {
    reg.H &= ~0x40;
    reg.PC += 2;
}

//RES 6,L
void executeCB0xB5(Registers& reg) {
    reg.L &= ~0x40;
    reg.PC += 2;
}

//RES 6,(HL)
void executeCB0xB6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x40;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 6,A
void executeCB0xB7(Registers& reg) {
    reg.A &= ~0x40;
    reg.PC += 2;
}

//RES 7,B
void executeCB0xB8(Registers& reg) {
    reg.B &= ~0x80;
    reg.PC += 2;
}

//RES 7,C
void executeCB0xB9(Registers& reg) {
    reg.C &= ~0x80;
    reg.PC += 2;
}

//RES 7,D
void executeCB0xBA(Registers& reg) {
    reg.D &= ~0x80;
    reg.PC += 2;
}

//RES 7,E
void executeCB0xBB(Registers& reg) {
    reg.E &= ~0x80;
    reg.PC += 2;
}

//RES 7,H
void executeCB0xBC(Registers& reg) {
    reg.H &= ~0x80;
    reg.PC += 2;
}

//RES 7,L
void executeCB0xBD(Registers& reg) {
    reg.L &= ~0x80;
    reg.PC += 2;
}

//RES 7,(HL)
void executeCB0xBE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value &= ~0x80;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//RES 7,A
void executeCB0xBF(Registers& reg) {
    reg.A &= ~0x80;
    reg.PC += 2;
}

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
    uint8_t value = readMemory(mem, hl);
    value |= 0x01;
    writeMemory(mem, hl, value);
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
    uint8_t value = readMemory(mem, hl);
    value |= 0x02;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SET 1,A
void executeCB0xCF(Registers& reg) {
    reg.A |= 0x02;
    reg.PC += 2;
}

//SET 2,B
void executeCB0xD0(Registers& reg) {
    reg.B |= 0x04;
    reg.PC += 2;
}

//SET 2,C
void executeCB0xD1(Registers& reg) {
    reg.C |= 0x04;
    reg.PC += 2;
}

//SET 2,D
void executeCB0xD2(Registers& reg) {
    reg.D |= 0x04;
    reg.PC += 2;
}

//SET 2,E
void executeCB0xD3(Registers& reg) {
    reg.E |= 0x04;
    reg.PC += 2;
}

//SET 2,H
void executeCB0xD4(Registers& reg) {
    reg.H |= 0x04;
    reg.PC += 2;
}

//SET 2,L
void executeCB0xD5(Registers& reg) {
    reg.L |= 0x04;
    reg.PC += 2;
}

//SET 2,(HL)
void executeCB0xD6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x04;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SET 2,A
void executeCB0xD7(Registers& reg) {
    reg.A |= 0x04;
    reg.PC += 2;
}

//SET 3,B
void executeCB0xD8(Registers& reg) {
    reg.B |= 0x08;
    reg.PC += 2;
}

//SET 3,C
void executeCB0xD9(Registers& reg) {
    reg.C |= 0x08;
    reg.PC += 2;
}

//SET 3,D
void executeCB0xDA(Registers& reg) {
    reg.D |= 0x08;
    reg.PC += 2;
}

//SET 3,E
void executeCB0xDB(Registers& reg) {
    reg.E |= 0x08;
    reg.PC += 2;
}

//SET 3,H
void executeCB0xDC(Registers& reg) {
    reg.H |= 0x08;
    reg.PC += 2;
}

//SET 3,L
void executeCB0xDD(Registers& reg) {
    reg.L |= 0x08;
    reg.PC += 2;
}

//SET 3,(HL)
void executeCB0xDE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x08;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SET 3,A
void executeCB0xDF(Registers& reg) {
    reg.A |= 0x08;
    reg.PC += 2;
}

//SET 4,B
void executeCB0xE0(Registers& reg) {
    reg.B |= 0x10;
    reg.PC += 2;
}

//SET 4,C
void executeCB0xE1(Registers& reg) {
    reg.C |= 0x10;
    reg.PC += 2;
}

//SET 4,D
void executeCB0xE2(Registers& reg) {
    reg.D |= 0x10;
    reg.PC += 2;
}

//SET 4,E
void executeCB0xE3(Registers& reg) {
    reg.E |= 0x10;
    reg.PC += 2;
}

//SET 4,H
void executeCB0xE4(Registers& reg) {
    reg.H |= 0x10;
    reg.PC += 2;
}

//SET 4,L
void executeCB0xE5(Registers& reg) {
    reg.L |= 0x10;
    reg.PC += 2;
}

//SET 4,(HL)
void executeCB0xE6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x10;
    writeMemory(mem, hl, value);
    reg.PC += 2;
}

//SET 4,A
void executeCB0xE7(Registers& reg) {
    reg.A |= 0x10;
    reg.PC += 2;
}

//SET 5,B
void executeCB0xE8(Registers& reg) {
    reg.B |= 0x20;
    reg.PC += 2;  
}

//SET 5,C
void executeCB0xE9(Registers& reg) {
    reg.C |= 0x20;
    reg.PC += 2;  
}

//SET 5,D
void executeCB0xEA(Registers& reg) {
    reg.D |= 0x20;
    reg.PC += 2;  
}

//SET 5,E
void executeCB0xEB(Registers& reg) {
    reg.E |= 0x20;
    reg.PC += 2;  
}

//SET 5,H
void executeCB0xEC(Registers& reg) {
    reg.H |= 0x20;
    reg.PC += 2;  
}

//SET 5,L
void executeCB0xED(Registers& reg) {
    reg.L |= 0x20;
    reg.PC += 2;  
}

//SET 5,(HL)
void executeCB0xEE(Registers& reg, Memory&mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x20;
    writeMemory(mem, hl, value);
    reg.PC += 2;  
}

//SET 5,A
void executeCB0xEF(Registers& reg) {
    reg.A |= 0x20;
    reg.PC += 2;  
}

//SET 6,B
void executeCB0xF0(Registers& reg) {
    reg.B |= 0x40;
    reg.PC += 2;  
}

//SET 6,C
void executeCB0xF1(Registers& reg) {
    reg.C |= 0x40;
    reg.PC += 2;  
}

//SET 6,D
void executeCB0xF2(Registers& reg) {
    reg.D |= 0x40;
    reg.PC += 2;  
}

//SET 6,E
void executeCB0xF3(Registers& reg) {
    reg.E |= 0x40;
    reg.PC += 2;  
}

//SET 6,H
void executeCB0xF4(Registers& reg) {
    reg.H |= 0x40;
    reg.PC += 2;  
}

//SET 6,L
void executeCB0xF5(Registers& reg) {
    reg.L |= 0x40;
    reg.PC += 2;  
}

//SET 6,(HL)
void executeCB0xF6(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x40;
    writeMemory(mem, hl, value);
    reg.PC += 2;  
}

//SET 6,A
void executeCB0xF7(Registers& reg) {
    reg.A |= 0x40;
    reg.PC += 2;  
}

//SET 7,B
void executeCB0xF8(Registers& reg) {
    reg.B |= 0x80;
    reg.PC += 2;  
}

//SET 7,C
void executeCB0xF9(Registers& reg) {
    reg.C |= 0x80;
    reg.PC += 2;  
}

//SET 7,D
void executeCB0xFA(Registers& reg) {
    reg.D |= 0x80;
    reg.PC += 2;  
}

//SET 7,E
void executeCB0xFB(Registers& reg) {
    reg.E |= 0x80;
    reg.PC += 2;  
}


//SET 7,H
void executeCB0xFC(Registers& reg) {
    reg.H |= 0x80;
    reg.PC += 2;  
}

//SET 7,L
void executeCB0xFD(Registers& reg) {
    reg.L |= 0x80;
    reg.PC += 2;  
}

//SET 7,(HL)
void executeCB0xFE(Registers& reg, Memory& mem) {
    uint16_t hl = (reg.H << 8) | reg.L;
    uint8_t value = readMemory(mem, hl);
    value |= 0x80;
    writeMemory(mem, hl, value);
    reg.PC += 2;  
}


//SET 7,A
void executeCB0xFF(Registers& reg) {
    reg.A |= 0x80;
    reg.PC += 2;  
}










void execute0xF3(Registers& reg) {
    reg.IME = false;
    reg.PC += 1;
}

void executeCB(Registers& reg, Memory& mem, uint8_t opcode) {
    switch (opcode) {
        case 0x00: executeCB0x00(reg); break;
        case 0x01: executeCB0x01(reg); break;
        case 0x02: executeCB0x02(reg); break;
        case 0x03: executeCB0x03(reg); break;
        case 0x04: executeCB0x04(reg); break;
        case 0x05: executeCB0x05(reg); break;
        case 0x06: executeCB0x06(reg, mem); break;
        case 0x07: executeCB0x07(reg); break;
        case 0x08: executeCB0x08(reg); break;
        case 0x09: executeCB0x09(reg); break;
        case 0x0A: executeCB0x0A(reg); break;
        case 0x0B: executeCB0x0B(reg); break;
        case 0x0C: executeCB0x0C(reg); break;
        case 0x0D: executeCB0x0D(reg); break;
        case 0x0E: executeCB0x0E(reg, mem); break;
        case 0x0F: executeCB0x0F(reg); break;
        case 0x10: executeCB0x10(reg); break;
        case 0x11: executeCB0x11(reg); break;
        case 0x12: executeCB0x12(reg); break;
        case 0x13: executeCB0x13(reg); break;
        case 0x14: executeCB0x14(reg); break;
        case 0x15: executeCB0x15(reg); break;
        case 0x16: executeCB0x16(reg, mem); break;
        case 0x17: executeCB0x17(reg); break;
        case 0x18: executeCB0x18(reg); break;
        case 0x19: executeCB0x19(reg); break;
        case 0x1A: executeCB0x1A(reg); break;
        case 0x1B: executeCB0x1B(reg); break;
        case 0x1C: executeCB0x1C(reg); break;
        case 0x1D: executeCB0x1D(reg); break;
        case 0x1E: executeCB0x1E(reg, mem); break;
        case 0x1F: executeCB0x1F(reg); break;
        case 0x20: executeCB0x20(reg); break;
        case 0x21: executeCB0x21(reg); break;
        case 0x22: executeCB0x22(reg); break;
        case 0x23: executeCB0x23(reg); break;
        case 0x24: executeCB0x24(reg); break;
        case 0x25: executeCB0x25(reg); break;
        case 0x26: executeCB0x26(reg, mem); break;
        case 0x27: executeCB0x27(reg); break;
        case 0x28: executeCB0x28(reg); break;
        case 0x29: executeCB0x29(reg); break;
        case 0x2A: executeCB0x2A(reg); break;
        case 0x2B: executeCB0x2B(reg); break;
        case 0x2C: executeCB0x2C(reg); break;
        case 0x2D: executeCB0x2D(reg); break;
        case 0x2E: executeCB0x2E(reg, mem); break;
        case 0x2F: executeCB0x2F(reg); break;
        case 0x30: executeCB0x30(reg); break;
        case 0x31: executeCB0x31(reg); break;
        case 0x32: executeCB0x32(reg); break;
        case 0x33: executeCB0x33(reg); break;
        case 0x34: executeCB0x34(reg); break;
        case 0x35: executeCB0x35(reg); break;
        case 0x36: executeCB0x36(reg, mem); break;
        case 0x37: executeCB0x37(reg); break;
        case 0x38: executeCB0x38(reg); break;
        case 0x39: executeCB0x39(reg); break;
        case 0x3A: executeCB0x3A(reg); break;
        case 0x3B: executeCB0x3B(reg); break;
        case 0x3C: executeCB0x3C(reg); break;
        case 0x3D: executeCB0x3D(reg); break;
        case 0x3E: executeCB0x3E(reg, mem); break;
        case 0x3F: executeCB0x3F(reg); break;
        case 0x40: executeCB0x40(reg); break;
        case 0x41: executeCB0x41(reg); break;
        case 0x42: executeCB0x42(reg); break;
        case 0x43: executeCB0x43(reg); break;
        case 0x44: executeCB0x44(reg); break;
        case 0x45: executeCB0x45(reg); break;
        case 0x46: executeCB0x46(reg, mem); break;
        case 0x47: executeCB0x47(reg); break;
        case 0x48: executeCB0x48(reg); break;
        case 0x49: executeCB0x49(reg); break;
        case 0x4A: executeCB0x4A(reg); break;
        case 0x4B: executeCB0x4B(reg); break;
        case 0x4C: executeCB0x4C(reg); break;
        case 0x4D: executeCB0x4D(reg); break;
        case 0x4E: executeCB0x4E(reg, mem); break;
        case 0x4F: executeCB0x4F(reg); break;
        case 0x50: executeCB0x50(reg); break;
        case 0x51: executeCB0x51(reg); break;
        case 0x52: executeCB0x52(reg); break;
        case 0x53: executeCB0x53(reg); break;
        case 0x54: executeCB0x54(reg); break;
        case 0x55: executeCB0x55(reg); break;
        case 0x56: executeCB0x56(reg, mem); break;
        case 0x57: executeCB0x57(reg); break;
        case 0x58: executeCB0x58(reg); break;
        case 0x59: executeCB0x59(reg); break;
        case 0x5A: executeCB0x5A(reg); break;
        case 0x5B: executeCB0x5B(reg); break;
        case 0x5C: executeCB0x5C(reg); break;
        case 0x5D: executeCB0x5D(reg); break;
        case 0x5E: executeCB0x5E(reg, mem); break;
        case 0x5F: executeCB0x5F(reg); break;
        case 0x60: executeCB0x60(reg); break;
        case 0x61: executeCB0x61(reg); break;
        case 0x62: executeCB0x62(reg); break;
        case 0x63: executeCB0x63(reg); break;
        case 0x64: executeCB0x64(reg); break;
        case 0x65: executeCB0x65(reg); break;
        case 0x66: executeCB0x66(reg, mem); break;
        case 0x67: executeCB0x67(reg); break;
        case 0x68: executeCB0x68(reg); break;
        case 0x69: executeCB0x69(reg); break;
        case 0x6A: executeCB0x6A(reg); break;
        case 0x6B: executeCB0x6B(reg); break;
        case 0x6C: executeCB0x6C(reg); break;
        case 0x6D: executeCB0x6D(reg); break;
        case 0x6E: executeCB0x6E(reg, mem); break;
        case 0x6F: executeCB0x6F(reg); break;
        case 0x70: executeCB0x70(reg); break;
        case 0x71: executeCB0x71(reg); break;
        case 0x72: executeCB0x72(reg); break;
        case 0x73: executeCB0x73(reg); break;
        case 0x74: executeCB0x74(reg); break;
        case 0x75: executeCB0x75(reg); break;
        case 0x76: executeCB0x76(reg, mem); break;
        case 0x77: executeCB0x77(reg); break;
        case 0x78: executeCB0x78(reg); break;
        case 0x79: executeCB0x79(reg); break;
        case 0x7A: executeCB0x7A(reg); break;
        case 0x7B: executeCB0x7B(reg); break;
        case 0x7C: executeCB0x7C(reg); break;
        case 0x7D: executeCB0x7D(reg); break;
        case 0x7E: executeCB0x7E(reg, mem); break;
        case 0x7F: executeCB0x7F(reg); break;
        case 0x80: executeCB0x80(reg); break;
        case 0x81: executeCB0x81(reg); break;
        case 0x82: executeCB0x82(reg); break;
        case 0x83: executeCB0x83(reg); break;
        case 0x84: executeCB0x84(reg); break;
        case 0x85: executeCB0x85(reg); break;
        case 0x86: executeCB0x86(reg, mem); break;
        case 0x87: executeCB0x87(reg); break;
        case 0x88: executeCB0x88(reg); break;
        case 0x89: executeCB0x89(reg); break;
        case 0x8A: executeCB0x8A(reg); break;
        case 0x8B: executeCB0x8B(reg); break;
        case 0x8C: executeCB0x8C(reg); break;
        case 0x8D: executeCB0x8D(reg); break;
        case 0x8E: executeCB0x8E(reg, mem); break;
        case 0x8F: executeCB0x8F(reg); break;
        case 0x90: executeCB0x90(reg); break;
        case 0x91: executeCB0x91(reg); break;
        case 0x92: executeCB0x92(reg); break;
        case 0x93: executeCB0x93(reg); break;
        case 0x94: executeCB0x94(reg); break;
        case 0x95: executeCB0x95(reg); break;
        case 0x96: executeCB0x96(reg, mem); break;
        case 0x97: executeCB0x97(reg); break;
        case 0x98: executeCB0x98(reg); break;
        case 0x99: executeCB0x99(reg); break;
        case 0x9A: executeCB0x9A(reg); break;
        case 0x9B: executeCB0x9B(reg); break;
        case 0x9C: executeCB0x9C(reg); break;
        case 0x9D: executeCB0x9D(reg); break;
        case 0x9E: executeCB0x9E(reg, mem); break;
        case 0x9F: executeCB0x9F(reg); break;
        case 0xA0: executeCB0xA0(reg); break;
        case 0xA1: executeCB0xA1(reg); break;
        case 0xA2: executeCB0xA2(reg); break;
        case 0xA3: executeCB0xA3(reg); break;
        case 0xA4: executeCB0xA4(reg); break;
        case 0xA5: executeCB0xA5(reg); break;
        case 0xA6: executeCB0xA6(reg, mem); break;
        case 0xA7: executeCB0xA7(reg); break;
        case 0xA8: executeCB0xA8(reg); break;
        case 0xA9: executeCB0xA9(reg); break;
        case 0xAA: executeCB0xAA(reg); break;
        case 0xAB: executeCB0xAB(reg); break;
        case 0xAC: executeCB0xAC(reg); break;
        case 0xAD: executeCB0xAD(reg); break;
        case 0xAE: executeCB0xAE(reg, mem); break;
        case 0xAF: executeCB0xAF(reg); break;
        case 0xB0: executeCB0xB0(reg); break;
        case 0xB1: executeCB0xB1(reg); break;
        case 0xB2: executeCB0xB2(reg); break;
        case 0xB3: executeCB0xB3(reg); break;
        case 0xB4: executeCB0xB4(reg); break;
        case 0xB5: executeCB0xB5(reg); break;
        case 0xB6: executeCB0xB6(reg, mem); break;
        case 0xB7: executeCB0xB7(reg); break;
        case 0xB8: executeCB0xB8(reg); break;
        case 0xB9: executeCB0xB9(reg); break;
        case 0xBA: executeCB0xBA(reg); break;
        case 0xBB: executeCB0xBB(reg); break;
        case 0xBC: executeCB0xBC(reg); break;
        case 0xBD: executeCB0xBD(reg); break;
        case 0xBE: executeCB0xBE(reg, mem); break;
        case 0xBF: executeCB0xBF(reg); break;
        case 0xC0: executeCB0xC0(reg); break;
        case 0xC1: executeCB0xC1(reg); break;
        case 0xC2: executeCB0xC2(reg); break;
        case 0xC3: executeCB0xC3(reg); break;
        case 0xC4: executeCB0xC4(reg); break;
        case 0xC5: executeCB0xC5(reg); break;
        case 0xC6: executeCB0xC6(reg, mem); break;
        case 0xC7: executeCB0xC7(reg); break;
        case 0xC8: executeCB0xC8(reg); break;
        case 0xC9: executeCB0xC9(reg); break;
        case 0xCA: executeCB0xCA(reg); break;
        case 0xCB: executeCB0xCB(reg); break;
        case 0xCC: executeCB0xCC(reg); break;
        case 0xCD: executeCB0xCD(reg); break;
        case 0xCE: executeCB0xCE(reg, mem); break;
        case 0xCF: executeCB0xCF(reg); break;
        case 0xD0: executeCB0xD0(reg); break;
        case 0xD1: executeCB0xD1(reg); break;
        case 0xD2: executeCB0xD2(reg); break;
        case 0xD3: executeCB0xD3(reg); break;
        case 0xD4: executeCB0xD4(reg); break;
        case 0xD5: executeCB0xD5(reg); break;
        case 0xD6: executeCB0xD6(reg, mem); break;
        case 0xD7: executeCB0xD7(reg); break;
        case 0xD8: executeCB0xD8(reg); break;
        case 0xD9: executeCB0xD9(reg); break;
        case 0xDA: executeCB0xDA(reg); break;
        case 0xDB: executeCB0xDB(reg); break;
        case 0xDC: executeCB0xDC(reg); break;
        case 0xDD: executeCB0xDD(reg); break;
        case 0xDE: executeCB0xDE(reg, mem); break;
        case 0xDF: executeCB0xDF(reg); break;
        case 0xE0: executeCB0xE0(reg); break;
        case 0xE1: executeCB0xE1(reg); break;
        case 0xE2: executeCB0xE2(reg); break;
        case 0xE3: executeCB0xE3(reg); break;
        case 0xE4: executeCB0xE4(reg); break;
        case 0xE5: executeCB0xE5(reg); break;
        case 0xE6: executeCB0xE6(reg, mem); break;
        case 0xE7: executeCB0xE7(reg); break;
        case 0xE8: executeCB0xE8(reg); break;
        case 0xE9: executeCB0xE9(reg); break;
        case 0xEA: executeCB0xEA(reg); break;
        case 0xEB: executeCB0xEB(reg); break;
        case 0xEC: executeCB0xEC(reg); break;
        case 0xED: executeCB0xED(reg); break;
        case 0xEE: executeCB0xEE(reg, mem); break;
        case 0xEF: executeCB0xEF(reg); break;
        case 0xF0: executeCB0xF0(reg); break;
        case 0xF1: executeCB0xF1(reg); break;
        case 0xF2: executeCB0xF2(reg); break;
        case 0xF3: executeCB0xF3(reg); break;
        case 0xF4: executeCB0xF4(reg); break;
        case 0xF5: executeCB0xF5(reg); break;
        case 0xF6: executeCB0xF6(reg, mem); break;
        case 0xF7: executeCB0xF7(reg); break;
        case 0xF8: executeCB0xF8(reg); break;
        case 0xF9: executeCB0xF9(reg); break;
        case 0xFA: executeCB0xFA(reg); break;
        case 0xFB: executeCB0xFB(reg); break;
        case 0xFC: executeCB0xFC(reg); break;
        case 0xFD: executeCB0xFD(reg); break;
        case 0xFE: executeCB0xFE(reg, mem); break;
        case 0xFF: executeCB0xFF(reg); break;
    }
}

void step(Registers& reg, Memory& mem) {
    uint8_t opcode = readMemory(mem, reg.PC);

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

        case 0x03:
            execute0x03(reg);
            break;

        case 0x05:
            execute0x05(reg);
            break;

        case 0x07:
            execute0x07(reg);
            break;

        case 0x08:
            execute0x08(reg, mem);
            break;

        case 0x09:
            execute0x09(reg);
            break;

        case 0x0B:
            execute0x0B(reg);
            break;

        case 0x0C:
            execute0x0C(reg);
            break;

        case 0x0D:
            execute0x0D(reg);
            break;

        case 0x0F:
            execute0x0F(reg);
            break;

        case 0x10:
            execute0x10(reg);
            break;

        case 0x13:
            execute0x13(reg, mem);
            break;

        case 0x14:
            execute0x14(reg);
            break;

        case 0x15:
            execute0x15(reg);
            break;

        case 0x17:
            execute0x17(reg);
            break;

        case 0x18:
            execute0x18(reg, mem);
            break;

        case 0x19:
            execute0x19(reg);
            break;

        case 0x1B:
            execute0x1B(reg);
            break;

        case 0x1C:
            execute0x1C(reg, mem);
            break;

        case 0x1D:
            execute0x1D(reg);
            break;

        case 0x1F:
            execute0x1F(reg);
            break;

        case 0x20:
            execute0x20(reg, mem);
            break;

        case 0x21:
            execute0x21(reg, mem);
            break;

        case 0x23:
            execute0x23(reg);
            break;

        case 0x24:
            execute0x24(reg);
            break;

        case 0x25:
            execute0x25(reg);
            break;

        case 0x26:
            execute0x26(reg, mem);
            break;

        case 0x27:
            execute0x27(reg);
            break;

        case 0x28:
            execute0x28(reg, mem);
            break;

        case 0x29:
            execute0x29(reg);
            break;

        case 0x2B:
            execute0x2B(reg);
            break;

        case 0x2C:
            execute0x2C(reg);
            break;

        case 0x2D:
            execute0x2D(reg);
            break;

        case 0x2F:
            execute0x2F(reg);
            break;

        case 0x30:
            execute0x30(reg, mem);
            break;

        case 0x31:
            execute0x31(reg, mem);
            break;

        case 0x33:
            execute0x33(reg);
            break;

        case 0x34:
            execute0x34(reg, mem);
            break;

        case 0x35:
            execute0x35(reg, mem);
            break;

        case 0x37:
            execute0x37(reg);
            break;

        case 0x38:
            execute0x38(reg, mem);
            break;

        case 0x39:
            execute0x39(reg);
            break;

        case 0x3B:
            execute0x3B(reg);
            break;

        case 0x3C:
            execute0x3C(reg);
            break;

        case 0x3D:
            execute0x3D(reg);
            break;

        case 0x3F:
            execute0x3F(reg);
            break;

        case 0x44:
            execute0x44(reg, mem);
            break;

        case 0x45:
            execute0x45(reg, mem);
            break;

        case 0x46:
            execute0x46(reg, mem);
            break;

        case 0x47:
            execute0x47(reg, mem);
            break;

        case 0x48:
            execute0x48(reg, mem);
            break;

        case 0x49:
            execute0x49(reg, mem);
            break;

        case 0x4A:
            execute0x4A(reg, mem);
            break;

        case 0x4B:
            execute0x4B(reg, mem);
            break;

        case 0x4C:
            execute0x4C(reg, mem);
            break;

        case 0x4D:
            execute0x4D(reg, mem);
            break;

        case 0x4E:
            execute0x4E(reg, mem);
            break;

        case 0x4F:
            execute0x4F(reg, mem);
            break;

        case 0x50:
            execute0x50(reg, mem);
            break;

        case 0x51:
            execute0x51(reg, mem);
            break;

        case 0x52:
            execute0x52(reg, mem);
            break;

        case 0x53:
            execute0x53(reg, mem);
            break;

        case 0x54:
            execute0x54(reg, mem);
            break;

        case 0x55:
            execute0x55(reg, mem);
            break;

        case 0x56:
            execute0x56(reg, mem);
            break;

        case 0x57:
            execute0x57(reg, mem);
            break;

        case 0x58:
            execute0x58(reg, mem);
            break;

        case 0x59:
            execute0x59(reg, mem);
            break;

        case 0x5A:
            execute0x5A(reg, mem);
            break;

        case 0x5B:
            execute0x5B(reg, mem);
            break;

        case 0x5C:
            execute0x5C(reg, mem);
            break;

        case 0x5D:
            execute0x5D(reg, mem);
            break;

        case 0x5E:
            execute0x5E(reg, mem);
            break;

        case 0x5F:
            execute0x5F(reg, mem);
            break;

        case 0x60:
            execute0x60(reg, mem);
            break;

        case 0x61:
            execute0x61(reg, mem);
            break;

        case 0x62:
            execute0x62(reg, mem);
            break;

        case 0x63:
            execute0x63(reg, mem);
            break;

        case 0x64:
            execute0x64(reg, mem);
            break;

        case 0x65:
            execute0x65(reg, mem);
            break;

        case 0x66:
            execute0x66(reg, mem);
            break;

        case 0x67:
            execute0x67(reg, mem);
            break;

        case 0x68:
            execute0x68(reg, mem);
            break;

        case 0x69:
            execute0x69(reg, mem);
            break;

        case 0x6A:
            execute0x6A(reg, mem);
            break;

        case 0x6B:
            execute0x6B(reg, mem);
            break;

        case 0x6C:
            execute0x6C(reg, mem);
            break;

        case 0x6D:
            execute0x6D(reg, mem);
            break;

        case 0x6E:
            execute0x6E(reg, mem);
            break;

        case 0x6F:
            execute0x6F(reg, mem);
            break;

        case 0x70:
            execute0x70(reg, mem);
            break;

        case 0x71:
            execute0x71(reg, mem);
            break;

        case 0x72:
            execute0x72(reg, mem);
            break;

        case 0x73:
            execute0x73(reg, mem);
            break;

        case 0x74:
            execute0x74(reg, mem);
            break;

        case 0x75:
            execute0x75(reg, mem);
            break;

        case 0x76:
            execute0x76(reg);
            break;

        case 0x77:
            execute0x77(reg, mem);
            break;

        case 0x78:
            execute0x78(reg, mem);
            break;

        case 0x79:
            execute0x79(reg, mem);
            break;

        case 0x7A:
            execute0x7A(reg, mem);
            break;

        case 0x7B:
            execute0x7B(reg, mem);
            break;

        case 0x7C:
            execute0x7C(reg, mem);
            break;

        case 0x7D:
            execute0x7D(reg, mem);
            break;

        case 0x7E:
            execute0x7E(reg, mem);
            break;

        case 0x7F:
            execute0x7F(reg, mem);
            break;

        case 0x80:
            execute0x80(reg);
            break;

        case 0x81:
            execute0x81(reg);
            break;

        case 0x82:
            execute0x82(reg);
            break;

        case 0x83:
            execute0x83(reg);
            break;

        case 0x84:
            execute0x84(reg);
            break;

        case 0x85:
            execute0x85(reg);
            break;

        case 0x86:
            execute0x86(reg, mem);
            break;

        case 0x87:
            execute0x87(reg);
            break;

        case 0x88:
            execute0x88(reg);
            break;

        case 0x89:
            execute0x89(reg);
            break;

        case 0x8A:
            execute0x8A(reg);
            break;

        case 0x8B:
            execute0x8B(reg);
            break;

        case 0x8C:
            execute0x8C(reg);
            break;

        case 0x8D:
            execute0x8D(reg);
            break;

        case 0x8E:
            execute0x8E(reg, mem);
            break;

        case 0x8F:
            execute0x8F(reg);
            break;

        case 0x90:
            execute0x90(reg);
            break;

        case 0x91:
            execute0x91(reg);
            break;

        case 0x92:
            execute0x92(reg);
            break;

        case 0x93:
            execute0x93(reg);
            break;

        case 0x94:
            execute0x94(reg);
            break;

        case 0x95:
            execute0x95(reg);
            break;

        case 0x96:
            execute0x96(reg, mem);
            break;

        case 0x97:
            execute0x97(reg);
            break;

        case 0x98:
            execute0x98(reg);
            break;

        case 0x99:
            execute0x99(reg);
            break;

        case 0x9A:
            execute0x9A(reg);
            break;

        case 0x9B:
            execute0x9B(reg);
            break;

        case 0x9C:
            execute0x9C(reg);
            break;

        case 0x9D:
            execute0x9D(reg);
            break;

        case 0x9E:
            execute0x9E(reg, mem);
            break;

        case 0x9F:
            execute0x9F(reg);
            break;

        case 0xA0:
            execute0xA0(reg);
            break;

        case 0xA1:
            execute0xA1(reg);
            break;

        case 0xA2:
            execute0xA2(reg);
            break;

        case 0xA3:
            execute0xA3(reg);
            break;

        case 0xA4:
            execute0xA4(reg);
            break;

        case 0xA5:
            execute0xA5(reg);
            break;

        case 0xA6:
            execute0xA6(reg, mem);
            break;

        case 0xA7:
            execute0xA7(reg);
            break;

        case 0xA8:
            execute0xA8(reg);
            break;

        case 0xA9:
            execute0xA9(reg);
            break;

        case 0xAA:
            execute0xAA(reg);
            break;

        case 0xAB:
            execute0xAB(reg);
            break;

        case 0xAC:
            execute0xAC(reg);
            break;

        case 0xAD:
            execute0xAD(reg);
            break;

        case 0xAE:
            execute0xAE(reg, mem);
            break;

        case 0xAF:
            execute0xAF(reg);
            break;

        case 0xB0:
            execute0xB0(reg);
            break;

        case 0xB1:
            execute0xB1(reg);
            break;

        case 0xB2:
            execute0xB2(reg);
            break;

        case 0xB3:
            execute0xB3(reg);
            break;

        case 0xB4:
            execute0xB4(reg);
            break;

        case 0xB5:
            execute0xB5(reg);
            break;

        case 0xB6:
            execute0xB6(reg, mem);
            break;

        case 0xB7:
            execute0xB7(reg);
            break;

        case 0xB8:
            execute0xB8(reg);
            break;

        case 0xB9:
            execute0xB9(reg);
            break;

        case 0xBA:
            execute0xBA(reg);
            break;

        case 0xBB:
            execute0xBB(reg);
            break;

        case 0xBC:
            execute0xBC(reg);
            break;

        case 0xBD:
            execute0xBD(reg);
            break;

        case 0xBE:
            execute0xBE(reg, mem);
            break;

        case 0xBF:
            execute0xBF(reg);
            break;

        case 0xC0:
            execute0xC0(reg, mem);
            break;

        case 0xC1:
            execute0xC1(reg, mem);
            break;

        case 0xC2:
            execute0xC2(reg, mem);
            break;

        case 0xC3:
            execute0xC3(reg, mem);
            break;

        case 0xC4:
            execute0xC4(reg, mem);
            break;

        case 0xC5:
            execute0xC5(reg, mem);
            break;

        case 0xC6:
            execute0xC6(reg, mem);
            break;

        case 0xC7:
            execute0xC7(reg, mem);
            break;

        case 0xC8:
            execute0xC8(reg, mem);
            break;

        case 0xC9:
            execute0xC9(reg, mem);
            break;

        case 0xCA:
            execute0xCA(reg, mem);
            break;

        case 0xCB:
            execute0xCB(reg, mem);
            break;

        case 0xCC:
            execute0xCC(reg, mem);
            break;

        case 0xCD:
            execute0xCD(reg, mem);
            break;

        case 0xCE:
            execute0xCE(reg, mem);
            break;

        case 0xCF:
            execute0xCF(reg, mem);
            break;

        case 0xD0:
            execute0xD0(reg, mem);
            break;

        case 0xD1:
            execute0xD1(reg, mem);
            break;

        case 0xD2:
            execute0xD2(reg, mem);
            break;

        case 0xD4:
            execute0xD4(reg, mem);
            break;

        case 0xD5:
            execute0xD5(reg, mem);
            break;

        case 0xD6:
            execute0xD6(reg, mem);
            break;

        case 0xD7:
            execute0xD7(reg, mem);
            break;

        case 0xD8:
            execute0xD8(reg, mem);
            break;

        case 0xD9:
            execute0xD9(reg, mem);
            break;

        case 0xDA:
            execute0xDA(reg, mem);
            break;

        case 0xDC:
            execute0xDC(reg, mem);
            break;

        case 0xDE:
            execute0xDE(reg, mem);
            break;

        case 0xDF:
            execute0xDF(reg, mem);
            break;

        case 0xE0:
            execute0xE0(reg, mem);
            break;

        case 0xE1:
            execute0xE1(reg, mem);
            break;

        case 0xE2:
            execute0xE2(reg, mem);
            break;

        case 0xE5:
            execute0xE5(reg, mem);
            break;

        case 0xE6:
            execute0xE6(reg, mem);
            break;

        case 0xE7:
            execute0xE7(reg, mem);
            break;

        case 0xE8:
            execute0xE8(reg, mem);
            break;

        case 0xE9:
            execute0xE9(reg);
            break;

        case 0xEA:
            execute0xEA(reg, mem);
            break;

        case 0xEE:
            execute0xEE(reg, mem);
            break;

        case 0xEF:
            execute0xEF(reg, mem);
            break;

        case 0xF0:
            execute0xF0(reg, mem);
            break;

        case 0xF1:
            execute0xF1(reg, mem);
            break;

        case 0xF2:
            execute0xF2(reg, mem);
            break;

        case 0xF3:
            execute0xF3(reg);
            break;

        case 0xF5:
            execute0xF5(reg, mem);
            break;

        case 0xF6:
            execute0xF6(reg, mem);
            break;

        case 0xF7:
            execute0xF7(reg, mem);
            break;

        case 0xF8:
            execute0xF8(reg, mem);
            break;

        case 0xF9:
            execute0xF9(reg);
            break;

        case 0xFA:
            execute0xFA(reg, mem);
            break;

        case 0xFB:
            execute0xFB(reg);
            break;

        case 0xFE:
            execute0xFE(reg, mem);
            break;

        case 0xFF:
            execute0xFF(reg, mem);
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
    writeMemory(mem, 0x0100, 0x00);  // NOP
    writeMemory(mem, 0x0101, 0x06);  // LD B, n
    writeMemory(mem, 0x0102, 0x42);  // Bに入れる値
    writeMemory(mem, 0x0103, 0x00);  // NOP

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