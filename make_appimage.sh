#!/bin/bash

# 1. 确保 FlashHelper 已经编译 (由用户在外部完成或在此处编译)
if [ ! -f "build/FlashHelper" ]; then
    echo "FlashHelper binary not found in build/. Attempting to build..."
    mkdir -p build && cd build
    if [ -f "/usr/bin/qmake6" ]; then qmake6 ../FlashHelper.pro; else qmake ../FlashHelper.pro; fi
    make -j$(nproc)
    cd ..
fi

# 2. 强制重新构建 flashrom 核心 (确保架构匹配)
echo "--- Building flashrom core for current architecture ---"
cd flashrom
# 清理旧的 x86 或其他架构残留
rm -rf build
rm -f flashrom

# 尝试使用 Meson 构建 (现代推荐方式)
if command -v meson &> /dev/null && command -v ninja &> /dev/null; then
    echo "Using Meson to build flashrom..."
    meson setup build -Dprogrammer=all || meson setup build --reconfigure -Dprogrammer=all
    ninja -C build
    FLASHROM_BIN=$(realpath build/flashrom)
# 回退到 Makefile 构建
elif [ -f "Makefile" ]; then
    echo "Using Makefile to build flashrom..."
    make clean
    make -j$(nproc)
    FLASHROM_BIN=$(realpath flashrom)
else
    echo "Error: Cannot find build system for flashrom source."
    exit 1
fi
cd ..

# 3. 准备 AppDir 结构
APPDIR=AppDir
rm -rf $APPDIR
mkdir -p $APPDIR/usr/bin
mkdir -p $APPDIR/usr/lib
mkdir -p $APPDIR/usr/share/icons/hicolor/scalable/apps
mkdir -p $APPDIR/usr/share/applications

# 4. 复制基础文件
cp build/FlashHelper $APPDIR/usr/bin/
cp -v -L "$FLASHROM_BIN" "$APPDIR/usr/bin/flashrom"
chmod +x $APPDIR/usr/bin/flashrom
cp res/flashhelper.svg $APPDIR/usr/share/icons/hicolor/scalable/apps/
cp deploy/flashhelper.desktop $APPDIR/usr/share/applications/
cp deploy/flashhelper.desktop $APPDIR/
cp res/flashhelper.svg $APPDIR/
(cd $APPDIR && ln -sf usr/bin/FlashHelper AppRun)

# --- 架构与工具逻辑 ---
ARCH=$(uname -m)
if [ "$ARCH" == "loongarch64" ] || [ "$ARCH" == "la64" ]; then
    LINUXDEPLOY_BIN="tools/linuxdeploy-loongarch64.AppImage"
    APPIMAGETOOL_BIN="tools/appimagetool-loongarch64.AppImage"
    RUNTIME_FILE="tools/runtime-loongarch64"
else
    LINUXDEPLOY_BIN="tools/linuxdeploy-x86_64.AppImage"
    APPIMAGETOOL_BIN="tools/appimagetool-x86_64.AppImage"
    RUNTIME_FILE="tools/runtime-x86_64"
fi

# 5. 部署依赖
if [ -f "$LINUXDEPLOY_BIN" ]; then
    echo "Using linuxdeploy for deployment..."
    if [ -f "/usr/bin/qmake6" ]; then export QMAKE="/usr/bin/qmake6"; else export QMAKE="/usr/bin/qmake"; fi
    ./$LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg
else
    echo "linuxdeploy not found, performing manual deployment..."
    ldd $APPDIR/usr/bin/FlashHelper | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so|libGL.so|libX11.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/
    mkdir -p $APPDIR/usr/plugins/platforms
    if [ -d "/usr/lib/$(uname -m)-linux-gnu/qt5/plugins/platforms" ]; then
        cp /usr/lib/$(uname -m)-linux-gnu/qt5/plugins/platforms/libqxcb.so $APPDIR/usr/plugins/platforms/
    elif [ -d "/usr/lib/qt/plugins/platforms" ]; then
        cp /usr/lib/qt/plugins/platforms/libqxcb.so $APPDIR/usr/plugins/platforms/
    fi
    if [ -f "$APPDIR/usr/plugins/platforms/libqxcb.so" ]; then
        ldd $APPDIR/usr/plugins/platforms/libqxcb.so | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/
    fi
    echo -e "[Paths]\nPlugins = ../plugins\nLibraries = ../lib" > $APPDIR/usr/bin/qt.conf
fi

# 提取并验证 flashrom 的核心依赖
if [ -f "$APPDIR/usr/bin/flashrom" ]; then
    echo "Extracting flashrom dependencies..."
    ldd "$APPDIR/usr/bin/flashrom" | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/
fi

# 6. 封装 AppImage
echo "Generating AppImage..."
export ARCH=$ARCH
./$APPIMAGETOOL_BIN $APPDIR FlashHelper-${ARCH}.AppImage

echo "Done! FlashHelper-${ARCH}.AppImage has been generated with FRESHLY BUILT flashrom."
