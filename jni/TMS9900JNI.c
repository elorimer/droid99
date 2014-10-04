#include <jni.h>
#include <stdlib.h>
#include <android/log.h>
#include "tms9900-memory.h"
#include "ti99-fdc.h"

#define LOG_INFO(format, args...)    __android_log_print(ANDROID_LOG_INFO, "And99", format, ## args)
#define LOG_ERROR(format, args...)   __android_log_print(ANDROID_LOG_ERROR, "And99", format, ## args)

// use a static var for now and don't pass between java-C
static struct TMS9900 mycpu;
static struct {
    uint8_t *memory;
    uint16_t address;
    int latch;
} GROM;
static uint8_t *vdpMemory = NULL;
static uint16_t vdpMemSize = 0;
static int vdpMemoryCounts[16384];

static uint8_t vdpTracePtr = 0;
static int vdpTracePC[16];

static struct {
    uint8_t registers[8];
    uint8_t status;
    uint16_t address;
    int latch;
    uint8_t dirty;
} VDP;
static struct {
    // tone generators
    uint16_t frequency[4];
    uint8_t attenuation[4];
    jobject soundCallbackObj;
} SndChip;

void LuaBindMemory(uint8_t *mem, const char *symbol);
JavaVM *gJavaVM;


jint
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    gJavaVM = vm;
    SndChip.soundCallbackObj = NULL;
    return JNI_VERSION_1_4;
}


int
Java_com_emllabs_droid99_TI99Simulator_CreateCPU(JNIEnv *env, jobject thiz,
                                              jbyteArray rom, jbyteArray grom,
                                              jboolean replay)
{
    TMS9900_Create(&mycpu, "994AROM.BIN", (uint8_t)replay);
    size_t gromLength = (*env)->GetArrayLength(env, grom);
    uint8_t *buffer = (uint8_t *)malloc(8192);
    GROM.address = 0;
    GROM.latch = 0;
    GROM.memory = (uint8_t *)malloc(gromLength);
    jbyte *bytes = (*env)->GetByteArrayElements(env, rom, 0);
    jbyte *gbytes = (*env)->GetByteArrayElements(env, grom, 0);
    memcpy(buffer, bytes, 8192);
    memcpy(GROM.memory, gbytes, gromLength);
    (*env)->ReleaseByteArrayElements(env, rom, bytes, 0);
    (*env)->ReleaseByteArrayElements(env, grom, gbytes, 0);
    // free mycpu.ROM except we know that it failed to allocate
    mycpu.ROM = buffer;

    memset(mycpu.icounts, 0, sizeof(uint32_t) * NUM_INSTRUCTIONS);

    // Lua bindings
    LuaBindMemory(GROM.memory, "grom_mem");
    LuaBindMemory(mycpu.ROM, "console_rom");
    LuaBindMemory(mycpu.RAM, "cpu_ram");
    LuaBindCPU(&mycpu);

    // FDC.  Not really part of CPU.  We should split these components
    // out properly
    TIFDC_Initialize();

    FILE *infile = fopen("/mnt/sdcard/floppy.dsk", "r");
    if (!infile) {
        LOG_ERROR("Unable to open image file for floppy disk.");
        return -1;
    }
    uint8_t *diskbuf = malloc(FD_SIZE);
    fread(diskbuf, 1, FD_SIZE, infile);
    fclose(infile);

    TIFDC_LoadImage(diskbuf);
    free(diskbuf);

    return 1;
}


void
Java_com_emllabs_droid99_TI99Simulator_DestroyCPU(JNIEnv *env, jobject thiz)
{
    // FDC.  Not really part of CPU
    TIFDC_Cleanup();

    TMS9900_Destroy(&mycpu);
    free(GROM.memory);
}


