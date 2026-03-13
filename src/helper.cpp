#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include "localspidriver.h"

/**
 * @brief Simple CLI Helper for FlashHelper to access local SPI hardware.
 * Designed to be run via pkexec for root privileges.
 */
void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [command] [args...]\n";
    std::cerr << "Commands:\n";
    std::cerr << "  detect                Detect flash chip\n";
    std::cerr << "  read <file> <size>    Read flash to file\n";
    std::cerr << "  write <file>          Write file to flash (auto erase)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    LocalSpiDriver driver;
    std::string cmd = argv[1];

    if (cmd == "detect") {
        if (!driver.init()) {
            std::cerr << "Failed to init SPI driver. Check root permissions.\n";
            return 2;
        }
        uint8_t m, d, c;
        if (driver.detectChip(m, d, c)) {
            std::cout << "SUCCESS: " << std::hex << std::uppercase << (int)m << " " << (int)d << " " << (int)c << std::endl;
        } else {
            std::cerr << "No chip detected.\n";
            driver.release();
            return 3;
        }
        driver.release();
        return 0;
    } 
    else if (cmd == "read") {
        if (argc < 4) return 1;
        const char* filename = argv[2];
        uint32_t size = std::stoul(argv[3]);

        if (!driver.init()) return 2;
        std::vector<uint8_t> buffer(size);
        if (driver.readFlash(0, size, buffer.data())) {
            std::ofstream ofs(filename, std::ios::binary);
            if (ofs.write((char*)buffer.data(), size)) {
                std::cout << "SUCCESS" << std::endl;
            }
        }
        driver.release();
        return 0;
    }
    else if (cmd == "write") {
        if (argc < 3) return 1;
        const char* filename = argv[2];
        
        std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) return 4;
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        ifs.read((char*)buffer.data(), size);

        if (!driver.init()) return 2;
        driver.eraseFlash(0, size);
        if (driver.writeFlash(0, size, buffer.data())) {
            std::cout << "SUCCESS" << std::endl;
        }
        driver.release();
        return 0;
    }

    return 1;
}
