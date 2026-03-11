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

# 将 flashrom 核心也打包进去 (实现真正的零依赖)
FLASHROM_BIN=$(which flashrom)
if [ -n "$FLASHROM_BIN" ]; then
    cp $FLASHROM_BIN $APPDIR/usr/bin/flashrom
    echo "Found and bundled flashrom: $FLASHROM_BIN"
fi

# --- 架构与离线构建逻辑 ---
ARCH=$(uname -m)
if [ "$ARCH" == "loongarch64" ]; then
    echo "Detected LoongArch architecture."
    LINUXDEPLOY_BIN="linuxdeploy-loongarch64.AppImage"
    APPIMAGETOOL_BIN="appimagetool-loongarch64.AppImage"
    RUNTIME_FILE="runtime-loongarch64"
    
    # 强制离线：指定本地 runtime
    if [ -f "$RUNTIME_FILE" ]; then
        export APPIMAGETOOL_RUNTIME_FILE=$(realpath "$RUNTIME_FILE")
        echo "Using local runtime: $APPIMAGETOOL_RUNTIME_FILE"
    fi

    # 检查 linuxdeploy
    if [ ! -f $LINUXDEPLOY_BIN ]; then
        SYSTEM_LINUXDEPLOY=$(which linuxdeploy)
        if [ -n "$SYSTEM_LINUXDEPLOY" ]; then
            LINUXDEPLOY_BIN=$SYSTEM_LINUXDEPLOY
            echo "Using system linuxdeploy: $LINUXDEPLOY_BIN"
        else
            echo "Error: linuxdeploy not found. Please install it or provide $LINUXDEPLOY_BIN"
            exit 1
        fi
    fi
    
    # 强制离线：指定本地 appimagetool
    if [ -f "$APPIMAGETOOL_BIN" ]; then
        export APPIMAGETOOL=$(realpath "$APPIMAGETOOL_BIN")
        echo "Using local appimagetool: $APPIMAGETOOL"
    fi
else
    LINUXDEPLOY_BIN="linuxdeploy-x86_64.AppImage"
    RUNTIME_FILE="runtime-x86_64"

    # 强制离线：指定本地 runtime
    if [ -f "$RUNTIME_FILE" ]; then
        export APPIMAGETOOL_RUNTIME_FILE=$(realpath "$RUNTIME_FILE")
        echo "Using local runtime: $APPIMAGETOOL_RUNTIME_FILE"
    fi

    # 下载 linuxdeploy (如果缺失)
    if [ ! -f $LINUXDEPLOY_BIN ]; then
        wget -c https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$LINUXDEPLOY_BIN
        chmod +x $LINUXDEPLOY_BIN
    fi
    # 下载 Qt 插件 (如果缺失)
    if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
        wget -c https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
        chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
    fi
fi

# 开始构建 AppImage (自动吸入所有库)
export QMAKE=/usr/bin/qmake6
if [[ "$LINUXDEPLOY_BIN" == *"AppImage" ]]; then
    ./$LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --output appimage --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg
else
    $LINUXDEPLOY_BIN --appdir $APPDIR --plugin qt --output appimage --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg
fi

echo "Done! FlashHelper-${ARCH}.AppImage has been generated."
