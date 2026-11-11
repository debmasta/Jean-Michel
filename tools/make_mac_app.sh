#!/bin/bash
# Makes "FM-1 Editor.app" in ~/Applications: the web editor (web/editor.html of this folder) in its own window,
# without the browser's bar, with its own icon. It opens the editor of this folder, so a `git pull` updates it.
# Needs Google Chrome, Microsoft Edge or Chromium (Web MIDI). Run again to rebuild it.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
ED="$HERE/web/editor.html"
APP="$HOME/Applications/FM-1 Editor.app"
[ -f "$ED" ] || { echo "editor.html introuvable : $ED"; exit 1; }

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cat > "$APP/Contents/MacOS/fm1editor" <<EOF
#!/bin/bash
ED="$ED"
DATA="\$HOME/Library/Application Support/FM-1 Editor"
for B in "Google Chrome" "Microsoft Edge" "Chromium"; do
  if open -Ra "\$B" 2>/dev/null; then
    open -na "\$B" --args --app="file://\$ED" --user-data-dir="\$DATA" --window-size=1300,950
    exit 0
  fi
done
osascript -e 'display alert "FM-1 Editor" message "Il faut Google Chrome, Microsoft Edge ou Chromium (pour le MIDI)."'
EOF
chmod +x "$APP/Contents/MacOS/fm1editor"

cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>FM-1 Editor</string>
  <key>CFBundleDisplayName</key><string>FM-1 Editor</string>
  <key>CFBundleIdentifier</key><string>com.debmaster.fm1editor</string>
  <key>CFBundleVersion</key><string>1.0</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleExecutable</key><string>fm1editor</string>
  <key>CFBundleIconFile</key><string>icon</string>
  <key>LSUIElement</key><false/>
</dict>
</plist>
EOF

# the icon, from the editor's own (web/icon-512.png)
if [ -f "$HERE/web/icon-512.png" ] && command -v iconutil >/dev/null; then
  SET="$(mktemp -d)/icon.iconset"
  mkdir -p "$SET"
  for s in 16 32 128 256 512; do
    sips -z $s $s "$HERE/web/icon-512.png" --out "$SET/icon_${s}x${s}.png" >/dev/null
    d=$((s * 2)); [ $d -le 512 ] && sips -z $d $d "$HERE/web/icon-512.png" --out "$SET/icon_${s}x${s}@2x.png" >/dev/null
  done
  iconutil -c icns "$SET" -o "$APP/Contents/Resources/icon.icns" || true
fi
touch "$APP"
echo "OK : $APP"
echo "Ouvre-le depuis le Launchpad ou le Finder (Applications de ton dossier) ; glisse-le dans le Dock pour l'avoir sous la main."
