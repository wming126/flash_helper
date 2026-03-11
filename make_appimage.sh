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

# 下载 linuxdeploy 工具 (如果本地没有)
if [ ! -f linuxdeploy-x86_64.AppImage ]; then
    wget -c https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

# 下载 Qt 插件 (针对 Qt6)
if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
    wget -c https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

# 开始构建 AppImage (自动吸入所有库，包括 Qt6)
export QMAKE=/usr/bin/qmake6
./linuxdeploy-x86_64.AppImage --appdir $APPDIR --plugin qt --output appimage --desktop-file deploy/flashhelper.desktop --icon-file res/flashhelper.svg

echo "Done! FlashHelper-x86_64.AppImage has been generated."
