#!/bin/bash

set -euo pipefail

copy_deps() {
    local target="$1"
    local exclude_regex='libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libgcc_s\.so|libstdc\+\+\.so'

    [ -f "$target" ] || return 0

    ldd "$target" | awk '/=> \// {print $3}' | grep -vE "$exclude_regex" | sort -u | while read -r dep; do
        cp -v -L --update=none "$dep" "$APPDIR/usr/lib/" || true
    done
}

find_qt_plugins_path() {
    local plugins_path=""

    if command -v qmake6 >/dev/null 2>&1; then
        plugins_path="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null | cut -d: -f2- || true)"
    fi

    if [ -z "$plugins_path" ] && command -v qmake >/dev/null 2>&1; then
        plugins_path="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null | cut -d: -f2- || true)"
    fi

    if [ -z "$plugins_path" ] && [ -d "/usr/lib/$(uname -m)-linux-gnu/qt6/plugins" ]; then
        plugins_path="/usr/lib/$(uname -m)-linux-gnu/qt6/plugins"
    elif [ -z "$plugins_path" ] && [ -d "/usr/lib/qt6/plugins" ]; then
        plugins_path="/usr/lib/qt6/plugins"
    elif [ -z "$plugins_path" ] && [ -d "/usr/lib/$(uname -m)-linux-gnu/qt5/plugins" ]; then
        plugins_path="/usr/lib/$(uname -m)-linux-gnu/qt5/plugins"
    elif [ -z "$plugins_path" ] && [ -d "/usr/lib/qt5/plugins" ]; then
        plugins_path="/usr/lib/qt5/plugins"
    fi

    printf '%s\n' "$plugins_path"
}

deploy_qt_plugin() {
    local plugins_root="$1"
    local relative_path="$2"
    local src="$plugins_root/$relative_path"
    local dest_dir="$APPDIR/usr/plugins/$(dirname "$relative_path")"

    [ -f "$src" ] || return 0

    mkdir -p "$dest_dir"
    cp -v -L "$src" "$dest_dir/"
    copy_deps "$dest_dir/$(basename "$relative_path")"
}

deploy_minimal_qt_runtime() {
    local plugins_root
    plugins_root="$(find_qt_plugins_path)"

    if [ -z "$plugins_root" ] || [ ! -d "$plugins_root" ]; then
        echo "Warning: Qt plugins path not found. AppImage may miss runtime plugins."
        return 0
    fi

    echo "Deploying minimal Qt runtime from: $plugins_root"

    deploy_qt_plugin "$plugins_root" "platforms/libqxcb.so"
    deploy_qt_plugin "$plugins_root" "imageformats/libqsvg.so"
    deploy_qt_plugin "$plugins_root" "iconengines/libqsvgicon.so"

    mkdir -p "$APPDIR/usr/bin"
    cat > "$APPDIR/usr/bin/qt.conf" <<'EOF'
[Paths]
Plugins = ../plugins
Libraries = ../lib
EOF
}

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
mkdir -p $APPDIR/usr/share/metainfo

# 4. 复制基础文件
cp build/FlashHelper $APPDIR/usr/bin/
if [ -f "build/flashhelper-helper" ]; then
    cp build/flashhelper-helper $APPDIR/usr/bin/
    chmod +x $APPDIR/usr/bin/flashhelper-helper
fi
cp -v -L "$FLASHROM_BIN" "$APPDIR/usr/bin/flashrom"
chmod +x $APPDIR/usr/bin/flashrom
cp res/flashhelper.svg $APPDIR/usr/share/icons/hicolor/scalable/apps/
cp deploy/com.robin.flashhelper.desktop $APPDIR/usr/share/applications/
cp deploy/com.robin.flashhelper.appdata.xml $APPDIR/usr/share/metainfo/
cp deploy/com.robin.flashhelper.desktop $APPDIR/
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
    echo "Using linuxdeploy for base dependency deployment..."
    ./$LINUXDEPLOY_BIN --appdir "$APPDIR" --desktop-file deploy/com.robin.flashhelper.desktop --icon-file res/flashhelper.svg
else
    echo "linuxdeploy not found, performing manual deployment..."
    copy_deps "$APPDIR/usr/bin/FlashHelper"
    copy_deps "$APPDIR/usr/bin/flashhelper-helper"
fi

deploy_minimal_qt_runtime

# 提取并验证 flashrom 的核心依赖
if [ -f "$APPDIR/usr/bin/flashrom" ]; then
    echo "Extracting flashrom dependencies..."
    copy_deps "$APPDIR/usr/bin/flashrom"
fi

# 6. 封装 AppImage
echo "Generating AppImage..."
export ARCH=$ARCH
rm -f "FlashHelper-${ARCH}.AppImage"
./$APPIMAGETOOL_BIN $APPDIR FlashHelper-${ARCH}.AppImage

echo "Done! FlashHelper-${ARCH}.AppImage has been generated with FRESHLY BUILT flashrom."
