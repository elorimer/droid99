#include "ti99-fdc.h"
#include "tms9900-core.h"
#include "tms9900-memory.h"
#include "opcodes.h"
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>

void LuaTraceCPU(uint16_t id);
void LuaBranchTrace(uint16_t new_pc);

/*
 * TI99 floppy disk controller ROM
 */
static uint8_t FDCROM[8192];


extern uint16_t get_grom();
/**
 * TMS9900 instruction decoder
 */

// Addressing macros
#define WR(n)       ((cpu->WP & 0xFF) + ((n) << 1))
#define REGADDR(n)  (cpu->WP + ((n) << 1))

#define ASSERT(x)

// status bit macros
#define LGT_BIT         15          // ST0
#define AGT_BIT         14          // ST1
#define EQUAL_BIT       13          // ST2
#define CARRY_BIT       12          // ST3
#define OVERFLOW_BIT    11          // ST4
#define PARITY_BIT      10          // ST5
#define SET_STATUS_BIT(x)       (cpu->ST |= (1 << (x)))
#define CLEAR_STATUS_BIT(x)     (cpu->ST &= ~(1 << (x)))

#define CLEAR_CARRY()       (cpu->ST &= ~(1 << CARRY_BIT))
#define SET_CARRY(x)        (cpu->ST |= ((x) << CARRY_BIT))
#define CLEAR_OVERFLOW()    (cpu->ST &= ~(1 << OVERFLOW_BIT))
#define SET_OVERFLOW(x)     (cpu->ST |= ((x) << OVERFLOW_BIT))
#define GET_STATUS_BIT(x)   ((cpu->ST >> x) & 0x1)


#define TRACEFILE       "/mnt/sdcard/and99.trace"


/*
 * Base cycle timings for each instruction.  See the TMS9900 Data Manual.
 *
 * Some opcodes have to be treated specially:
 * - DIV, LDCR, shifts, STCR, jumps
 */
static base_cycles[NUM_INSTRUCTIONS] =
                         { 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,      // 12
                           14, 14, 52, 16, 36, 8, 12, 26, 10, 10, 10, 12, 12,       // 25
                           10, 10, 10, 10, 10, 8, 20, 42, 12, 12, 12, 8, 8, 8,      // 39
                           8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12, 12, 14, 14,    // 55
                           14, 12, 14, 10, 16, 8, 8, 14, 12, 26, 12, 12, 12 };


static void read_replay(struct TMS9900 *cpu, struct ReplayRecord *event)
{
    static uint32_t nEvents = 0;

    fread(event, sizeof(event->type) + sizeof(event->icount) + sizeof(event->PC), 1, cpu->trace);
    if (event->type == KEYBOARD)
        fread(&event->data, sizeof(event->data), 1, cpu->trace);
    nEvents++;
}


static void CRU_write_bit(struct TMS9900* cpu, uint16_t CRU_addr, int val)
{
    if (CRU_addr == 0x15) {
        cpu->keyboard.alpha = val;
        return;
    }

    // VDP DSR echoing interrupt
    if (CRU_addr == 0x2)
        return;

    if  (CRU_addr >= 0x1100 && CRU_addr <= 0x1110) {
        // turn on/off ROM at 0x4000 ...
        if (CRU_addr == 0x1100)
            cpu->peripheral_mem = (val) ? FDCROM : 0x0;

        fdc_cru_writebit(CRU_addr, val);
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, "And99", "CRU write bit: 0x%x = %d", CRU_addr, val);
}


static int CRU_read_bit(struct TMS9900* cpu, uint16_t CRU_addr)
{
    if (CRU_addr == 0x02) {
        // VDP interrupt
        int s = (cpu->vdp_int) ? 0 : 1;     // inverted
        cpu->vdp_int = 0;
        return s;
    }

    if (CRU_addr == 0x07)
        return cpu->keyboard.alpha;

    __android_log_print(ANDROID_LOG_INFO, "And99", "CRU read bit: 0x%x\n", CRU_addr);

    return 0;
}


/*
 * 1 on success, 0 on fail.  initializes the cpu structure and loads the ROM
 * from the filename
 */
int TMS9900_Create(struct TMS9900 *cpu, const char *rom, uint8_t _replay)
{
    // initialize cpu (could read this from ROM)
    cpu->PC = 0x0012;
    cpu->WP = 0x83E0;
    cpu->ST = 0x0000;
    cpu->interrupt = 0;
    cpu->vdp_int = 0;
    cpu->icount = 0;
    cpu->replay = 0;
    if (_replay) {
        cpu->replay = 1;
        cpu->trace = fopen(TRACEFILE, "r");
    }
    else
        cpu->trace = fopen(TRACEFILE, "w");

    if (cpu->trace == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "And99", "FAILED to open trace file");
    }

    if (cpu->replay)
        read_replay(cpu, &cpu->next_event);

    memset(cpu->RAM, 0x00, 256);
    memset(cpu->expansion_mem, 0x00, 32768);
    cpu->peripheral_mem = 0x0;
    
    // keyboard
    pthread_mutex_init(&cpu->keyboard.lock, NULL);
    memset(cpu->keyboard.keys, 0xFF, sizeof(cpu->keyboard.keys));
    cpu->keyboard.column = 0;
    cpu->keyboard.alpha = 0;
    cpu->keyboard.queued = 0;

    // cartridge
    cpu->cartridge = 0x00;          // no cartridge
    cpu->cartRAM = 0x00;

    // ROM
    cpu->ROM = (uint8_t *)malloc(8192);
    FILE *romFile = fopen(rom, "r");
    if (!romFile) goto error;
    fread(cpu->ROM, 1, 8192, romFile);
    fclose(romFile);

