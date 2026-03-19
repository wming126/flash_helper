# Contributing to FlashHelper

Thank you for your interest in contributing!

## Build Instructions

To build FlashHelper from source:

1. **Install dependencies**:
   ```bash
   # Debian/Ubuntu
   sudo apt update
   sudo apt install build-essential cmake qt6-base-dev qt6-svg-dev libpci-dev libusb-1.0-0-dev libftdi1-dev meson ninja-build
   ```

2. **Configure and build**:
   ```bash
   cmake -S . -B build
   cmake --build build -j$(nproc)
   ```

3. **Run tests**:
   ```bash
   cd build/tests
   ctest --output-on-failure
   ```

## Packaging

- **Build AppImage**: `bash make_appimage.sh`
- **Build DEB/RPM**:
  ```bash
  cd build
  cpack -G DEB
  cpack -G RPM
  ```

## Pre-release Checklist

Before a release, ensure the following steps are completed:

- [ ] Bump the version in `VERSION` file.
- [ ] Run full test suite (`ctest`).
- [ ] Verify manual build on both x86_64 and LoongArch64 if possible.
- [ ] Build and smoke test the AppImage.
- [ ] Verify DEB/RPM package installations.
- [ ] Update `TODO` file for the next release.
