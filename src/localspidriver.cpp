#include "localspidriver.h"
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

// SPI Register Offsets (Loongson Specific)
#define REG_SPCR   0x00
#define REG_SPSR   0x01
#define REG_SPDR   0x02
#define REG_SPER   0x03
#define REG_PARAM  0x04
#define REG_SOFTCS 0x05
#define REG_TIME   0x06

// SPI Commands
#define CMD_WREN       0x06
#define CMD_WRDI       0x04
#define CMD_RDID       0x9F
#define CMD_RDSR       0x05
#define CMD_WRSR       0x01
#define CMD_READ       0x03
#define CMD_FAST_READ  0x0B
#define CMD_BYTE_WRITE 0x02
#define CMD_BE4K       0x20
#define CMD_BE64K      0xD8

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

    m_physBaseAddr = getRegBaseAddr();
    if (m_physBaseAddr == 0) return false;

    m_memFd = open("/dev/mem", O_RDWR | O_SYNC);
    if (m_memFd < 0) return false;

    // Map 4KB for registers (enough for SPI controller)
    uint64_t map_base = m_physBaseAddr & ~0xFFF;
    uint32_t map_offset = m_physBaseAddr & 0xFFF;
    
    m_virtBaseAddr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, m_memFd, (off_t)map_base);
    if (m_virtBaseAddr == MAP_FAILED) {
        close(m_memFd);
        m_memFd = -1;
        return false;
    }

    // Adjust virtual address to the exact register start
    m_virtBaseAddr = (uint8_t*)m_virtBaseAddr + map_offset;

    return true;
}

void LocalSpiDriver::release() {
    if (m_virtBaseAddr && m_virtBaseAddr != MAP_FAILED) {
        // Correctly calculate the mapping base for munmap
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
            if (pos != std::string::npos) {
                return line.substr(pos + 1);
            }
        }
    }
    return "";
}

uint64_t LocalSpiDriver::getRegBaseAddr() {
    // Default standard LS3A/7A bridge SPI base address
    uint64_t base = 0x1fe001f0;

    // Dynamically detect LS7A Bridge SPI Address from PCI Config Space BAR0
    // Physical address: 0xefdfe000010 (Device 22, Function 0, Offset 0x10)
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd >= 0) {
        uint64_t pci_config_spi = 0xefdfe000000ULL | (22 << 11) | 0x10;
        uint64_t map_base = pci_config_spi & ~0xFFF;
        uint32_t map_offset = pci_config_spi & 0xFFF;
        void* p = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, (off_t)map_base);
        if (p != MAP_FAILED) {
            uint32_t bar0 = *(volatile uint32_t*)((uint8_t*)p + map_offset);
            munmap(p, 4096);
            // Mask out non-address bits of BAR
            if (bar0 != 0xFFFFFFFF && bar0 != 0) {
                base = bar0 & 0xfffffff0;
            }
        }
        close(fd);
    }
    return base;
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
    // Save current state
    m_savedSpcr = regRead8(REG_SPCR);
    m_savedSpsr = regRead8(REG_SPSR);
    m_savedSper = regRead8(REG_SPER);
    m_savedParam = regRead8(REG_PARAM);

    if (m_is3C5000 || m_is3D5000) {
        // Speed down for 3C/3D5000 stability (ported from OsTools)
        regWrite8(REG_SPCR, 0x52);
        regWrite8(REG_SPSR, 0xc0);
        regWrite8(REG_SPER, 0x04);
        regWrite8(REG_PARAM, 0x20);
        regWrite8(REG_TIME, 0x01);
    } else {
        // Default performance
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
        regWrite8(REG_PARAM, 0x47); // Reset to default state
    }
}

void LocalSpiDriver::spiFlashWait() {
    int timeout = 1000000;
    while (timeout--) {
        spiFlashSetCs(true);
        spiTransferByte(CMD_RDSR);
        uint8_t sr = spiTransferByte(0x00);
        spiFlashSetCs(false);
        if (!(sr & SR_BUSY)) return;
    }
}