//    pthread_mutex_init(&cpu->lock, NULL);
    return 1;

error:
    free(cpu->ROM);
    return 0;
}


void TMS9900_Destroy(struct TMS9900 *cpu)
{
    if (cpu && cpu->ROM) {
        free(cpu->ROM);
        free(cpu->cartridge);
        free(cpu->cartRAM);
        cpu->ROM = NULL;
        cpu->cartridge = NULL;
        cpu->cartRAM = NULL;
    }
}


// should take a replay flag
void TMS9900_Reset(struct TMS9900 *cpu)
{
    fclose(cpu->trace);

    // initialize cpu (could read this from ROM)
    cpu->PC = 0x0012;
    cpu->WP = 0x83E0;
    cpu->ST = 0x0000;
    cpu->interrupt = 0;
    cpu->vdp_int = 0;
    cpu->icount = 0;
    cpu->replay = 0;
    cpu->trace = fopen(TRACEFILE, "w");

    if (cpu->trace == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "And99", "FAILED to open trace file");
    }

    memset(cpu->RAM, 0x00, 256);
    memset(cpu->expansion_mem, 0x00, 32768);
    cpu->peripheral_mem = 0x0;
    
    // keyboard
    memset(cpu->keyboard.keys, 0xFF, sizeof(cpu->keyboard.keys));
    cpu->keyboard.column = 0;
    cpu->keyboard.alpha = 0;
    cpu->keyboard.queued = 0;

    // cartridge
    cpu->cartridge = 0x00;          // no cartridge
    cpu->cartRAM = 0x00;
}


/* Caller allocates buffer for saving image but he needs to know how big a
   buffer to allocate ... */
int TMS9900_GetImageSize()
{
    // TODO: how to make this match SaveImage without manually recalculating
    return 266;
}


// make sure buffer has space for 276 bytes
void TMS9900_SaveImage(struct TMS9900 *cpu, uint8_t *buffer)
{
    uint8_t *p = buffer;
    memcpy(p, &cpu->PC, sizeof(cpu->PC));
    p += sizeof(cpu->PC);
    memcpy(p, &cpu->WP, sizeof(cpu->WP));
    p += sizeof(cpu->WP);
    memcpy(p, &cpu->ST, sizeof(cpu->ST));
    p += sizeof(cpu->ST);
    memcpy(p, cpu->RAM, sizeof(cpu->RAM));
    p += sizeof(cpu->RAM);
    memcpy(p, &cpu->icount, sizeof(cpu->icount));
    p += sizeof(cpu->icount);
/*
    *p++ = cpu->interrupt;
    *p++ = cpu->vdp_int;

    *p++ = cpu->keyboard.alpha;
    *p++ = cpu->keyboard.column;
    memcpy(p, cpu->keyboard.keys, sizeof(cpu->keyboard.keys));
    p += sizeof(cpu->keyboard.keys);
*/

    // other variables
    /*
    uint8_t     *ROM;
    uint8_t     *cartridge;

    FILE        *trace;     // trace file to read/write
    uint8_t     replay;     // boolean: replay the trace file
    struct ReplayRecord next_event;
    */
}


/*
 * TODO: Fix this to work with replay.  For now, disable replay
 */
void TMS9900_LoadImage(struct TMS9900 *cpu, uint8_t *buffer)
{
    uint16_t *wordp = (uint16_t *)buffer;
    cpu->PC = wordp[0];
    cpu->WP = wordp[1];
    cpu->ST = wordp[2];
    memcpy(cpu->RAM, buffer+6, 256);
    memcpy(&cpu->icount, buffer+262, sizeof(cpu->icount));

    // initialize fields we didn't save
    cpu->interrupt = 0;
    cpu->vdp_int = 0;
}


void TMS9900_Keyboard(struct TMS9900 *cpu, uint8_t row, uint8_t col, uint8_t state)
{
    /*
    if (!cpu->replay) {
        struct ReplayRecord r;
        r.type = KEYBOARD;
        pthread_mutex_lock(&cpu->lock);
        r.icount = cpu->icount;
        r.PC = cpu->PC;
        pthread_mutex_unlock(&cpu->lock);
        r.data[0] = row;
        r.data[1] = col;
        r.data[2] = state;
//        __android_log_print(ANDROID_LOG_INFO, "And99", "write event: %d, type = 1", r.icount);
        fwrite(&r, sizeof(r), 1, cpu->trace);
        fflush(cpu->trace);
    } else {
        read_replay(cpu, &cpu->next_event);
    }
    */

    // invert state since TI 99 decodes bit set as key not pressed
    state = (state > 0) ? 0 : 1;
    pthread_mutex_lock(&cpu->keyboard.lock);
    uint8_t i = cpu->keyboard.queued;
    cpu->keyboard.data[i][0] = row;
    cpu->keyboard.data[i][1] = col;
    cpu->keyboard.data[i][2] = state;
    cpu->keyboard.queued++;
    // ASSERT queued < queue depth
    pthread_mutex_unlock(&cpu->keyboard.lock);
}


