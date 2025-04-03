#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char* REG16[8] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
const char* REG8[8]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

// Read a file into a buffer and return its length
size_t read_binary(const char* filename, uint8_t** buffer) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open binary file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    rewind(file);

    *buffer = malloc(length);
    if (!*buffer) {
        perror("Memory allocation failed");
        exit(1);
    }

    fread(*buffer, 1, length, file);
    fclose(file);
    return length;
}

void disassemble_mov_reg_to_reg(uint8_t opcode, uint8_t modrm) {
    int d = (opcode & 0x2) >> 1;
    int w = opcode & 0x1;

    int mod = (modrm & 0xC0) >> 6;
    int reg = (modrm & 0x38) >> 3;
    int rm  = modrm & 0x07;

    if (mod != 0b11) {
        printf("; Skipping non register-to-register mov instruction\n");
        return;
    }

    /* Checks if it is 16-bit or 8-bit */
    const char* src = w ? REG16[reg] : REG8[reg];
    const char* dst = w ? REG16[rm]  : REG8[rm];

    if (d) {
        printf("\tmov %s, %s\n", dst, src); // reg <- r/m
    } else {
        printf("\tmov %s, %s\n", src, dst); // r/m <- reg
    }
}

void disassemble(const uint8_t* code, size_t length) {
    printf("bits 16\n\n");
    size_t i = 0;
    while (i < length) {
        uint8_t byte = code[i];

        // MOV register-to-register (8A / 88 or 8B / 89)
        if ((byte >= 0x88 && byte <= 0x8B)) {
            if (i + 1 >= length) {
                fprintf(stderr, "Unexpected EOF after opcode\n");
                break;
            }
            disassemble_mov_reg_to_reg(byte, code[i + 1]);
            i += 2;
        } else {
            printf("; Unsupported opcode: 0x%02X\n", byte);
            i++;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    uint8_t* buffer = NULL;
    size_t length = read_binary(argv[1], &buffer);
    disassemble(buffer, length);
    free(buffer);
    return 0;
}
