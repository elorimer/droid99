#include <stdio.h>
#include <pthread.h>

#ifndef TMS9900_INS
#define TMS9900_INS

#define NUM_INSTRUCTIONS        69

enum { A = 0, AB, C, CB, S, SB, SOC, SOCB, SZC, SZCB, MOV, MOVB, COC, CZC, XOR,
       MPY, DIV, XOP, B, BL, BLWP, CLR, SETO, INV, NEG, ABS, SWPB, INC, INCT,
       DEC, DECT, X, LDCR, STCR, SBO, SBZ, TB, JEQ, JGT, JH, JHE, JL, JLE, JLT,
       JMP, JNC, JNE, JNO, JOC, JOP, SLA, SRA, SRC, SRL, AI, ANDI, CI, LI, ORI,
       LWPI, LIMI, STST, STWP, RTWP, IDLE, CKOF, CKON, LREX };

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;

#define BSWAP(x)    ((uint16_t)((((uint16_t)x) << 8) | (((uint16_t)x) >> 8)))


static const char *mnemonics[] = {
    "A   ", "AB  ", "C   ", "CB  ", "S   ", "SB  ", "SOC ", "SOCB", "SZC ",
    "SZCB", "MOV ", "MOVB", "COC ", "CZC ", "XOR ", "MPY ", "DIV ", "XOP ",
    "B   ", "BL  ", "BLWP", "CLR ", "SETO", "INV ", "NEG ", "ABS ", "SWPB",
    "INC ", "INCT", "DEC ", "DECT", "X   ", "LDCR", "STCR", "SBO ", "SBZ ",
    "TB  ", "JEQ ", "JGT ", "JH  ", "JHE ", "JL  ", "JLE ", "JLT ", "JMP ",
    "JNC ", "JNE ", "JNO ", "JOC ", "JOP ", "SLA ", "SRA ", "SRC ", "SRL ",
    "AI  ", "ANDI", "CI  ", "LI  ", "ORI ", "LWPI", "LIMI", "STST", "STWP",
    "RTWP", "IDLE", "RSET", "CKOF", "CKON", "LREX" };


#define INTERRUPT   0
#define KEYBOARD    1
struct __attribute__((__packed__)) ReplayRecord {
//    enum        { INTERRUPT = 0, KEYBOARD }     type;
    uint8_t     type;
    uint32_t    icount;
    uint16_t    PC;             // DEBUG to make sure we're in sync
    uint8_t     data[3];        // keyboard (row,col,state)
};

/* CRU peripheral card/device */
struct peripheral {
    uint16_t start;             // CRU starting address
    uint16_t end;               // CRU ending address
    // callback functions - sbo, sbz, ldcr, stcr, read_mem
};


struct TMS9900 {
    uint16_t    PC;         // program counter
    uint16_t    WP;         // workspace pointer
    uint16_t    ST;         // status register
    uint8_t     RAM[256];   // 256-bytes of scratch-pad memory mapped at
                            // 0x8000-80FF, 0x8100-81FF, 0x8200-82FF,
                            // 0x8300-0x83FF
    uint8_t     interrupt;  // TI-99/4A only has one interrupt level
    uint8_t     vdp_int;

    uint8_t     exception;  // flag whether we hit an exception and should break
                            // into the debugger

    // other variables
    uint8_t     *ROM;
    uint8_t     *cartridge;     // cartridge GROM accessible through GROM port
    uint8_t     *cartRAM;       // cartridge accessible at 0x6000

    uint32_t    icount;         // total instruction counter. use to stamp traces
    FILE        *trace;         // trace file to read/write
    uint8_t     replay;         // boolean: replay the trace file
    struct ReplayRecord next_event;

    uint32_t    cycles;         // cycle counter for accurate time simulation
    uint8_t     expansion_mem[32768];
    uint8_t     *peripheral_mem;

//    pthread_mutex_t lock;   // lock for PC+icounter updates

    // this should be a generic CRU interface
    struct {
        uint8_t alpha;
        uint8_t column;
        // 'keys' actually includes the joystick interface since it reads
        // through the same CRU interface
        uint8_t keys[8];
        uint8_t queued;
        uint8_t data[3][3];
        pthread_mutex_t lock;
    } keyboard;

    /* These are in no particular order so we scan through all n_peripherals
       for the correct address range */
    struct peripheral peripherals[8];
    int n_peripherals;

    // DEBUG
    uint32_t    icounts[NUM_INSTRUCTIONS];     // instruction counts
};


// public functions
int TMS9900_Create(struct TMS9900 *cpu, const char *rom, uint8_t _replay);
void TMS9900_Destroy(struct TMS9900 *cpu);
void TMS9900_Reset(struct TMS9900 *cpu);
int TMS9900_GetImageSize();
void TMS9900_SaveImage(struct TMS9900 *cpu, uint8_t *buffer);
void TMS9900_LoadImage(struct TMS9900 *cpu, uint8_t *buffer);
void TMS9900_Interrupt(struct TMS9900 *cpu);
int TMS9900_StepCPU(struct TMS9900 *cpu);
int TMS9900_RunCPU(struct TMS9900 *cpu, uint16_t breakpoint, int32_t cycles);
void TMS9900_DumpRegisters(struct TMS9900 *cpu, char *buffer, int buflen);
void TMS9900_Keyboard(struct TMS9900 *cpu, uint8_t row, uint8_t col, uint8_t state);
void TMS9900_LoadFDCROM(struct TMS9900 *cpu, uint8_t *buffer);


#endif      // TMS9900_INS