void
Java_com_emllabs_droid99_TI99Simulator_ResetCPU(JNIEnv *env, jobject thiz)
{
    // "soft" reset.  Leave built-in ROM and GROM untouched

    // assert CPU created

    // lock something
    GROM.latch = 0;
    GROM.address = 0x0;

    // more VDP resets
    VDP.latch = 0;
    VDP.dirty = 1;
    memset(vdpMemory, 0x0, vdpMemSize);

    // more reset (sound chip, esp)

    // reset PC
    TMS9900_Reset(&mycpu);
}

jstring
Java_com_emllabs_droid99_TI99Simulator_DumpRegisters(JNIEnv *env, jobject thiz)
{
    char buffer[256];
    TMS9900_DumpRegisters(&mycpu, buffer, 256);

    return (*env)->NewStringUTF(env, buffer);
}

jint
Java_com_emllabs_droid99_TI99Simulator_StepCPU(JNIEnv *env, jobject thiz)
{
    return TMS9900_StepCPU(&mycpu);
}

jint
Java_com_emllabs_droid99_TI99Simulator_RunCPU(JNIEnv *env, jobject thiz,
                                           jint breakpoint,
                                           jint cycles)
{
    return TMS9900_RunCPU(&mycpu, (uint16_t)breakpoint, (int32_t)cycles);
}

void
Java_com_emllabs_droid99_TI99Simulator_SetVDPMemory(JNIEnv *env, jobject thiz,
                                                 jobject bytes, jint length)
{
    vdpMemory = (uint8_t *)(*env)->GetDirectBufferAddress(env, bytes);
    vdpMemSize = length;
    memset(vdpMemory, 0x0, length);
    VDP.dirty = 1;

    LuaBindMemory(vdpMemory, "vdp_mem");
}

jbyteArray
Java_com_emllabs_droid99_TI99Simulator_GetVDPRegisters(JNIEnv *env, jobject thiz)
{
    jbyteArray jb = (*env)->NewByteArray(env, 10);
    (*env)->SetByteArrayRegion(env, jb, 0, 8, (jbyte *)VDP.registers);
    (*env)->SetByteArrayRegion(env, jb, 8, 1, (jbyte *)&VDP.status);
    (*env)->SetByteArrayRegion(env, jb, 9, 1, (jbyte *)&VDP.dirty);
    return jb;
}

jintArray
Java_com_emllabs_droid99_TI99Simulator_GetVDPCounts(JNIEnv *env, jobject thiz)
{
    jintArray jb = (*env)->NewIntArray(env, 16384+16);
    (*env)->SetIntArrayRegion(env, jb, 0, 16384, vdpMemoryCounts);
    (*env)->SetIntArrayRegion(env, jb, 16384, 16, vdpTracePC);
    return jb;
}

void
Java_com_emllabs_droid99_TI99Simulator_ClearVDPCounts(JNIEnv *env, jobject thiz)
{
    memset(vdpMemoryCounts, 0x0, sizeof(vdpMemoryCounts));
    memset(vdpTracePC, 0x0, sizeof(vdpTracePC));
    vdpTracePtr = 0;
}


void
Java_com_emllabs_droid99_TI99Simulator_TIKeyboardChange(JNIEnv *env,
    jobject thiz, jint row, jint col, jint state)
{
    TMS9900_Keyboard(&mycpu, (uint8_t)row, (uint8_t)col, (uint8_t)state);
}

void
Java_com_emllabs_droid99_TI99Simulator_LoadCartGROM(JNIEnv *env, jobject thiz,
    jbyteArray data)
{
    size_t cartSize = (*env)->GetArrayLength(env, data);
    jbyte *bytes = (*env)->GetByteArrayElements(env, data, 0);
    free(mycpu.cartridge);
    mycpu.cartridge = (uint8_t *)malloc(cartSize);
    memcpy(mycpu.cartridge, bytes, cartSize);
    LuaBindMemory(mycpu.cartridge, "cartridge_grom");
}

