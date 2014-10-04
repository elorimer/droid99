#ifndef TI99_FDC_H
#define TI99_FDC_H

#include "tms9900-memory.h"

/* DS/SD floppy disks */
#define NUM_SIDES       2
#define NUM_TRACKS      40
#define NUM_SECTORS     9
#define SECTOR_SIZE     256
#define FD_SIZE         (NUM_SIDES * NUM_TRACKS * NUM_SECTORS * SECTOR_SIZE)

#define BYTES_PER_TRACK (NUM_SECTORS * SECTOR_SIZE)
#define BYTES_PER_SIDE  (BYTES_PER_TRACK * NUM_TRACKS)


void TIFDC_Initialize();
void TIFDC_LoadImage(uint8_t* buffer);
void TIFDC_SaveImage(uint8_t* buffer, int len);
void TIFDC_Cleanup();

void fdc_cru_writebit(uint16_t address, uint8_t bit);
void fdc_write_byte(uint16_t address, uint8_t byte);
uint8_t fdc_read_byte(uint16_t address);

#endif
