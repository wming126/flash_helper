#include "localspidriver.h"
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

// SPI Register Offsets (Strictly from OsTools/LoongArch/spi.c)
#define REG_SPCR   0x00
#define REG_SPSR   0x01
#define REG_SPDR   0x02
#define REG_SPER   0x03
#define REG_PARAM  0x04
#define REG_SOFTCS 0x05
#define REG_TIME   0x06

// SPI Commands (Strictly from OsTools/LoongArch/spi.c)
#define CMD_WREN       0x06
#define CMD_WRDI       0x04
#define CMD_RDSR       0x05
#define CMD_WRSR       0x01
#define CMD_FAST_READ  0x0B
#define CMD_BYTE_WRITE 0x02
#define CMD_BE4K       0x20
#define CMD_EWSR       0x50

// Status Register Bits
#define SR_BUSY        (1 << 0)
#define SR_BP0         (1 << 2)
#define SR_BP1         (1 << 3)
#define SR_BP2         (1 << 4)

LocalSpiDriver::LocalSpiDriver() {}

LocalSpiDriver::~LocalSpiDriver() {
    release();
}

bool LocalSpiDriver::init() {
    std::string cpuModel = getCpuModel();
    m_is3C5000 = (cpuModel.find("3C5000") != std::string::npos);
    m_is3D5000 = (cpuModel.find("3D5000") != std::string::npos);

    m_physBaseAddr = getRegBaseAddr(); // Returns 0x1fe001f0

    m_memFd = open("/dev/mem", O_RDWR | O_SYNC);
    if (m_memFd < 0) return false;

    // Map the register base
    uint64_t map_base = m_physBaseAddr & ~0xFFF;
    uint32_t map_offset = m_physBaseAddr & 0xFFF;
    
    m_virtBaseAddr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, m_memFd, (off_t)map_base);
    if (m_virtBaseAddr == MAP_FAILED) {
        close(m_memFd);
        m_memFd = -1;
        return false;
    }

    m_virtBaseAddr = (uint8_t*)m_virtBaseAddr + map_offset;
    return true;
}

void LocalSpiDriver::release() {
    if (m_virtBaseAddr && m_virtBaseAddr != MAP_FAILED) {
        uint64_t map_offset = m_physBaseAddr & 0xFFF;
        munmap((uint8_t*)m_virtBaseAddr - map_offset, 4096);
        m_virtBaseAddr = nullptr;
    }
    if (m_memFd >= 0) {
        close(m_memFd);
        m_memFd = -1;
    }
}

std::string LocalSpiDriver::getCpuModel() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) return line.substr(pos + 1);
        }
    }
    return "";
}

uint64_t LocalSpiDriver::getRegBaseAddr() {
    // Strictly OsTools: Hardcoded address for CPU SPI0 (BIOS)
    return 0x1fe001f0;
}

void LocalSpiDriver::regWrite8(uint8_t reg, uint8_t val) {
    if (!m_virtBaseAddr) return;
    *((volatile uint8_t*)m_virtBaseAddr + reg) = val;
}

uint8_t LocalSpiDriver::regRead8(uint8_t reg) {
    if (!m_virtBaseAddr) return 0;
    return *((volatile uint8_t*)m_virtBaseAddr + reg);
}

void LocalSpiDriver::spiFlashInit() {
    if (m_savedSpcr == 0xFF) {
        m_savedSpcr = regRead8(REG_SPCR);
        m_savedSpsr = regRead8(REG_SPSR);
        m_savedSper = regRead8(REG_SPER);
        m_savedParam = regRead8(REG_PARAM);
    }

    if (m_is3C5000 || m_is3D5000) {
        regWrite8(REG_SPCR, 0x52);
        regWrite8(REG_SPSR, 0xc0);
        regWrite8(REG_SPER, 0x04);
        regWrite8(REG_PARAM, 0x20);
        regWrite8(REG_TIME, 0x01);
    } else {
        regWrite8(REG_SPCR, 0x50);
        regWrite8(REG_SPSR, 0xc0);
        regWrite8(REG_SPER, 0x05);
        regWrite8(REG_PARAM, 0x40);
        regWrite8(REG_TIME, 0x01);
    }
}

void LocalSpiDriver::spiFlashReset() {
    if (m_savedSpcr != 0xFF) {
        regWrite8(REG_SPCR, m_savedSpcr);
        regWrite8(REG_SPSR, m_savedSpsr);
        regWrite8(REG_SPER, m_savedSper);
        regWrite8(REG_PARAM, m_savedParam);
        regWrite8(REG_PARAM, 0x47); // Reset SFC Param Reg
        m_savedSpcr = 0xFF;
    }
}

uint8_t LocalSpiDriver::spiFlashWait() {
    uint8_t Ret;
    int TimeOut = 1000000; // Increased timeout
    do {
        // SpiFlashReadStatus inline
        regWrite8(REG_SOFTCS, 0x01);
        spiTransferByte(CMD_RDSR);
        Ret = spiTransferByte(0x00);
        regWrite8(REG_SOFTCS, 0x11);

        if (!(Ret & 0x01)) break;
        
        // Small delay in polling loop
        for(volatile int i=0; i<1000; i++); 
    } while (TimeOut--);
    return Ret;
}

void LocalSpiDriver::spiFlashSetCs(bool enable) {
    regWrite8(REG_SOFTCS, enable ? 0x01 : 0x11);
    // Increased delay significantly to be safe on high-frequency CPUs
    for(volatile int i=0; i<5000; i++); 
}