void
Java_com_emllabs_droid99_TI99Simulator_LoadCartRAM(JNIEnv *env, jobject thiz,
    jbyteArray data)
{
    size_t cartSize = (*env)->GetArrayLength(env, data);
    jbyte *bytes = (*env)->GetByteArrayElements(env, data, 0);
    free(mycpu.cartRAM);
    mycpu.cartRAM = (uint8_t *)malloc(cartSize);
    memcpy(mycpu.cartRAM, bytes, cartSize);
    LuaBindMemory(mycpu.cartRAM, "cartridge_ram");
}

void
Java_com_emllabs_droid99_TI99Simulator_LoadFDCROM(JNIEnv *env, jobject thiz,
    jbyteArray data)
{
    size_t romSize = (*env)->GetArrayLength(env, data);
    jbyte *bytes = (*env)->GetByteArrayElements(env, data, 0);
    TMS9900_LoadFDCROM(&mycpu, bytes);
}


jintArray
Java_com_emllabs_droid99_TI99Simulator_GetICounts(JNIEnv *env, jobject thiz)
{
    int i;
    jintArray counts = (*env)->NewIntArray(env, NUM_INSTRUCTIONS);
    jint *elements = (*env)->GetIntArrayElements(env, counts, NULL);
    for (i=0; i < NUM_INSTRUCTIONS; i++)
        elements[i] = mycpu.icounts[i];
    (*env)->ReleaseIntArrayElements(env, counts, elements, 0);

    return counts;
}

void
Java_com_emllabs_droid99_TI99Simulator_ResetICounts(JNIEnv *env, jobject thiz)
{
    memset(mycpu.icounts, 0, sizeof(uint32_t) * NUM_INSTRUCTIONS);
}


void
Java_com_emllabs_droid99_TI99Simulator_VDPInterrupt(JNIEnv *env, jobject thiz)
{
    mycpu.interrupt = 1;
    mycpu.vdp_int = 1;
}

void
Java_com_emllabs_droid99_TI99Simulator_ClearVDPDirty(JNIEnv *env, jobject thiz)
{
    VDP.dirty = 0;
}


void
Java_com_emllabs_droid99_TI99Simulator_SaveImage(JNIEnv *env, jobject thiz)
{
    FILE *outfile = fopen("/mnt/sdcard/image.dat", "w");
    if (!outfile) {
        LOG_ERROR("Unable to open image file for output");
        return;
    }
    uint32_t bufSize = TMS9900_GetImageSize();
    uint8_t *buffer = malloc(bufSize);
    TMS9900_SaveImage(&mycpu, buffer);
    fwrite(buffer, 1, bufSize, outfile);
    free(buffer);

    // Save GROM
    fwrite(&GROM.address, sizeof(GROM.address), 1, outfile);
    fwrite(&GROM.latch, sizeof(GROM.latch), 1, outfile);

    // Save VDP
    fwrite(VDP.registers, 1, 8, outfile);
    fwrite(&VDP.status, sizeof(VDP.status), 1, outfile);
    fwrite(&VDP.address, sizeof(VDP.address), 1, outfile);
    fwrite(&VDP.latch, sizeof(VDP.latch), 1, outfile);
    fwrite(vdpMemory, 1, vdpMemSize, outfile);

    // TODO: Save sound

    // Save floppy disk image
    FILE *diskfile = fopen("/mnt/sdcard/floppy.dsk", "w");
    if (!diskfile) {
        LOG_ERROR("Unable to open floppy disk image for writing");
        fclose(outfile);
        return;
    }
    uint8_t *diskbuf = malloc(FD_SIZE);
    TIFDC_SaveImage(diskbuf, FD_SIZE);
    fwrite(diskbuf, 1, FD_SIZE, diskfile);
    free(diskbuf);
    fclose(diskfile);
    
    fclose(outfile);
}


