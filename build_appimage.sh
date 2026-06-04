#!/bin/bash

# Exit on error
set -e

APP_NAME="DalekDefender"
APP_DIR="AppDir"

# 1. Clean up old AppDir
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/lib"
mkdir -p "$APP_DIR/usr/share/gcg"

# 2. Copy the executable
cp gcg "$APP_DIR/usr/share/gcg/"

# 3. Copy local library
# cp godot/bin/libgodot.linuxbsd.template_release.x86_64.so "$APP_DIR/usr/lib/"
#for f in $(ldd gcg |awk '{print $3}'); do cp $f "$APP_DIR/usr/lib/"; done 
sh check_libs_copy.sh gcg "$APP_DIR/usr/lib/"

# 4. Copy all required assets
cp *.lua "$APP_DIR/usr/share/gcg/"
cp *.png "$APP_DIR/usr/share/gcg/"
cp *.gd "$APP_DIR/usr/share/gcg/"
cp *.tscn "$APP_DIR/usr/share/gcg/"
cp *.wav "$APP_DIR/usr/share/gcg/"
cp *.mp3 "$APP_DIR/usr/share/gcg/"
cp project.godot "$APP_DIR/usr/share/gcg/"
# Optional assets
cp *.mp4 "$APP_DIR/usr/share/gcg/" 2>/dev/null || true

# Copy the icon to the root so appimagetool can make the .DirIcon symlink
cp game_icon.png "$APP_DIR/game_icon.png"
# Explicitly create .DirIcon just to be safe
cp game_icon.png "$APP_DIR/.DirIcon"

# Also place the icon in the standard hicolor directory for AppImageLauncher
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"
cp game_icon.png "$APP_DIR/usr/share/icons/hicolor/256x256/apps/game_icon.png"

# 5. Create AppRun script
cat > "$APP_DIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
cd "$HERE/usr/share/gcg"
exec "$HERE/usr/share/gcg/gcg" --lua start.lua "$@"
EOF
chmod +x "$APP_DIR/AppRun"

# 6. Create .desktop file
cat > "$APP_DIR/${APP_NAME}.desktop" << EOF
[Desktop Entry]
Name=${APP_NAME}
Exec=AppRun
Icon=game_icon
Type=Application
Categories=Game;
StartupWMClass=gcg
EOF

# 7. Use appimagetool to build the final AppImage
~/bin/appimagetool "$APP_DIR"

echo "AppImage created successfully!"