uint8_t LocalSpiDriver::spiTransferByte(uint8_t val) {
    regWrite8(REG_SPDR, val);
    int timeout = 1000000;
    while ((regRead8(REG_SPSR) & 0x01) && timeout--);
    return regRead8(REG_SPDR);
}

void LocalSpiDriver::spiWriteEnable() {
    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_WREN);
    regWrite8(REG_SOFTCS, 0x11);
    // OsTools often has a wait or delay after WREN
    spiFlashWait(); 
}

void LocalSpiDriver::spiDisableWriteProtection() {
    // Strictly OsTools: SpiFlashDisableWriteProtection
    uint8_t Val = spiFlashWait();
    Val &= ~(SR_BP0 | SR_BP1 | SR_BP2);

    // SpiFlashWriteStatus(Val)
    // 1. SpiFlashEWRS()
    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_EWSR);
    regWrite8(REG_SOFTCS, 0x11);

    // 2. WRSR
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_WRSR);
    spiTransferByte(Val);
    regWrite8(REG_SOFTCS, 0x11);

    // 3. SpiFlashWriteDisable()
    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_WRDI);
    regWrite8(REG_SOFTCS, 0x11);

    spiFlashWait();
}

void LocalSpiDriver::spiEnableWriteProtection() {
    uint8_t Val = (SR_BP0 | SR_BP1 | SR_BP2);
    
    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_EWSR);
    regWrite8(REG_SOFTCS, 0x11);

    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_WRSR);
    spiTransferByte(Val);
    regWrite8(REG_SOFTCS, 0x11);

    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_WRDI);
    regWrite8(REG_SOFTCS, 0x11);

    spiFlashWait();
}

bool LocalSpiDriver::detectChip(uint8_t &manuId, uint8_t &devId, uint8_t &capaId) {
    spiFlashInit();
    spiFlashWait();
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(0x9F); // RDID
    manuId = spiTransferByte(0x00);
    devId = spiTransferByte(0x00);
    capaId = spiTransferByte(0x00);
    regWrite8(REG_SOFTCS, 0x11);
    spiFlashReset();
    return (manuId != 0xFF && manuId != 0x00);
}

bool LocalSpiDriver::readFlash(uint32_t offset, uint32_t size, uint8_t *buffer) {
    spiFlashInit();
    spiFlashWait(); // Wait before starting read
    printf("[READ] Starting read at 0x%X, size %u bytes...\n", offset, size);
    printf("Read: 0%%\n");
    fflush(stdout);
    regWrite8(REG_SOFTCS, 0x01);
    spiTransferByte(CMD_FAST_READ);
    spiTransferByte((offset >> 16) & 0xff);
    spiTransferByte((offset >> 8) & 0xff);
    spiTransferByte(offset & 0xff);
    spiTransferByte(0x00); // Dummy
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = spiTransferByte(0x00);
        if ((i % 65536) == 0 || i == size - 1) {
            printf("Read: %d%%\n", (int)((uint64_t)(i + 1) * 100 / size));
            fflush(stdout);
        }
    }
    regWrite8(REG_SOFTCS, 0x11);
    spiFlashReset();
    printf("[READ] Completed successfully.\n");
    fflush(stdout);
    return true;
}

bool LocalSpiDriver::eraseFlash(uint32_t offset, uint32_t size) {
    spiFlashInit();
    spiDisableWriteProtection();
    uint32_t pos = offset;
    printf("[ERASE] Starting erase at 0x%X, size %u bytes...\n", offset, size);
    printf("Erase: 0%%\n");
    fflush(stdout);
    while (pos < offset + size) {
        spiWriteEnable();
        // spiWriteEnable now includes a spiFlashWait
        regWrite8(REG_SOFTCS, 0x01);
        spiTransferByte(CMD_BE4K);
        spiTransferByte((pos >> 16) & 0xff);
        spiTransferByte((pos >> 8) & 0xff);
        spiTransferByte(pos & 0xff);
        regWrite8(REG_SOFTCS, 0x11);
        spiFlashWait();
        pos += 4096;
        
        if ((pos % (64 * 1024)) == 0 || pos >= offset + size) {
            printf("Erase: %d%%\n", (int)((uint64_t)(pos - offset) * 100 / size));
            fflush(stdout);
        }
    }
    spiEnableWriteProtection();
    spiFlashReset();
    printf("[ERASE] Completed successfully.\n");
    fflush(stdout);
    return true;
}

bool LocalSpiDriver::writeFlash(uint32_t offset, uint32_t size, const uint8_t *buffer) {
    spiFlashInit();
    spiDisableWriteProtection();
    printf("[WRITE] Starting write at 0x%X, size %u bytes...\n", offset, size);
    printf("Write: 0%%\n");
    fflush(stdout);
    for (uint32_t i = 0; i < size; i++) {
        uint32_t pos = offset + i;
        spiWriteEnable();
        // spiWriteEnable now includes a spiFlashWait
        regWrite8(REG_SOFTCS, 0x01);
        spiTransferByte(CMD_BYTE_WRITE);
        spiTransferByte((pos >> 16) & 0xff);
        spiTransferByte((pos >> 8) & 0xff);
        spiTransferByte(pos & 0xff);
        spiTransferByte(buffer[i]);
        regWrite8(REG_SOFTCS, 0x11);
        spiFlashWait();

        if ((i % 16384) == 0 || i == size - 1) {
            printf("Write: %d%%\n", (int)((uint64_t)(i + 1) * 100 / size));
            fflush(stdout);
        }
    }
    spiEnableWriteProtection();
    spiFlashReset();
    printf("[WRITE] Completed successfully.\n");
    fflush(stdout);
    return true;
}