void
Java_com_emllabs_droid99_TI99Simulator_LoadDSKImage(JNIEnv *env, jobject thiz, jstring fname)
{
    const char *s = (*env)->GetStringUTFChars(env, fname, NULL);
    FILE *dskFile = fopen(s, "r");
    if (!dskFile) {
        LOG_ERROR("Unable to open disk image: %s", s);
        (*env)->ReleaseStringUTFChars(env, fname, s);
        return;
    }
    uint8_t* buffer = malloc(FD_SIZE);
    size_t nbytes = fread(buffer, 1, FD_SIZE, dskFile);
    LOG_INFO("Read %d bytes from disk image.", nbytes);
    fclose(dskFile);

    TIFDC_LoadImage(buffer);
    free(buffer);

    (*env)->ReleaseStringUTFChars(env, fname, s);
}


void
Java_com_emllabs_droid99_TI99Simulator_LoadImage(JNIEnv *env, jobject thiz)
{
    FILE *infile = fopen("/mnt/sdcard/image.dat", "r");
    if (!infile) {
        LOG_ERROR("Unable to open image file for output.");
        return;
    }

    uint32_t bufSize = TMS9900_GetImageSize();
    uint8_t *buffer = malloc(bufSize);
    fread(buffer, 1, bufSize, infile);
    TMS9900_LoadImage(&mycpu, buffer);

    fread(&GROM.address, sizeof(GROM.address), 1, infile);
    fread(&GROM.latch, sizeof(GROM.latch), 1, infile);

    fread(VDP.registers, 1, 8, infile);
    fread(&VDP.status, sizeof(VDP.status), 1, infile);
    fread(&VDP.address, sizeof(VDP.address), 1, infile);
    fread(&VDP.latch, sizeof(VDP.latch), 1, infile);
    fread(vdpMemory, 1, vdpMemSize, infile);

    free(buffer);
    fclose(infile);
}


void
Java_com_emllabs_droid99_TI99Simulator_ClearException(JNIEnv *env, jobject thiz)
{
    mycpu.exception = 0;
}

jint
Java_com_emllabs_droid99_TI99Simulator_GetGROMAddress(JNIEnv *env, jobject thiz)
{
    return GROM.address;
}

jint
Java_com_emllabs_droid99_TI99Simulator_GetGROMByte(JNIEnv *env, jobject thiz)
{
    if ((GROM.address-1) > 0x5FFF) {
        if (mycpu.cartridge && (GROM.address-1) < 0x10000)
            return mycpu.cartridge[(GROM.address-1) - 0x6001];
        return 0x0;
    }
    return GROM.memory[GROM.address-1];
}

jbyteArray
Java_com_emllabs_droid99_TI99Simulator_ReadCPUMemory(JNIEnv *env, jobject thiz)
{
    jbyteArray result = (*env)->NewByteArray(env, 256);
    (*env)->SetByteArrayRegion(env, result, 0, 256, mycpu.RAM);

    return result;
}


void
Java_com_emllabs_droid99_TI99Simulator_SetupSound(JNIEnv *env, jobject thiz,
                                               jobject cbObj)
{
    SndChip.frequency[0] = 0xFFFF;
    SndChip.frequency[1] = 0xFFFF;
    SndChip.frequency[2] = 0xFFFF;
    SndChip.attenuation[0] = 0xF;
    SndChip.attenuation[1] = 0xF;
    SndChip.attenuation[2] = 0xF;

    // stash a reference to the callback object
    SndChip.soundCallbackObj = (*env)->NewGlobalRef(env, cbObj);
    // TODO: This needs to be freed somewhere
}



void soundCallback(int generator)
{
    // Check soundCallbackObj is not NULL (race)
    if (SndChip.soundCallbackObj == NULL) {
        // sound not set up
        return;
    }

    // TODO: Cache some of the parameters
    JNIEnv *env;
    (*gJavaVM)->GetEnv(gJavaVM, (void **)&env, JNI_VERSION_1_4);
    if (env == NULL) {
        LOG_ERROR("GetEnv FAILED.");
        return;
    }
    jclass cls = (*env)->GetObjectClass(env, SndChip.soundCallbackObj);
    jmethodID method = (*env)->GetMethodID(env, cls, "updateRegisters", "(III)V");
    if (method == NULL) {
        LOG_ERROR("ERROR calling sound callback");
        return;
    }
    (*env)->CallVoidMethod(env, SndChip.soundCallbackObj, method, generator,
                  SndChip.frequency[generator], SndChip.attenuation[generator]);
}


