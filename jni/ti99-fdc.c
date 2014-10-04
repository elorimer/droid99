#include "ti99-fdc.h"
#include "ti99-logger.h"

static struct {
    // register names
//    enum { STATUS, TRACKIN, SECTORIN, DATAIN, COMMAND, TRACKOUT, SECTOROUT, DATAOUT };
    uint8_t registers[8];

    uint8_t status;
    uint8_t track, sector, data;
    uint8_t side;

    uint8_t buffer[256];
    uint16_t blen, bp, wp;

    unsigned write_idx;           // index where we are writing.
                                  // blen counts down

    uint8_t *raw_data;
} DiskState;


void TIFDC_Initialize()
{
    DiskState.raw_data = NULL;
    DiskState.wp = 0;
    DiskState.side = 0;
}


void TIFDC_LoadImage(uint8_t* buffer)
{
    if (DiskState.raw_data == NULL)
        DiskState.raw_data = malloc(FD_SIZE);

    memcpy(DiskState.raw_data, buffer, FD_SIZE);
}


/*
 * Write the disk image back out to save back to file
 */
void TIFDC_SaveImage(uint8_t* buffer, int len)
{
    // assert len is correct
    if (DiskState.raw_data != NULL)
        memcpy(buffer, DiskState.raw_data, len);
}


void TIFDC_Cleanup()
{
    if (DiskState.raw_data)
        free(DiskState.raw_data);
}


void fdc_cru_writebit(uint16_t address, uint8_t bit)
{
    if (address == 0x1100) {
        // initialization
        DiskState.status = 0xff;
        return;
    }
        
    if (address == 0x1101)          // strobe motor
        return;
    if (address == 0x1103)          // signal head loaded
        return;

    if (address == 0x1107)
        DiskState.side = bit;

    LOG_INFO("FDC: CRU write bit 0x%x = %d", address, bit);
}


void fdc_write_byte(uint16_t address, uint8_t byte)
{
    // bus is inverted
    byte = ~byte;
    
    enum { RESTORE, SEEK, STEP, STEPIN, STEPOUT, READSECT, WRITESECT, READID,
           FORCEINT, READTRACK, WRITETRACK } commands;
    static const char* command_str[] = { "RESTORE", "SEEK", "STEP", "STEP-IN",
        "STEP-OUT", "READSECT", "WRITESECT", "READID", "FORCE_INT", "READTRACK",
        "WRITETRACK" };
    uint8_t cmd_map[16] = { 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10 };

    if (address == 0x5ff8)
    {
        // command register
        uint8_t hi_nibble = byte >> 4;
        uint8_t cmd = cmd_map[hi_nibble];
        LOG_INFO("FDC: command = %s", command_str[cmd]);
        
        switch (cmd) {
            case RESTORE:
                DiskState.track = 0;
                DiskState.status = ~0x24;       // track0, head loaded
                break;
            case FORCEINT:
                LOG_INFO("FDC: issue interrupts 0x%x", byte & 0xf);
                // reset status to good/known state
                DiskState.status = ~0x24;        // track0, head loaded
                break;
            case SEEK:
                LOG_INFO("FDC: SEEK track = %d", DiskState.data);
                DiskState.track = DiskState.data;
                if (DiskState.track == 0)
                    DiskState.status = ~0x24;
                else
                    DiskState.status = ~0x20;       // head loaded
                break;
            case READID:
                DiskState.buffer[0] = DiskState.track;
                DiskState.buffer[1] = DiskState.side;
                DiskState.buffer[2] = DiskState.sector;
                DiskState.buffer[3] = 0x1;          // sector length = 256
                DiskState.buffer[4] = 0x0;
                DiskState.buffer[5] = 0x0;          // XXX: CRC implement
                DiskState.blen = 6;
                DiskState.bp = 0;
                DiskState.status = 0xff;
                break;
            case READSECT:
                LOG_INFO("FDC: reading sector %d, track %d, side %d", DiskState.sector,
                            DiskState.track, DiskState.side);
                unsigned int index = DiskState.side * BYTES_PER_SIDE +
                                     DiskState.track * BYTES_PER_TRACK +
                                     DiskState.sector * SECTOR_SIZE;
                // assert legal values in range, raw_data not null, ...
                memcpy(DiskState.buffer, DiskState.raw_data + index, SECTOR_SIZE);
                DiskState.blen = SECTOR_SIZE;
                DiskState.bp = 0;
                DiskState.status = 0xff;
                break;
            case WRITESECT:
                LOG_INFO("FDC: writing sector %d, track %d, side %d", DiskState.sector,
                            DiskState.track, DiskState.side);
                DiskState.write_idx = DiskState.side * BYTES_PER_SIDE +
                                      DiskState.track * BYTES_PER_TRACK +
                                      DiskState.sector * SECTOR_SIZE;
                DiskState.blen = SECTOR_SIZE;
                DiskState.wp = SECTOR_SIZE;
                DiskState.status = 0xff;
                break;
            default:
                LOG_INFO("FDC: Unhandled command");
                break;
        }
        return;
    }
    if (address == 0x5ffa)  { DiskState.track = byte; LOG_INFO("FDC: track register = %d", DiskState.track); return; }
    if (address == 0x5ffc)  { DiskState.sector = byte; LOG_INFO("FDC: sector register = %d", DiskState.sector); return; }
    if (address == 0x5ffe)
    {
        DiskState.data = byte;
        if (DiskState.wp > 0) {
            LOG_INFO("FDC: writing byte to disk: 0x%x", byte);
            DiskState.raw_data[DiskState.write_idx++] = byte;
            DiskState.wp--;
        }
        return;
    }

    LOG_INFO("FDC: unhandled fdc_write_byte 0x%x", address);
}


uint8_t fdc_read_byte(uint16_t address)
{
    if (address == 0x5ff0) {
        LOG_INFO("FDC: read_byte status = 0x%x", DiskState.status);
        // status register
        return DiskState.status;
    }

    if (address == 0x5ff6) {
        // data register, do we have anything to feed
        if (DiskState.bp < DiskState.blen) {
            LOG_INFO("FDC: read_byte data = 0x%x", DiskState.buffer[DiskState.bp]);
            return ~DiskState.buffer[DiskState.bp++];
        }
        // set some error bit in the status register
    }

    LOG_INFO("FDC: read_byte 0x%x = 0x0", address);
    return 0x0;
}