// some internal functions now
static inline void setLAE(struct TMS9900 *cpu, uint16_t result)
{
    uint8_t bits = 0;
    if (result != 0) {
        bits |= 0x4;
        if (result >> 15 == 0)
            bits |= 0x2;
    } else
        bits |= 0x1;

    cpu->ST = (cpu->ST & 0x1FFF) | (bits << 13);
}


static inline void setParity(struct TMS9900 *cpu, uint16_t sa)
{
    uint8_t ones = 0;
    while (sa) {
        sa = sa & (sa-1);
        ones++;
    }
    cpu->ST &= ~(1 << 10);
    if (ones % 2 == 1)
        cpu->ST |= (1 << 10);
}


static inline uint16_t addressOf(struct TMS9900 *cpu, uint8_t type, uint8_t x, uint8_t size)
{
    uint16_t mask = 0xFFFF;
    if (size == 2)
        mask = 0xFFFE;

    switch (type) {
        case 0x00:              // workspace register (direct)
            return (cpu->WP + (x << 1)) & mask;
        case 0x01:              // workspace register (indirect)
            cpu->cycles += 4;
            return ((cpu->RAM[WR(x)] << 8) | cpu->RAM[WR(x)+1]) & mask;
        case 0x02:
            cpu->cycles += 4;
            if (x != 0) {
                // indexed
                cpu->PC += 1;
                uint16_t lookup = read_memory_word(cpu, cpu->PC << 1);
                uint16_t index = (cpu->RAM[WR(x)] << 8) | cpu->RAM[WR(x)+1];
                return (lookup + index) & mask;
            }
            // symbolic
            cpu->PC += 1;
            uint16_t label = read_memory_word(cpu, cpu->PC << 1);
            return label & mask;
        case 0x03: {             // workspace register (indirect, auto-inc) 
            uint16_t result = ((cpu->RAM[WR(x)] << 8) | cpu->RAM[WR(x)+1]);
            write_memory_word(cpu, cpu->WP + (x << 1), result + size);
            if (size == 1)
                cpu->cycles += 6;
            else
                cpu->cycles += 8;
            return result;
        }
        default:
            ASSERT(0);
            break;
    }
}



void TMS9900_DumpRegisters(struct TMS9900 *cpu, char *buffer, int buflen)
{
    uint16_t insn = read_memory_word(cpu, cpu->PC << 1);
    uint8_t id = table[(insn & 0xffe0) >> 5];

    uint8_t i = 0;
    uint16_t *x = (uint16_t *)(&cpu->RAM[cpu->WP & 0xFF]);
    int len;
    if (id < 69)
        len = snprintf(buffer, buflen, "PC: 0x%x (%s)\tWP: 0x%x\tST: 0x%x\r\n", cpu->PC << 1, mnemonics[id], cpu->WP, cpu->ST);
    else
        len = snprintf(buffer, buflen, "PC: 0x%x (INVALID - %d)\tWP: 0x%x\tST: 0x%x\r\n", cpu->PC << 1, id, cpu->WP, cpu->ST);
    buffer += len;
    buflen -= len;
    for (i = 0; i < 16; i+=4) {
        len = snprintf(buffer, buflen , "R%d => 0x%x\tR%d => 0x%x\tR%d => 0x%x\tR%d => 0x%x\r\n", i, BSWAP(x[i]), i+1, BSWAP(x[i+1]), i+2, BSWAP(x[i+2]), i+3, BSWAP(x[i+3]));
        buffer += len;
        buflen -= len;
    }
}


void TMS9900_Interrupt(struct TMS9900 *cpu)
{
    uint16_t oldWP = cpu->WP;
    // read from interrupt vector
    cpu->WP = 0x83c0;
    write_memory_word(cpu, cpu->WP + 13 * 2, oldWP);
    write_memory_word(cpu, cpu->WP + 14 * 2, cpu->PC << 1);
    write_memory_word(cpu, cpu->WP + 15 * 2, cpu->ST);

    if (!cpu->replay) {
        // need a corresponding write_replay
        struct ReplayRecord r;
        r.icount = cpu->icount;
        r.type = INTERRUPT;
        r.PC = cpu->PC;
//        __android_log_print(ANDROID_LOG_INFO, "And99", "write_event %d, type = 0", r.icount);
        fwrite(&r, sizeof(r.type) + sizeof(r.icount) + sizeof(r.PC), 1, cpu->trace);
        fflush(cpu->trace);
    }
    else {
        cpu->vdp_int = 1;       // all interrupts are VDP TODO: save interrupt
                                // line in replay log
        read_replay(cpu, &cpu->next_event);
    }

    // TODO: read this from interrupt vector
    cpu->PC = 0x900 >> 1;
    cpu->interrupt = 0;
}



/*
 * Step one instruction; return number of cycles taken
 */
