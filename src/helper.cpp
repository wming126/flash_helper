#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include "localspidriver.h"

namespace {

constexpr const char *kUdevRulesPath = "/etc/udev/rules.d/z60_flashrom.rules";
constexpr const char *kUdevRulesContent =
    "# Flashrom Programmers\n"
    "# STM32 VSerprog\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"5740\", MODE:=\"0666\"\n"
    "# CH341A\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"1a86\", ATTRS{idProduct}==\"5512\", MODE:=\"0666\"\n"
    "# FT2232\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0403\", ATTRS{idProduct}==\"6010\", MODE:=\"0666\"\n"
    "# Dediprog\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"dada\", MODE:=\"0666\"\n"
    "# STLINK-V3\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"374e\", MODE:=\"0666\"\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"374f\", MODE:=\"0666\"\n"
    "# PICkit 2\n"
    "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"04d8\", ATTRS{idProduct}==\"0033\", MODE:=\"0666\"\n"
    "\n"
    "# Generic ACM and USB Serial\n"
    "KERNEL==\"ttyACM*\", MODE:=\"0666\"\n"
    "KERNEL==\"ttyUSB*\", MODE:=\"0666\"\n"
    "# Linux SPI (spidev)\n"
    "KERNEL==\"spidev*\", MODE:=\"0666\"\n";

bool writeFile(const char *path, const char *content) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open " << path << ": " << std::strerror(errno) << "\n";
        return false;
    }
    ofs << content;
    if (!ofs.good()) {
        std::cerr << "Failed to write " << path << "\n";
        return false;
    }
    return true;
}

bool reloadUdevRules() {
    return std::system("udevadm control --reload-rules >/dev/null 2>&1 && udevadm trigger >/dev/null 2>&1") == 0;
}

} // namespace

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
    std::cerr << "  install-rules         Install udev access rules\n";
    std::cerr << "  remove-rules          Remove udev access rules\n";
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
    else if (cmd == "install-rules") {
        if (!writeFile(kUdevRulesPath, kUdevRulesContent)) {
            return 5;
        }
        if (!reloadUdevRules()) {
            std::cerr << "udev rules installed, but failed to reload them.\n";
            return 6;
        }
        std::cout << "SUCCESS" << std::endl;
        return 0;
    }
    else if (cmd == "remove-rules") {
        if (unlink(kUdevRulesPath) != 0 && errno != ENOENT) {
            std::cerr << "Failed to remove " << kUdevRulesPath << ": " << std::strerror(errno) << "\n";
            return 7;
        }
        if (!reloadUdevRules()) {
            std::cerr << "udev rules removed, but failed to reload them.\n";
            return 8;
        }
        std::cout << "SUCCESS" << std::endl;
        return 0;
    }

    return 1;
}
