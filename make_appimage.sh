#!/bin/bash

# 确保 build 已经编译
cd build
make -j$(nproc)
cd ..

# 准备 AppDir 结构
APPDIR=AppDir
rm -rf $APPDIR
mkdir -p $APPDIR/usr/bin
mkdir -p $APPDIR/usr/share/icons/hicolor/scalable/apps
mkdir -p $APPDIR/usr/share/applications

# 复制文件
cp build/FlashHelper $APPDIR/usr/bin/
cp res/flashhelper.svg $APPDIR/usr/share/icons/hicolor/scalable/apps/
cp deploy/flashhelper.desktop $APPDIR/usr/share/applications/

# 将 flashrom 核心也打包进去
FLASHROM_BIN=$(which flashrom)
if [ -n "$FLASHROM_BIN" ]; then
    cp $FLASHROM_BIN $APPDIR/usr/bin/flashrom
    echo "Found and bundled flashrom: $FLASHROM_BIN"
fi

# --- 架构与工具逻辑 ---
ARCH=$(uname -m)
if [ "$ARCH" == "loongarch64" ]; then
    LINUXDEPLOY_BIN="linuxdeploy-loongarch64.AppImage"
    APPIMAGETOOL_BIN="appimagetool-loongarch64.AppImage"
    RUNTIME_FILE="runtime-loongarch64"
else
    LINUXDEPLOY_BIN="linuxdeploy-x86_64.AppImage"
    APPIMAGETOOL_BIN="appimagetool-x86_64.AppImage"
    RUNTIME_FILE="runtime-x86_64"
fi

# 确保工具存在
if [ ! -f "$LINUXDEPLOY_BIN" ]; then
    echo "Error: $LINUXDEPLOY_BIN not found."
    exit 1
fi
if [ ! -f "$APPIMAGETOOL_BIN" ]; then
    echo "Error: $APPIMAGETOOL_BIN not found."
    exit 1
fi
if [ ! -f "$RUNTIME_FILE" ]; then
    echo "Error: $RUNTIME_FILE not found."
    exit 1
fi

# 1. 使用 linuxdeploy 准备 AppDir (不直接输出 appimage，避免它联网)
export QMAKE=/usr/bin/qmake6
./$LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg

# 2. 手动运行 appimagetool (强制离线模式)
export APPIMAGETOOL_RUNTIME_FILE=$(realpath "$RUNTIME_FILE")
./$APPIMAGETOOL_BIN $APPDIR FlashHelper-${ARCH}.AppImage

echo "Done! FlashHelper-${ARCH}.AppImage has been generated."