int TMS9900_StepCPU(struct TMS9900 *cpu)
{
    // If we hit an exception, spin and let the debugger attach
    if (cpu->exception)
        return 0;

    // handle interrupts
    if (cpu->replay && cpu->icount == cpu->next_event.icount) {
        if (cpu->PC != cpu->next_event.PC) {
            __android_log_print(ANDROID_LOG_ERROR, "And99", "Replay ERROR: 0x%x != 0x%x", cpu->PC, cpu->next_event.PC);
            exit(0);
        }
        if (cpu->next_event.type == INTERRUPT)
            TMS9900_Interrupt(cpu);
        else if (cpu->next_event.type == KEYBOARD) {
            uint8_t row = cpu->next_event.data[0];
            uint8_t col = cpu->next_event.data[1];
            uint8_t state = cpu->next_event.data[2];
            pthread_mutex_lock(&cpu->keyboard.lock);
            cpu->keyboard.keys[col] &= ~(1 << row);
            cpu->keyboard.keys[col] |= (state << row);
            pthread_mutex_unlock(&cpu->keyboard.lock);
            read_replay(cpu, &cpu->next_event);
//            TMS9900_Keyboard(cpu, cpu->next_event.data[0], cpu->next_event.data[1],
//                             cpu->next_event.data[2]);
        }
    } else {
        if (cpu->interrupt && ((cpu->ST & 0xf) >= 0x2))
            TMS9900_Interrupt(cpu);
    }

    // handle queued keyboard events
    pthread_mutex_lock(&cpu->keyboard.lock);
    while (cpu->keyboard.queued) {
//    if (cpu->keyboard.queued) {
        uint8_t i = cpu->keyboard.queued - 1;
        uint8_t row = cpu->keyboard.data[i][0];
        uint8_t col = cpu->keyboard.data[i][1];
        uint8_t state = cpu->keyboard.data[i][2];
        cpu->keyboard.keys[col] &= ~(1 << row);
        cpu->keyboard.keys[col] |= (state << row);
        cpu->keyboard.queued--;
        if (!cpu->replay) {
            struct ReplayRecord r;
            r.type = KEYBOARD;
            r.icount = cpu->icount;
            r.PC = cpu->PC;
            r.data[0] = cpu->keyboard.data[i][0];
            r.data[1] = cpu->keyboard.data[i][1];
            r.data[2] = cpu->keyboard.data[i][2];
            fwrite(&r, sizeof(r), 1, cpu->trace);
            fflush(cpu->trace);
        }
        /*
        else {
            read_replay(cpu, &cpu->next_event);
        }
        */
    }
    pthread_mutex_unlock(&cpu->keyboard.lock);

    cpu->cycles = 0;            // reset cycles and accumulate over one instruction
    uint16_t insn = read_memory_word(cpu, cpu->PC << 1);
XLABEL: ;
    uint8_t id = table[(insn & 0xffe0) >> 5];

    cpu->icounts[id]++;
    LuaTraceCPU(id);
    switch (id) {
        case RTWP: {
            cpu->ST = read_memory_word(cpu, REGADDR(15));
            cpu->PC = read_memory_word(cpu, REGADDR(14)) >> 1;
            cpu->WP = read_memory_word(cpu, REGADDR(13));
            break;
        }
        case CZC:
        case XOR:
        case COC: {
            uint8_t reg = (insn >> 6) & 0xf;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = insn & 0xF;

            // need to account for the immediate operand (= one more read)
            read_memory_word(cpu, cpu->PC << 1);

            uint16_t saddr = addressOf(cpu, Ts, MS, 2);
            uint16_t source = read_memory_word(cpu, saddr);
            uint16_t D = (cpu->RAM[WR(reg)] << 8) | cpu->RAM[WR(reg)+1];
            if (id == CZC)
                D = ~D;
            if (id == XOR) {
                D ^= source;
                write_memory_word(cpu, REGADDR(reg), D);
                setLAE(cpu, D);
            } 
            else {
                // status bit updates
                CLEAR_STATUS_BIT(EQUAL_BIT);
                if ((source & D) == source) SET_STATUS_BIT(EQUAL_BIT);
            }
            cpu->PC += 1;
            break;
        }
        case DIV:
        case MPY: {
            // TODO: Special timing rules
            uint8_t reg = (insn >> 6) & 0xf;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = insn & 0xF;
            uint16_t saddr = addressOf(cpu, Ts, MS, 2);
            uint16_t source = read_memory_word(cpu, saddr);
            uint16_t D = (cpu->RAM[WR(reg)] << 8) | cpu->RAM[WR(reg)+1];
            if (id == MPY) {
                unsigned int result = D * source;
                write_memory_word(cpu, REGADDR(reg), result >> 16);
                write_memory_word(cpu, REGADDR(reg+1), result & 0xFFFF);
            }
            if (id == DIV) {
                CLEAR_OVERFLOW();
                if (source <= D) {
                    SET_OVERFLOW(1);
                } else {
                    uint16_t Dd = (cpu->RAM[WR(reg+1)] << 8) | cpu->RAM[WR(reg+1)+1];
                    unsigned int divisor = (D << 16) | Dd;
                    uint16_t quotient = divisor / source;
                    uint16_t remainder = divisor % source;
                    write_memory_word(cpu, REGADDR(reg), quotient);
                    write_memory_word(cpu, REGADDR(reg+1), remainder);
                }
            }
            cpu->PC += 1;
            break;
        }
        case CLR:
        case BL:
        case BLWP:
        case SWPB:
        case SETO:
        case INV:
        case NEG:
        case DEC:
        case DECT:
        case INC:
        case INCT:
        case ABS:
        case X:
        case B: {
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = insn & 0xf;
            uint16_t saddr;
            if (id == SWPB) 
                saddr = addressOf(cpu, Ts, MS, 1);
            else
                saddr = addressOf(cpu, Ts, MS, 2);
            if (id == BL) {
                // timing and correctness
                read_memory_word(cpu, REGADDR(11));
                write_memory_word(cpu, REGADDR(11), (cpu->PC+1) << 1);
                LuaBranchTrace(saddr);
                cpu->PC = saddr >> 1;
            }
            if (id == BLWP) {
                uint16_t word = read_memory_word(cpu, saddr);
                uint16_t oldWP = cpu->WP;
                uint16_t oldPC = (cpu->PC+1) << 1;
                uint16_t oldST = cpu->ST;
                cpu->WP = word;
                word = read_memory_word(cpu, saddr + 2);
                LuaBranchTrace(word);
                cpu->PC = word >> 1;
                write_memory_word(cpu, REGADDR(13), oldWP);
                write_memory_word(cpu, REGADDR(14), oldPC);
                write_memory_word(cpu, REGADDR(15), oldST);
            }
            if (id == B) {
                // timing and correctness
                read_memory_word(cpu, saddr);
                LuaBranchTrace(saddr);
                cpu->PC = saddr >> 1;
            }
            if (id == CLR) {
                // timing and correctness
                read_memory_word(cpu, saddr);
                write_memory_word(cpu, saddr, 0x0000);
                cpu->PC += 1;
            }
            if (id == SWPB) {
                uint16_t word = read_memory_word(cpu, saddr);
                word = BSWAP(word);
                write_memory_word(cpu, saddr, word);
                cpu->PC += 1;
            }
            if (id == SETO) {
                // timing and correctness
                read_memory_word(cpu, saddr);
                write_memory_word(cpu, saddr, 0xFFFF);
                SET_STATUS_BIT(LGT_BIT);
                cpu->PC += 1;
            }
            if (id == INV) {
                uint16_t word = read_memory_word(cpu, saddr);
                word = ~word;
                write_memory_word(cpu, saddr, word);
                setLAE(cpu, word);
                cpu->PC += 1;
            }
            if (id == NEG) {
                int16_t word = read_memory_word(cpu, saddr);
                word = -word;
                write_memory_word(cpu, saddr, word);
                setLAE(cpu, word);
                // double check these bits
                CLEAR_OVERFLOW();
                if (((word >> 15) == 1))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                if (word == 0x8000)
                    SET_CARRY(1);
                cpu->PC += 1;
            }
            if (id == DEC || id == DECT) {
                uint8_t x = (id == DEC) ? 1 : 2;
                uint16_t word = read_memory_word(cpu, saddr);
                uint16_t result = word - x;
                write_memory_word(cpu, saddr, result);
                setLAE(cpu, result);
                CLEAR_OVERFLOW();
                if (((word >> 15) == 1) && ((result >> 15) == 0))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                // TODO: not sure about these status bits
                if (word > (x-1))
                    SET_CARRY(1);
                cpu->PC += 1;
            }
            if (id == INC || id == INCT) {
                uint8_t x = (id == INC) ? 1 : 2;
                uint16_t word = read_memory_word(cpu, saddr);
                uint16_t result = word + x;
                write_memory_word(cpu, saddr, result);
                setLAE(cpu, result);
                CLEAR_OVERFLOW();
                if (((word >> 15) == 1) && ((result >> 15) == 0))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                // TODO: not sure about these status bits
                if (word > (0xFFFF-x))
                    SET_CARRY(1);
                cpu->PC += 1;
            }
            if (id == X) {
                insn = read_memory_word(cpu, saddr);
                // Timing is a little tricky.  double check
                goto XLABEL;
            }
            if (id == ABS) {
                int16_t word = read_memory_word(cpu, saddr);
                int16_t absword = (word < 0) ? -word : word;
                if (word < 0) {
                    write_memory_word(cpu, saddr, absword);
                    cpu->cycles += 2;
                }
                setLAE(cpu, absword);
                // double check these bits
                /*
                CLEAR_OVERFLOW();
                if (((word >> 15) == 1))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                if (word == 0x8000)
                    SET_CARRY(1);
                    */
                CLEAR_CARRY();
                // how can carry be set??
                CLEAR_OVERFLOW();
                if (word == 0x8000)
                    SET_OVERFLOW(1);
                cpu->PC += 1;
            }
            break;
        }
        case SLA:
        case SRA:
        case SRC:
        case SRL: {
            uint8_t c = (insn >> 4) & 0xf;
            uint8_t w = insn & 0xf;
            if (c == 0) {
                cpu->cycles += 8;
                uint16_t wr0 = read_memory_word(cpu, cpu->WP);
                c = wr0 & 0xf;
                if (c == 0)
                    c = 16;
            }
            uint16_t word = read_memory_word(cpu, REGADDR(w));

            uint8_t carry = 0, overflow = 0;
            if (id == SRL) {
                carry = (word >> (c-1)) & 0x1;
                word = word >> c;
            }
            if (id == SLA) {
                carry = (uint16_t)(word << (c-1)) >> 15;
                uint16_t mask = 0xFFFF << (15-c);
                overflow = 0;
                if (((word & mask) !=0) && ((word & mask) != mask))
                    overflow = 1;
                  overflow = (word >> (15-c)) > 0;
                word = word << c;
                cpu->ST &= ~(1 << 11);
                cpu->ST |= (overflow << 11);
            }
            if (id == SRA) {
                int16_t w2 = word;
                carry = (w2 >> (c-1)) & 0x1;
                word = w2 >> c;
            }
            if (id == SRC) {
                uint16_t mask = (1 << c) - 1;
                uint16_t remainder = word & mask;
                word = word >> c;
                word |= (uint16_t)(remainder << (16-c));
                // set carry
                carry = word >> 15;
            }
            cpu->cycles += c * 2;
            write_memory_word(cpu, REGADDR(w), word);
            setLAE(cpu, word);
            // set carry
            cpu->ST &= ~(1 << 12);
            cpu->ST |= (carry << 12);
            cpu->PC += 1;
            break;
        }

        // Dual operand with multiple addressing modes
        case C:
        case CB:
        case A:
        case AB:
        case SB:
        case S:
        case MOV:
        case MOVB: {
            uint8_t Td = (insn >> 10) & 0x3;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = (insn & 0xf);
            uint8_t D = (insn >> 6) & 0xf;
            uint16_t saddr, daddr;
            if (id == MOVB || id == AB || id == CB || id == SB) {
                saddr = addressOf(cpu, Ts, MS, 1);
                daddr = addressOf(cpu, Td, D, 1);
            } else {
                saddr = addressOf(cpu, Ts, MS, 2);
                daddr = addressOf(cpu, Td, D, 2);
            }
            if (id == MOVB) {
                uint8_t source = read_memory_byte(cpu, saddr);
                // timing and correctness
//                read_memory_byte(cpu, daddr);
                write_memory_byte(cpu, daddr, source);
                setLAE(cpu, (uint16_t)(source << 8));
                setParity(cpu, source);
            }
            if (id == MOV) {
                uint16_t source = read_memory_word(cpu, saddr);
                // timing and correctness
//                read_memory_word(cpu, daddr);
                write_memory_word(cpu, daddr, source);
                setLAE(cpu, source);
            }
            if (id == C) {
                uint16_t source = read_memory_word(cpu, saddr);
                uint16_t dest = read_memory_word(cpu, daddr);
                // set status bits?
                uint8_t bits = 0;
                if ((source >> 15 == 1 && dest >> 15 == 0) || (source >> 15 == dest >> 15 && ((uint16_t)(dest-source)) >> 15 == 1))
//                    SET_STATUS_BIT(LGT_BIT);
                    bits = 0x4;
                if ((source >> 15 == 0 && dest >> 15 == 1) || (source >> 15 == dest >> 15 && ((uint16_t)(dest-source)) >> 15 == 1))
//                    SET_STATUS_BIT(AGT_BIT);
                    bits |= 0x2;
                if (source == dest) bits |= 0x1;
//                if (source == dest)
//                    SET_STATUS_BIT(EQUAL_BIT);
                cpu->ST = (cpu->ST & 0x1FFF) | (bits << 13);
            }
            if (id == CB) {
                uint8_t source = read_memory_byte(cpu, saddr);
                uint8_t dest = read_memory_byte(cpu, daddr);
                // set status bits?
                uint8_t bits = 0;
                if ((source >> 7 == 1 && dest >> 7 == 0) || (source >> 7 == dest >> 7 && ((uint8_t)(dest-source)) >> 7 == 1))
                    bits = 0x4;
                if ((source >> 7 == 0 && dest >> 7 == 1) || (source >> 7 == dest >> 7 && ((uint8_t)(dest-source)) >> 7 == 1))
                    bits |= 0x2;
                if (source == dest) bits |= 0x1;
                cpu->ST = (cpu->ST & 0x1FFF) | (bits << 13);
            }
            if (id == AB || id == SB) {
                uint8_t source = read_memory_byte(cpu, saddr);
                uint8_t dest = read_memory_byte(cpu, daddr);
                uint8_t result;
                if (id == AB)
                    result = dest + source;
                else
                    result = dest - source;
                write_memory_byte(cpu, daddr, result);
                CLEAR_OVERFLOW();
                if (((source >> 7) == (dest >> 7)) && ((result >> 7) != (dest >> 7)))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                if (result < dest || result < source)
                    SET_CARRY(1);
                setLAE(cpu, result << 8);
                setParity(cpu, result);
            }
            if (id == A || id == S) {
                uint16_t source = read_memory_word(cpu, saddr);
                uint16_t dest = read_memory_word(cpu, daddr);
                uint16_t result;
                if (id == A)
                    result = dest + source;
                else
                    result = dest - source;
                write_memory_word(cpu, daddr, result);
                CLEAR_OVERFLOW();
                if (((source >> 15) == (dest >> 15)) && ((result >> 15) != (dest >> 15)))
                    SET_OVERFLOW(1);
                CLEAR_CARRY();
                if (result < dest || result < source)
                    SET_CARRY(1);
                setLAE(cpu, result);
            }
            cpu->PC += 1;
            break;
        }

        case SOC:
        case SZC:
        case SOCB:
        case SZCB: {
            uint8_t Td = (insn >> 10) & 0x3;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = (insn & 0xf);
            uint8_t D = (insn >> 6) & 0xf;
            uint16_t saddr, daddr;
            if (id == SZCB || id == SOCB) {
                saddr = addressOf(cpu, Ts, MS, 1);
                daddr = addressOf(cpu, Td, D, 1);
            } else {
                saddr = addressOf(cpu, Ts, MS, 2);
                daddr = addressOf(cpu, Td, D, 2);
            }
            if (id == SZCB) {
                uint8_t source = read_memory_byte(cpu, saddr);
                uint8_t dest = read_memory_byte(cpu, daddr);
                dest &= ~source;
                write_memory_byte(cpu, daddr, dest);
                setLAE(cpu, dest << 8);
                setParity(cpu, dest);
            }
            if (id == SOCB) {
                uint8_t source = read_memory_byte(cpu, saddr);
                uint8_t dest = read_memory_byte(cpu, daddr);
                dest |= source;
                write_memory_byte(cpu, daddr, dest);
                setLAE(cpu, dest << 8);
                setParity(cpu, dest);
            }
            if (id == SOC) {
                uint16_t source = read_memory_word(cpu, saddr);
                uint16_t dest = read_memory_word(cpu, daddr);
                dest |= source;
                write_memory_word(cpu, daddr, dest);
                setLAE(cpu, dest);
            }
            if (id == SZC) {
                uint16_t source = read_memory_word(cpu, saddr);
                uint16_t dest = read_memory_word(cpu, daddr);
                dest &= ~source;
                write_memory_word(cpu, daddr, dest);
                setLAE(cpu, dest);
            }
            cpu->PC += 1;
            break;
        }

        // Immediate operand instructions
        case ANDI:
        case CI:
        case AI:
        case ORI:
        case LI: {
            uint8_t r = insn & 0xf;
            uint16_t imm = read_memory_word(cpu, (cpu->PC + 1) << 1);
            if (id == LI) {
                cpu->RAM[WR(r)+1] = imm & 0xFF;
                cpu->RAM[WR(r)] = imm >> 8;
                setLAE(cpu, imm);
            }
            if (id == ANDI) {
                uint16_t w = read_memory_word(cpu, REGADDR(r));
                w = w & imm;
                write_memory_word(cpu, REGADDR(r), w);
                setLAE(cpu, w);
            }
            if (id == ORI) {
                uint16_t w = read_memory_word(cpu, REGADDR(r));
                w = w | imm;
                write_memory_word(cpu, REGADDR(r), w);
                setLAE(cpu, w);
            }
            if (id == CI) {
                uint16_t w = read_memory_word(cpu, REGADDR(r));
                // compare imm to w and set bits
                uint8_t bits = 0;
                if ((w >> 15 == 1 && imm >> 15 == 0) || (w >> 15 == imm >> 15 && ((uint16_t)(imm-w)) >> 15 == 1))
                    bits = 0x4;
                if ((w >> 15 == 0 && imm >> 15 == 1) || (w >> 15 == imm >> 15 && ((uint16_t)(imm-w)) >> 15 == 1))
                    bits |= 0x2;
                if (w == imm) bits |= 0x1;
                cpu->ST = (cpu->ST & 0x1FFF) | (bits << 13);
//                setLAE(&cpu, imm);
            }
            if (id == AI) {
                uint16_t w = read_memory_word(cpu, REGADDR(r));
                uint16_t result = w + imm;
                setLAE(cpu, result);
                CLEAR_OVERFLOW();
                CLEAR_CARRY();
                if (((w >> 15) == (imm >> 15)) && ((result >> 15) != (w >> 15)))
                    SET_OVERFLOW(1);
                // What is the difference between overflow and carry??
                if (result < w || result < imm)
                    SET_CARRY(1);
                write_memory_word(cpu, REGADDR(r), result);
            }
            // -- 
            cpu->PC += 2;
            break;
        }
        case JOC:
        case JNC: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            uint8_t compare = ((id == JOC) ? 1 : 0);
            if (GET_STATUS_BIT(CARRY_BIT) == compare) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JEQ:
        case JNE: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            uint8_t compare = ((id == JEQ) ? 1 : 0);
            if (GET_STATUS_BIT(EQUAL_BIT) == compare) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JH: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if ((GET_STATUS_BIT(LGT_BIT) == 1) && (GET_STATUS_BIT(EQUAL_BIT) == 0)) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JHE: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if ((GET_STATUS_BIT(LGT_BIT) == 1) || (GET_STATUS_BIT(EQUAL_BIT) == 1)) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JL: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if ((GET_STATUS_BIT(LGT_BIT) == 0) && (GET_STATUS_BIT(EQUAL_BIT) == 0)) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JNO: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if (GET_STATUS_BIT(OVERFLOW_BIT) == 0) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JLE: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if (GET_STATUS_BIT(LGT_BIT) == 0 || GET_STATUS_BIT(EQUAL_BIT) == 1) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JMP: {
            int8_t disp = (insn & 0xff);
            LuaBranchTrace((cpu->PC + disp + 1) << 1);
            cpu->cycles += 2;
            cpu->PC += (disp + 1);       // +1 for this ins.
            break;
        }

        // CRU single-bit instructions
        case SBO:
        case SBZ:
        case TB: {
            int8_t disp = (insn & 0xff);
            uint16_t r12 = read_memory_word(cpu, REGADDR(12));
            uint16_t CRU_addr = r12 + disp;
            // TODO: read CRU bit, set ST2 = 1 if set

            if (id == SBO || id == SBZ)
                CRU_write_bit(cpu, CRU_addr, (id == SBO) ? 1 : 0);
            else {
                if (CRU_read_bit(cpu, CRU_addr))
                    SET_STATUS_BIT(EQUAL_BIT);
                else
                    CLEAR_STATUS_BIT(EQUAL_BIT);
            }
            cpu->PC += 1;
            break;
        }
        case LWPI:
        case LIMI: {
            uint16_t imm = read_memory_word(cpu, (cpu->PC + 1) << 1);
            if (id == LIMI) {
                cpu->ST &= 0xfff0;
                cpu->ST |= imm & 0xf;
            }
            if (id == LWPI)
                cpu->WP = imm;
            cpu->PC += 2;
            break;
        }
        case JGT: {
            int8_t disp = (insn & 0xff);
            cpu->PC += 1;
            if (GET_STATUS_BIT(AGT_BIT)) {
                LuaBranchTrace((cpu->PC + disp) << 1);
                cpu->cycles += 2;
                cpu->PC += disp;
            }
            break;
        }
        case JLT: {
            int8_t disp = (insn & 0xff);
            if (((cpu->ST >> 13) & 0x3) == 0) {
                LuaBranchTrace((cpu->PC + disp + 1) << 1);
                cpu->cycles += 2;
                cpu->PC += (disp + 1);
            }
            else
                cpu->PC += 1;
            break;
        }


        // CRU multiple-bit instructions
        case STCR: {
            // CRU base register is WR12
            uint8_t c = (insn >> 6) & 0xf;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = insn & 0xf;
            uint16_t w12 = read_memory_word(cpu, REGADDR(12));
            if (c > 8) {
                uint16_t saddr = addressOf(cpu, Ts, MS, 2);
                // XXX: always writing 0
                // timing and correctness
                read_memory_word(cpu, saddr);
                write_memory_word(cpu, saddr, 0xFF00);
                setLAE(cpu, 0xFF);
                __android_log_print(ANDROID_LOG_INFO, "And99", "STCR > 8");
            }
            else {
//                uint8_t byte = 0xFF;
                uint8_t byte = 0x0;
                uint16_t saddr = addressOf(cpu, Ts, MS, 1);
                if (w12 == 0x6)  {              // keyboard/joystick read
                    byte = cpu->keyboard.keys[cpu->keyboard.column];
                    /*
                    if (cpu->keyboard.column > 5) {
                        __android_log_print(ANDROID_LOG_INFO, "And99", "reading joystick: %d = 0x%x, 0x%x", cpu->keyboard.column, byte, byte & ~(1 << c));
                    }
                    */
                    byte &= ~(0xFF << c);
                }
                else
                    __android_log_print(ANDROID_LOG_INFO, "And99", "CRU STCR: 0x%x, 0x%x, %d", w12, saddr, c);
                // timing and correctness
                read_memory_byte(cpu, saddr);
                write_memory_byte(cpu, saddr, byte);
                setLAE(cpu, byte);
            }

            // weird timing calculation
            if (c == 0) cpu->cycles += 18;
            if (c == 8) cpu->cycles += 2;
            if (c > 8) cpu->cycles += 16;

            // setParity for 1 <= c <= 8
            cpu->PC += 1;
            break;
        }
        case LDCR: {
            // CRU base register is WR12
            uint8_t c = (insn >> 6) & 0xf;
            uint8_t Ts = (insn >> 4) & 0x3;
            uint8_t MS = insn & 0xf;
            uint16_t source;
            if (c > 8) {
                uint16_t saddr = addressOf(cpu, Ts, MS, 2);
                source = read_memory_word(cpu, saddr);
            }
            else {
                uint16_t saddr = addressOf(cpu, Ts, MS, 1);
                source = read_memory_byte(cpu, saddr) << 8;
            }
            uint16_t w12 = read_memory_word(cpu, REGADDR(12));
            if (w12 == 0x24)            // keyboard set column
            {
            /*
                if ((source >> 8) > 5) {
                    // this is the joystick
                    __android_log_print(ANDROID_LOG_ERROR, "And99", "invalid kb column = %d", (source >> 8));
                } else
                */
                    cpu->keyboard.column = source >> 8;     // also mask c bits
            }
            else if (w12 > 0x1100 && w12 < 0x1110) {
                while (c) {
                    fdc_cru_writebit(w12++, (source & 0x1));
                    source >>= 1;
                    c--;
                }
            }
            else
                __android_log_print(ANDROID_LOG_INFO, "And99", "LDCR: 0x%x, %d, 0x%x\n", w12, c, source);
            setLAE(cpu, source);
            if (c >= 1 && c <= 8)
                setParity(cpu, source);
            cpu->cycles += 2 * c;
            cpu->PC += 1;
            break;
        }

        // Status instructions
        case STST: {
            uint8_t reg = insn & 0xf;
            write_memory_word(cpu, REGADDR(reg), cpu->ST);
            cpu->PC += 1;
            break;
        }

        case STWP: {
            uint8_t reg = insn & 0xf;
            write_memory_word(cpu, REGADDR(reg), cpu->WP);
            cpu->PC += 1;
            break;
        }
    }

    cpu->cycles += base_cycles[id];
    cpu->icount++;

    return cpu->cycles;
}


/*
 * Run the CPU until the PC reaches address breakpoint or the number of cycles
 * pass.  Return # of cycles left.  (Which may be negative as we execute instructions
 * without checking that we have time for them; but only one)
 */
int TMS9900_RunCPU(struct TMS9900 *cpu, uint16_t breakpoint, int32_t cycles)
{
    uint16_t lastPC = cpu->PC;
    while ((cpu->PC != breakpoint) && (cycles > 0)) {
        cycles -= TMS9900_StepCPU(cpu);
        if (lastPC == cpu->PC)
            // not making any progress. possibly an unimplemented
            // instruction.
            return cycles;
        lastPC = cpu->PC;
    }

    return cycles;
}


void TMS9900_LoadFDCROM(struct TMS9900 *cpu, uint8_t *buffer)
{
    memcpy(FDCROM, buffer, sizeof(FDCROM));
}
