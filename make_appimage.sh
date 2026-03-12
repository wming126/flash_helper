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
# 优先级：本地源码编译的 build/flashrom -> 系统 flashrom
if [ -f "flashrom/build/flashrom" ]; then
    FLASHROM_BIN="flashrom/build/flashrom"
    echo "Found and bundling custom compiled flashrom: $FLASHROM_BIN"
elif [ -f "flashrom/flashrom" ]; then
    # 针对 Makefile 编译的情况
    FLASHROM_BIN="flashrom/flashrom"
    echo "Found and bundling custom compiled flashrom: $FLASHROM_BIN"
else
    FLASHROM_BIN=$(which flashrom)
    echo "Found and bundled system flashrom: $FLASHROM_BIN"
fi

if [ -n "$FLASHROM_BIN" ]; then
    cp $FLASHROM_BIN $APPDIR/usr/bin/flashrom
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

# 1. 使用 linuxdeploy 准备 AppDir
if [ -f "/usr/bin/qmake6" ]; then
    export QMAKE="/usr/bin/qmake6"
elif [ -f "/usr/bin/qmake" ]; then
    export QMAKE="/usr/bin/qmake"
fi
./$LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg

# --- 强制更新 flashrom 核心 ---
# 无论 linuxdeploy 做了什么，我们在这里强行覆盖为最新的版本
if [ -f "flashrom/build/flashrom" ]; then
    cp -v flashrom/build/flashrom $APPDIR/usr/bin/flashrom
    echo "CRITICAL: Forced update AppDir/usr/bin/flashrom from flashrom/build/flashrom"
elif [ -f "flashrom/flashrom" ]; then
    cp -v flashrom/flashrom $APPDIR/usr/bin/flashrom
    echo "CRITICAL: Forced update AppDir/usr/bin/flashrom from flashrom/flashrom"
fi

# 确保它有可执行权限
chmod +x $APPDIR/usr/bin/flashrom

# 验证打包进 AppDir 的版本
echo "Verifying bundled flashrom version in AppDir:"
./$APPDIR/usr/bin/flashrom --version

# 2. 手动运行 appimagetool (强制离线模式)
export APPIMAGETOOL_RUNTIME_FILE=$(realpath "$RUNTIME_FILE")
./$APPIMAGETOOL_BIN $APPDIR FlashHelper-${ARCH}.AppImage

echo "Done! FlashHelper-${ARCH}.AppImage has been generated."
