#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *REG16[8] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
const char *REG8[8] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
const char *EA_TABLE[8] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};

// Read a file into a buffer and return its length
size_t read_binary(const char *filename, uint8_t **buffer)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Failed to open binary file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    rewind(file);

    *buffer = malloc(length);
    if (!*buffer)
    {
        perror("Memory allocation failed");
        exit(1);
    }

    fread(*buffer, 1, length, file);
    fclose(file);
    return length;
}

void disassemble_mov(const uint8_t *code, size_t *i, size_t length)
{
    uint8_t opcode = code[*i];
    (*i)++;

    /* Handle accumulator memory-mov */
    if (opcode == 0xA1 || opcode == 0xA3)
    {
        if (*i + 1 >= length)
        {
            printf("; Unexpected EOF\n");
            return;
        }

        uint16_t addr = code[*i] | (code[*i + 1] << 8);
        *i += 2;

        if (opcode == 0xA1)
        {
            printf("\tmov ax, [%u]\n", addr);
        }
        else
        {
            printf("\tmov [%u], ax\n", addr);
        }
        return;
    }

    /* Immediate to register */
    if (opcode >= 0xB0 && opcode <= 0xBF)
    {
        uint8_t reg = opcode & 0x7;
        uint8_t w = (opcode & 0x8) >> 3;
        if (*i >= length)
        {
            printf("; Unexpected EOF\n");
            return;
        }

        int16_t imm;
        if (w == 0)
        {
            imm = (int8_t)code[*i];
            (*i)++;
            printf("    mov %s, %d\n", REG8[reg], imm);
        }
        else
        {
            if (*i + 1 >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            imm = code[*i] | (code[*i + 1] << 8);
            (*i) += 2;
            printf("    mov %s, %d\n", REG16[reg], imm);
        }
        return;
    }

    /* Immediate to memory/register */
    if (opcode == 0xC6 || opcode == 0xC7)
    {
        if (*i >= length)
        {
            printf("; Unexpected EOF\n");
            return;
        }

        uint8_t modrm = code[*i];
        (*i)++;

        int mod = (modrm & 0xC0) >> 6;
        int rm = (modrm & 0x07);

        int16_t disp = 0;
        if (mod == 0b01)
        {
            if (*i >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            disp = (int8_t)code[*i];
            (*i)++;
        }
        else if (mod == 0b10 || (mod == 0 && rm == 0b110))
        {
            if (*i + 1 >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            disp = code[*i] | (code[*i + 1] << 8);
            (*i) += 2;
        }

        char mem[64] = {0};
        if (mod == 0 && rm == 0b110)
        {
            snprintf(mem, sizeof(mem), "[%d]", disp);
        }
        else if (disp == 0)
        {
            snprintf(mem, sizeof(mem), "[%s]", EA_TABLE[rm]);
        }
        else
        {
            snprintf(mem, sizeof(mem), "[%s %+d]", EA_TABLE[rm], disp);
        }

        if (opcode == 0xC6)
        {
            if (*i >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            int8_t imm = (int8_t)code[*i];
            (*i)++;
            printf("    mov %s, byte %d\n", mem, imm);
        }
        else
        { // C7
            if (*i + 1 >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            int16_t imm = code[*i] | (code[*i + 1] << 8);
            (*i) += 2;
            printf("    mov %s, word %d\n", mem, imm);
        }
        return;
    }

    /* mov register/memory to/from register */
    if (opcode >= 0x88 && opcode <= 0x8B)
    {
        if (*i >= length)
        {
            printf("; Unexpected EOF\n");
            return;
        }

        uint8_t modrm = code[*i];
        (*i)++;

        int d = (opcode & 0x2) >> 1;
        int w = opcode & 0x1;

        int mod = (modrm & 0xC0) >> 6;
        int reg = (modrm & 0x38) >> 3;
        int rm = (modrm & 0x07);

        int16_t disp = 0;
        if (mod == 0b01)
        { // 8-bit signed displacement
            if (*i >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            disp = (int8_t)code[*i];
            (*i)++;
        }
        else if (mod == 0b10 || (mod == 0 && rm == 0b110))
        { // 16-bit displacement
            if (*i + 1 >= length)
            {
                printf("; Unexpected EOF\n");
                return;
            }
            disp = code[*i] | (code[*i + 1] << 8);
            (*i) += 2;
        }

        char mem[64] = {0};
        if (mod == 0b11)
        {
            // Register-to-register
            const char *src = w ? REG16[reg] : REG8[reg];
            const char *dst = w ? REG16[rm] : REG8[rm];
            if (d)
                printf("    mov %s, %s\n", dst, src);
            else
                printf("    mov %s, %s\n", src, dst);
            return;
        }
        else
        {
            // Memory operand
            if (mod == 0 && rm == 0b110)
            {
                snprintf(mem, sizeof(mem), "[%d]", disp);
            }
            else if (disp == 0)
            {
                snprintf(mem, sizeof(mem), "[%s]", EA_TABLE[rm]);
            }
            else
            {
                snprintf(mem, sizeof(mem), "[%s %+d]", EA_TABLE[rm], disp);
            }

            const char *regstr = w ? REG16[reg] : REG8[reg];
            if (d)
                printf("    mov %s, %s\n", regstr, mem); // reg ← mem
            else
                printf("    mov %s, %s\n", mem, regstr); // mem ← reg
            return;
        }
    }

    printf("; Unsupported opcode: 0x%02X\n", opcode);
}

void disassemble(const uint8_t *code, size_t length)
{
    printf("bits 16\n\n");
    size_t i = 0;
    while (i < length)
    {
        disassemble_mov(code, &i, length);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    uint8_t *buffer = NULL;
    size_t length = read_binary(argv[1], &buffer);
    disassemble(buffer, length);
    free(buffer);
    return 0;
}
