#include "tms9900-memory.h"
#include <stdlib.h>

uint8_t external_read_byte(uint16_t address);
uint16_t external_read_word(uint16_t address);
void external_write_byte(uint16_t address, uint8_t byte);
void external_write_word(uint16_t address, uint16_t word);

/******************************************************************************
 * memory callback routines
 */
uint8_t read_memory_byte(struct TMS9900 *cpu, uint16_t address)
{
    if ((address >> 10) == 0x20) {         // scratchpad memory
        uint8_t base = address & 0xff;
        return cpu->RAM[base];
    }

    if (address < 0x2000)                 // ROM in PROGMEM
        return cpu->ROM[address];

    // cartridge
    if (address >= 0x6000 && address < 0x8000 && cpu->cartRAM) {
        cpu->cycles += 4;
        return cpu->cartRAM[address - 0x6000];
    }

    // external memory - call out
    cpu->cycles += 4;
    return external_read_byte(address);
}

uint16_t read_memory_word(struct TMS9900 *cpu, uint16_t address)
{
    if ((address >> 10) == 0x20) {         // scratchpad memory
        uint8_t base = address & 0xff;
        return (cpu->RAM[base] << 8) | cpu->RAM[base+1];
    }

    if (address < 0x2000) {               // ROM in PROGMEM
        uint16_t word = *((uint16_t *)(cpu->ROM + address));
        return BSWAP(word);
    }

    // cartridge
    if (address >= 0x6000 && address < 0x8000 && cpu->cartRAM) {
        uint16_t word = *((uint16_t *)(cpu->cartRAM + address - 0x6000));
        cpu->cycles += 4;
        return BSWAP(word);
    }

    // external memory - call out
    cpu->cycles += 4;
    return external_read_word(address);
}


void write_memory_byte(struct TMS9900 *cpu, uint16_t address, uint8_t byte)
{
    if ((address >> 10) == 0x20) {
        uint8_t base = address & 0xff;
        cpu->RAM[base] = byte;
        return;
    }

    // external memory - call out
    cpu->cycles += 4;
    external_write_byte(address, byte);
}


void write_memory_word(struct TMS9900 *cpu, uint16_t address, uint16_t word)
{
    if ((address >> 10) == 0x20) {
        uint8_t base = address & 0xff;
        cpu->RAM[base+1] = word & 0xff;
        cpu->RAM[base] = word >> 8;
        return;
    }

    cpu->cycles += 4;
    external_write_word(address, word);
}
