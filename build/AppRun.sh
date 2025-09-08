#!/bin/bash
export APPDIR="$(dirname "$(readlink -f "$0")")"
export PATH="$APPDIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"

# Set up config directory
mkdir -p "$HOME/.config/soundboard"

# Launch the GUI
exec "$APPDIR/usr/bin/soundboardgui" "$@"