// memory implementation

static int latch = 0;
uint8_t external_read_byte(uint16_t address)
{
    if (address > 0x5FFF && address < 0x8000) {
        // console RAM/ROM
        if (mycpu.cartRAM)
            return mycpu.cartRAM[address - 0x6000];
        return 0x0;
    }

    switch (address) {
        case 0x9800:        // GROM
        case 0x9804:
            latch = 0;
            GROM.latch = 0;
            if ((GROM.address & 0x1FFF) > 0x1800) {
                LOG_ERROR("(0x%x) GROM address = 0x%x", mycpu.PC << 1, GROM.address);
//                mycpu.exception = 1;
            }
            if (GROM.address > 0x5FFF) {
                GROM.address++;
                if (mycpu.cartridge && GROM.address < 0x10000)
                    return mycpu.cartridge[GROM.address - 0x6001];
//                mycpu.exception = 1;
                return 0x0;
            }
            return GROM.memory[GROM.address++];
        case 0x9806:
        case 0x9802: {        // GROM read address
            latch = (latch + 1) % 2;
            if ((GROM.address & 0x1FFF) > 0x1800)
                LOG_ERROR("(0x%x) GROM read address BAD = 0x%x", mycpu.PC << 1, GROM.address);
            if (latch == 0) {
                return (GROM.address+1) & 0xff;
            }
            return (GROM.address+1) >> 8;
        }
        case 0x9400:            // speech synthesizer write port?
            return 0x00;
        case 0x9000:            // speech synthesizer read
            return 0x00;
        case 0x8400:            // sound chip write port
            return 0x00;
        case 0x8800:
        case 0x8804:        // need to decode this properly
            return vdpMemory[VDP.address++];
        case 0x8802:            // VDP status
            // XXX: TODO
            return 0x00;
        default:
            if (address > 0x1FFF && address < 0x4000) {
                // 8K "low memory" expansion
                return mycpu.expansion_mem[address - 0x2000];
            }
            if (address > 0x9FFF)
                // 24K "high memory" expansion
                return mycpu.expansion_mem[address - 0xA000];

            if (mycpu.peripheral_mem && address > 0x3FFF && address < 0x6000) {
                if (address >= 0x5FF0) {
                    return fdc_read_byte(address);
                }
                return mycpu.peripheral_mem[address - 0x4000];
            }

// too verbose ...
//            LOG_ERROR("(0x%x) external_read_byte 0x%x", mycpu.PC << 1, address);

            // several requests for cartridge roms
            if ((address & 0xFFF) > 0 && (address >> 8) != 0x98) {
                mycpu.exception = 1;
            }
    }
    return 0x0;
}


uint16_t external_read_word(uint16_t address)
{
    if (address > 0x1FFF && address < 0x4000) {
        return WORD_MEMORY_READ(mycpu.expansion_mem, address - 0x2000);
    }
    if (address > 0x9FFF) {
        return WORD_MEMORY_READ(mycpu.expansion_mem, address - 0xA000);
    }

    if (address > 0x3FFF && address < 0x6000) {
        // peripheral card ROM (if peripheral is enabled => peripheral_mem not NULL)
        if (mycpu.peripheral_mem)
            return WORD_MEMORY_READ(mycpu.peripheral_mem, address - 0x4000);
        return 0x00;
    }

// too verbose
//    LOG_ERROR("(0x%x) external_read_word 0x%x", mycpu.PC << 1, address);
    return 0x00;
}