void LocalSpiDriver::spiFlashSetCs(bool enable) {
    // 0x01: CS Low (Enable/Select), 0x11: CS High (Disable/Deselect)
    regWrite8(REG_SOFTCS, enable ? 0x01 : 0x11);
    // Small busy delay to match OsTools' SpiFlashDelayUs(3)
    for(volatile int i=0; i<200; i++); 
}

uint8_t LocalSpiDriver::spiTransferByte(uint8_t val) {
    regWrite8(REG_SPDR, val);
    int timeout = 100000;
    while ((regRead8(REG_SPSR) & 0x01) && timeout--);
    return regRead8(REG_SPDR);
}

void LocalSpiDriver::spiWriteEnable() {
    spiFlashWait();
    spiFlashSetCs(true);
    spiTransferByte(CMD_WREN);
    spiFlashSetCs(false);
}

void LocalSpiDriver::spiDisableWriteProtection() {
    spiFlashWait();
    // Enable status register write
    spiFlashSetCs(true);
    spiTransferByte(0x50); // EWSR
    spiFlashSetCs(false);

    // Write SR to clear BP bits
    spiFlashSetCs(true);
    spiTransferByte(CMD_WRSR);
    spiTransferByte(0x00);
    spiFlashSetCs(false);
    spiFlashWait();
}

bool LocalSpiDriver::detectChip(uint8_t &manuId, uint8_t &devId, uint8_t &capaId) {
    spiFlashInit();
    spiFlashWait();
    spiFlashSetCs(true);
    spiTransferByte(CMD_RDID);
    manuId = spiTransferByte(0x00);
    devId = spiTransferByte(0x00);
    capaId = spiTransferByte(0x00);
    spiFlashSetCs(false);
    spiFlashReset();
    return (manuId != 0xFF && manuId != 0x00);
}

bool LocalSpiDriver::readFlash(uint32_t offset, uint32_t size, uint8_t *buffer) {
    spiFlashInit();
    spiFlashSetCs(true);
    spiTransferByte(CMD_FAST_READ);
    spiTransferByte((offset >> 16) & 0xFF);
    spiTransferByte((offset >> 8) & 0xFF);
    spiTransferByte(offset & 0xFF);
    spiTransferByte(0x00); // Dummy byte for fast read

    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = spiTransferByte(0x00);
    }

    spiFlashSetCs(false);
    spiFlashReset();
    return true;
}

bool LocalSpiDriver::eraseFlash(uint32_t offset, uint32_t size) {
    spiFlashInit();
    spiDisableWriteProtection();

    uint32_t pos = offset;
    while (pos < offset + size) {
        spiWriteEnable();
        spiFlashSetCs(true);
        spiTransferByte(CMD_BE4K);
        spiTransferByte((pos >> 16) & 0xFF);
        spiTransferByte((pos >> 8) & 0xFF);
        spiTransferByte(pos & 0xFF);
        spiFlashSetCs(false);
        spiFlashWait();
        pos += 4096; // 4K Block
    }

    spiFlashReset();
    return true;
}

bool LocalSpiDriver::writeFlash(uint32_t offset, uint32_t size, const uint8_t *buffer) {
    spiFlashInit();
    spiDisableWriteProtection();

    for (uint32_t i = 0; i < size; i++) {
        uint32_t pos = offset + i;
        spiWriteEnable();
        spiFlashSetCs(true);
        spiTransferByte(CMD_BYTE_WRITE);
        spiTransferByte((pos >> 16) & 0xFF);
        spiTransferByte((pos >> 8) & 0xFF);
        spiTransferByte(pos & 0xFF);
        spiTransferByte(buffer[i]);
        spiFlashSetCs(false);
        spiFlashWait();
    }

    spiFlashReset();
    return true;
}
