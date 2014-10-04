#ifndef TMS9900_MEMORY_H
#define TMS9900_MEMORY_H

#include "tms9900-core.h"

#define WORD_MEMORY_READ(A, address)       (((uint16_t)A[address] << 8) | (uint16_t)A[address+1])


// external memory callbacks for the core
uint8_t read_memory_byte(struct TMS9900 *cpu, uint16_t address);
uint16_t read_memory_word(struct TMS9900 *cpu, uint16_t address);
void write_memory_byte(struct TMS9900 *cpu, uint16_t address, uint8_t byte);
void write_memory_word(struct TMS9900 *cpu, uint16_t address, uint16_t word);

#endif // TMS9900_MEMORY_H