void LuaTraceVDP(uint16_t address, uint8_t source);
void external_write_byte(uint16_t address, uint8_t byte)
{
    static uint8_t newaddress[2];
    static uint8_t newvdpaddress[2];

    if (address > 0x5FFF && address < 0x7000) {
        LOG_ERROR("write to cartridge RAM");
        if (mycpu.cartRAM)
            mycpu.cartRAM[address - 0x6000] = byte;
        return;
    }

    switch (address) {
        case 0x9c06:
        case 0x9c02:        // GROM set address
            GROM.latch = (GROM.latch + 1) % 2;
            latch = 0;
            newaddress[GROM.latch] = byte;
            if (GROM.latch == 0) {
                uint16_t p = *((uint16_t *)newaddress);
                if ((p & 0x1FFF) > 0x1800) {
                    LOG_ERROR("(0x%x) GROM set address 0x%x", mycpu.PC << 1, p);
                    mycpu.exception = 1;
                }
                GROM.address = p;
            }
            return;
        case 0x8c02:        // VDP set address
            VDP.latch = (VDP.latch + 1) % 2;
            newvdpaddress[VDP.latch] = byte;
            if (VDP.latch == 0) {
                uint8_t byte2 = newvdpaddress[0];
                uint8_t byte1 = newvdpaddress[1];

                if (byte2 & 0x80) {
                    // register write
                    uint8_t reg = byte2 & 0xf;
                    VDP.registers[reg] = byte1;
                    break;
                }
                else if (byte2 & 0x40) {
                    // set address for write
                    uint16_t address = ((byte2 & 0x3F) << 8) | byte1;
                    VDP.address = address;
                    break;
                }
                else {
                    uint16_t address = ((byte2 & 0x3F) << 8) | byte1;
                    VDP.address = address;
                    break;
                }
            }
            break;
        case 0x8c00:        // VDP write
            vdpMemoryCounts[VDP.address]++;
            vdpMemory[VDP.address++] = byte;
            LuaTraceVDP(VDP.address-1, byte);
            VDP.dirty = 1;
            break;
        case 0x8400: {       // sound generator
            static uint8_t generator;

            if (byte & 0x80) {
                generator = (byte >> 5) & 0x3;
                // command byte
                uint8_t reg = (byte >> 4) & 0x7;
                if (reg % 2 == 0) {
                    // frequency
                    SndChip.frequency[generator] = byte & 0xf;
                } else {
                    // attenuator
                    SndChip.attenuation[generator] = byte & 0xf;
                    soundCallback(generator);
                }
            } else {
                // bottom half of frequency
                SndChip.frequency[generator] |= (byte & 0x3f) << 4;
                soundCallback(generator);
            }
            // update sound registers (call callback)
//            soundCallback();
            break;
        }
        default:
            if (address > 0x1FFF && address < 0x4000) {
                // 8K "low memory" expansion
                mycpu.expansion_mem[address - 0x2000] = byte;
                return;
            }
            if (address > 0x9fff) {
                // 24K "high memory" expansion
                mycpu.expansion_mem[address - 0xA000] = byte;
                return;
            }

            if (mycpu.peripheral_mem && address >= 0x5FF0 && address < 0x6000) {
                fdc_write_byte(address, byte);
                return;
            }
            // too verbose
//            LOG_ERROR("(0x%x) external_write_byte 0x%x => 0x%x", mycpu.PC << 1, address, byte);
    }
    return;
}


void external_write_word(uint16_t address, uint16_t word)
{
    if (address > 0x1FFF && address < 0x4000) {
        // 8K "low memory" expansion
        mycpu.expansion_mem[address - 0x2000] = word >> 8;
        mycpu.expansion_mem[address - 0x1FFF] = word & 0xff;
        return;
    }
    if (address > 0x9FFF) {
        // 24K "high memory" expansion
        mycpu.expansion_mem[address - 0xA000] = word >> 8;
        mycpu.expansion_mem[address - 0x9FFF] = word & 0xff;
        return;
    }

    if (address == 0x8C00) {
        // VDP writes are only single byte even when accessed with
        // word instructions
        vdpMemory[VDP.address++] = word >> 8;
        return;
    }

// too verbose
//    LOG_ERROR("external_write_word 0x%x => 0x%x", address, word);
    mycpu.exception = 1;
    return;
}


uint16_t get_grom()
{
    return GROM.address;
}
