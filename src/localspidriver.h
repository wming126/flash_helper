#ifndef LOCALSPIDRIVER_H
#define LOCALSPIDRIVER_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Loongson SPI Controller Hardware Driver
 * Ported and enhanced from OsTools/LoongArch/spi.c
 */
class LocalSpiDriver {
public:
    LocalSpiDriver();
    ~LocalSpiDriver();

    // Hardware Lifecycle
    bool init();
    void release();

    // Chip Operations
    bool detectChip(uint8_t &manuId, uint8_t &devId, uint8_t &capaId);
    bool readFlash(uint32_t offset, uint32_t size, uint8_t *buffer);
    bool eraseFlash(uint32_t offset, uint32_t size);
    bool writeFlash(uint32_t offset, uint32_t size, const uint8_t *buffer);

    // Helpers
    std::string getCpuModel();
    uint64_t getRegBaseAddr();
    uint32_t getFlashSize() const { return m_flashSize; }
    void setFlashSize(uint32_t size) { m_flashSize = size; }

private:
    // Low-level SPI primitives
    void spiFlashInit();
    void spiFlashReset();
    uint8_t spiFlashWait();
    void spiFlashSetCs(bool enable);
    uint8_t spiTransferByte(uint8_t val);
    void spiWriteEnable();
    void spiDisableWriteProtection();
    void spiEnableWriteProtection();

    // Register Access
    void regWrite8(uint8_t reg, uint8_t val);
    uint8_t regRead8(uint8_t reg);

    // State
    uint64_t m_physBaseAddr = 0;
    void* m_virtBaseAddr = nullptr;
    int m_memFd = -1;
    uint32_t m_flashSize = 0x800000; // Default 8MB

    // CPU Flags for 3C/3D5000 workaround
    bool m_is3C5000 = false;
    bool m_is3D5000 = false;

    // Saved register states for restoration
    uint8_t m_savedSpcr = 0xFF;
    uint8_t m_savedSpsr = 0xFF;
    uint8_t m_savedSper = 0xFF;
    uint8_t m_savedParam = 0xFF;
};

#endif // LOCALSPIDRIVER_H
