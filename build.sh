#!/bin/bash
# =================================================================
# FlashHelper 跨平台一键构建脚本 (v1.2.2)
# 支持架构: x86_64, LoongArch64
# =================================================================

set -e  # 遇错立即停止

# 1. 架构探测
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        QMAKE_CMD=$(command -v qmake6 || command -v qmake || echo "qmake")
        echo "Detected x86_64 architecture. Using $QMAKE_CMD"
        ;;
    loongarch64|la64)
        QMAKE_CMD=$(command -v qmake || echo "/usr/lib/qt5/bin/qmake")
        echo "Detected LoongArch64 architecture. Using $QMAKE_CMD"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# 2. 深度清理 (防止跨平台同步产生的 Qt 缓存冲突)
echo "--- Step 1: Cleaning environment ---"
rm -rf build
rm -rf flashrom/build
rm -f FlashHelper-${ARCH}.AppImage
rm -rf AppDir

# 3. 构建 GUI 程序 (FlashHelper)
echo "--- Step 2: Building FlashHelper GUI ---"
mkdir -p build && cd build
$QMAKE_CMD ../FlashHelper.pro
make -j$(nproc)
cd ..

# 4. 调用自动化打包脚本 (内部会处理 flashrom 的编译)
echo "--- Step 3: Building flashrom and Packaging AppImage ---"
chmod +x make_appimage.sh
./make_appimage.sh

# 5. 完成提示
echo "================================================="
echo "Build Success!"
echo "Architecture: $ARCH"
echo "Target Image: $(ls -lh FlashHelper-${ARCH}.AppImage | awk '{print $9 " (" $5 ")"}')"
echo "================================================="
