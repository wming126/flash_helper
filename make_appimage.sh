#!/bin/bash

# 确保 build 已经编译
if [ ! -f "build/FlashHelper" ]; then
    echo "Error: build/FlashHelper not found. Please build the project first."
    exit 1
fi

# 准备 AppDir 结构
APPDIR=AppDir
rm -rf $APPDIR
mkdir -p $APPDIR/usr/bin
mkdir -p $APPDIR/usr/lib
mkdir -p $APPDIR/usr/share/icons/hicolor/scalable/apps
mkdir -p $APPDIR/usr/share/applications

# 复制基础文件
cp build/FlashHelper $APPDIR/usr/bin/
cp res/flashhelper.svg $APPDIR/usr/share/icons/hicolor/scalable/apps/
cp deploy/flashhelper.desktop $APPDIR/usr/share/applications/
cp deploy/flashhelper.desktop $APPDIR/
cp res/flashhelper.svg $APPDIR/
(cd $APPDIR && ln -sf usr/bin/FlashHelper AppRun)

# 将 flashrom 核心打包进去
if [ -f "flashrom/build/flashrom" ]; then
    FLASHROM_BIN="flashrom/build/flashrom"
elif [ -f "flashrom/flashrom" ]; then
    FLASHROM_BIN="flashrom/flashrom"
else
    FLASHROM_BIN=$(which flashrom)
fi

if [ -n "$FLASHROM_BIN" ]; then
    cp $FLASHROM_BIN $APPDIR/usr/bin/flashrom
    chmod +x $APPDIR/usr/bin/flashrom
fi

# --- 架构与工具逻辑 ---
ARCH=$(uname -m)
if [ "$ARCH" == "loongarch64" ] || [ "$ARCH" == "la64" ]; then
    LINUXDEPLOY_BIN="tools/linuxdeploy-loongarch64.AppImage"
    APPIMAGETOOL_BIN="tools/appimagetool-loongarch64.AppImage"
    RUNTIME_FILE="tools/runtime-loongarch64"
    LIB_DIR="/usr/lib/loongarch64-linux-gnu"
else
    LINUXDEPLOY_BIN="tools/linuxdeploy-x86_64.AppImage"
    APPIMAGETOOL_BIN="tools/appimagetool-x86_64.AppImage"
    RUNTIME_FILE="tools/runtime-x86_64"
    LIB_DIR="/usr/lib/x86_64-linux-gnu"
fi

# 1. 尝试使用 linuxdeploy
if [ -f "$LINUXDEPLOY_BIN" ]; then
    echo "Using linuxdeploy for deployment..."
    if [ -f "/usr/bin/qmake6" ]; then export QMAKE="/usr/bin/qmake6"; else export QMAKE="/usr/bin/qmake"; fi
    ./$LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg
else
    echo "linuxdeploy not found, performing manual deployment..."
    # 手动提取 Qt 和 核心依赖 (针对 LoongArch 优化)
    ldd $APPDIR/usr/bin/FlashHelper | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so|libGL.so|libX11.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/
    
    # 复制 Qt 平台插件
    mkdir -p $APPDIR/usr/plugins/platforms
    if [ -d "/usr/lib/$(uname -m)-linux-gnu/qt5/plugins/platforms" ]; then
        cp /usr/lib/$(uname -m)-linux-gnu/qt5/plugins/platforms/libqxcb.so $APPDIR/usr/plugins/platforms/
    elif [ -d "/usr/lib/qt/plugins/platforms" ]; then
        cp /usr/lib/qt/plugins/platforms/libqxcb.so $APPDIR/usr/plugins/platforms/
    fi
    
    # 提取插件依赖
    if [ -f "$APPDIR/usr/plugins/platforms/libqxcb.so" ]; then
        ldd $APPDIR/usr/plugins/platforms/libqxcb.so | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/
    fi
    
    # 创建 qt.conf
    echo -e "[Paths]\nPlugins = ../plugins\nLibraries = ../lib" > $APPDIR/usr/bin/qt.conf
fi

# 确保 flashrom 的依赖也在
ldd $APPDIR/usr/bin/flashrom | grep '=> /' | grep -vE 'libc.so|libm.so|libpthread.so|libdl.so|librt.so|libgcc_s.so|libstdc++.so' | awk '{print $3}' | xargs -I {} cp -vn {} $APPDIR/usr/lib/

# 2. 封装 AppImage
echo "Generating AppImage..."
export ARCH=$ARCH
./$APPIMAGETOOL_BIN $APPDIR FlashHelper-${ARCH}.AppImage

echo "Done! FlashHelper-${ARCH}.AppImage has been generated."
