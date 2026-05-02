#include <iostream>
#include <cstdint>

// ============================================
// レジスタ定義
// ============================================
struct Registers {
    uint8_t A = 0x01;
    uint8_t B = 0x00;
    uint8_t C = 0x13;
    uint8_t D = 0x00;
    uint8_t E = 0xD8;
    uint8_t H = 0x01;
    uint8_t L = 0x4D;
    uint8_t F = 0xB0;  // フラグレジスタ (bit7=Z, bit6=N, bit5=H, bit4=C)
    uint16_t PC = 0x0100;
    uint16_t SP = 0xFFFE;
};

// ============================================
// メモリ定義
// ============================================
struct Memory {
    uint8_t data[65536] = {};  // 0x0000〜0xFFFF を全部0で初期化
};

// ============================================
// メモリ読み書き
// ============================================
uint8_t readMemory(Memory& mem, uint16_t address) {
    return mem.data[address];
}

void writeMemory(Memory& mem, uint16_t address, uint8_t value) {
    mem.data[address] = value;
}

// ============================================
// フラグ操作ヘルパー
// ============================================
void setFlag(Registers& reg, uint8_t flag, bool value) {
    if (value) reg.F |= flag;
    else       reg.F &= ~flag;
}

bool getFlag(Registers& reg, uint8_t flag) {
    return (reg.F & flag) != 0;
}

// フラグのビット位置
const uint8_t FLAG_Z = 0x80;  // bit7
const uint8_t FLAG_N = 0x40;  // bit6
const uint8_t FLAG_H = 0x20;  // bit5
const uint8_t FLAG_C = 0x10;  // bit4

// ============================================
// 命令の実装
// ============================================

// 0x00: NOP - 何もしない
void executeNOP(Registers& reg) {
    reg.PC += 1;
}

// 0x06: LD B, n - 次のバイトの値をBに入れる
void executeLD_B_n(Registers& reg, Memory& mem) {
    reg.B = mem.data[reg.PC + 1];
    reg.PC += 2;
}

// 0x0E: LD C, n - 次のバイトの値をCに入れる
void executeLD_C_n(Registers& reg, Memory& mem) {
    reg.C = mem.data[reg.PC + 1];
    reg.PC += 2;
}

// 0x3E: LD A, n - 次のバイトの値をAに入れる
void executeLD_A_n(Registers& reg, Memory& mem) {
    reg.A = mem.data[reg.PC + 1];
    reg.PC += 2;
}

// 0x78: LD A, B - Bの値をAに入れる
void executeLD_A_B(Registers& reg) {
    reg.A = reg.B;
    reg.PC += 1;
}

// 0x80: ADD A, B - AにBを足す
void executeADD_A_B(Registers& reg) {
    uint16_t result = reg.A + reg.B;
    setFlag(reg, FLAG_Z, (result & 0xFF) == 0);       // 結果が0ならZ
    setFlag(reg, FLAG_N, false);                       // 足し算なのでNは0
    setFlag(reg, FLAG_H, ((reg.A & 0x0F) + (reg.B & 0x0F)) > 0x0F);  // Hフラグ
    setFlag(reg, FLAG_C, result > 0xFF);               // Cフラグ
    reg.A = result & 0xFF;
    reg.PC += 1;
}

// 0xC3: JP a16 - 指定アドレスにジャンプ
void executeJP(Registers& reg, Memory& mem) {
    uint16_t address = mem.data[reg.PC + 1] | (mem.data[reg.PC + 2] << 8);
    reg.PC = address;
}

// ============================================
// CPUの1サイクル
// ============================================
void step(Registers& reg, Memory& mem) {
    uint8_t opcode = mem.data[reg.PC];  // PCが指す命令を読む

    switch (opcode) {
        case 0x00: executeNOP(reg);        break;
        case 0x06: executeLD_B_n(reg, mem); break;
        case 0x0E: executeLD_C_n(reg, mem); break;
        case 0x3E: executeLD_A_n(reg, mem); break;
        case 0x78: executeLD_A_B(reg);      break;
        case 0x80: executeADD_A_B(reg);     break;
        case 0xC3: executeJP(reg, mem);     break;
        default:
            std::cout << "未実装: 0x" << std::hex << (int)opcode
                      << " at PC=0x" << reg.PC << std::endl;
            reg.PC += 1;
            break;
    }
}

// ============================================
// デバッグ出力
// ============================================
void printState(Registers& reg) {
    std::cout << "PC=0x" << std::hex << reg.PC
              << " A=0x"  << (int)reg.A
              << " B=0x"  << (int)reg.B
              << " C=0x"  << (int)reg.C
              << " F=0x"  << (int)reg.F
              << std::endl;
}

// ============================================
// テスト用main
// ============================================
int main() {
    Registers reg;
    Memory mem;

    // テストプログラム: 1 + 1 を計算する
    reg.PC = 0x0100;
    mem.data[0x0100] = 0x3E;  // LD A, 1
    mem.data[0x0101] = 0x01;
    mem.data[0x0102] = 0x06;  // LD B, 1
    mem.data[0x0103] = 0x01;
    mem.data[0x0104] = 0x80;  // ADD A, B
    mem.data[0x0105] = 0x00;  // NOP

    std::cout << "=== 初期状態 ===" << std::endl;
    printState(reg);

    for (int i = 0; i < 5; i++) {
        step(reg, mem);
        printState(reg);
    }
    printState(reg);
    return 0;
}