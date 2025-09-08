#!/bin/bash
SOUNDBOARD_DIR="$HOME/soundboard"
CONFIG_FILE="$SOUNDBOARD_DIR/config.txt"
VIRTUAL_MIC="soundboard_output"
# Enhanced dependency check for modern audio systems

# Detect if running from AppImage
if [ -n "$APPDIR" ]; then
    # Running from AppImage
    SCRIPT_DIR="$APPDIR/usr/bin"
    SOUNDBOARD_DIR="$HOME/soundboard"
    SCRIPT_PATH="$APPDIR/usr/bin/soundboard.sh"
else
    # Running standalone
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    SOUNDBOARD_DIR="$SCRIPT_DIR"
    SCRIPT_PATH="$SCRIPT_DIR/soundboard.sh"
fi

CONFIG_FILE="$SOUNDBOARD_DIR/config.txt"
VIRTUAL_MIC="soundboard_output"

# Ensure config directory exists
mkdir -p "$SOUNDBOARD_DIR"


check_script_dependencies() {
    local missing_deps=""
    local missing_count=0

    # Check each required command
    for cmd in pactl paplay xbindkeys; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing_deps="$missing_deps $cmd"
            missing_count=$((missing_count + 1))
        fi
    done
    if [ $missing_count -gt 0 ]; then
        echo "ERROR: Missing required dependencies:$missing_deps"
        echo ""
        echo "Please install the required packages:"
        echo "  Ubuntu/Debian: sudo apt install pipewire-pulse xbindkeys"
        echo "  Fedora: sudo dnf install pipewire-pulse xbindkeys"
        echo "  Arch: sudo pacman -S pipewire-pulse xbindkeys"
        echo ""
        echo "Note: If you're still using PulseAudio instead of PipeWire:"
        echo "  Ubuntu/Debian: sudo apt install pulseaudio-utils xbindkeys"
        echo ""
        return 1
    fi

    return 0
}
# Check audio system availability
check_audio_system() {
    if ! pactl info >/dev/null 2>&1; then
        echo "WARNING: Audio system not accessible"
        echo ""
        echo "Possible solutions:"
        echo "  For PipeWire: systemctl --user start pipewire pipewire-pulse"
        echo "  For PulseAudio: systemctl --user start pulseaudio"
        echo "  Check audio group: groups \$USER | grep -q audio"
        echo ""
        return 1
    fi

    # Try to detect audio system
    if pactl info | grep -qi pipewire 2>/dev/null; then
        echo "Detected: PipeWire with PulseAudio compatibility"
    elif pactl info | grep -qi pulseaudio 2>/dev/null; then
        echo "Detected: PulseAudio"
    else
        echo "Detected: Unknown audio server (but pactl works)"
    fi

    return 0
}
setup_virtual_mic() {
    if ! pactl list sources short | grep -q "$VIRTUAL_MIC"; then
        echo "Setting up virtual microphone with real mic passthrough..."
        pactl load-module module-null-sink sink_name="$VIRTUAL_MIC" sink_properties=device.description="Soundboard-Output"
        pactl load-module module-null-sink sink_name="soundboard_local" sink_properties=device.description="Soundboard-Headphones"
        pactl load-module module-null-sink sink_name="soundboard_combined" sink_properties=device.description="Soundboard-Combined"
        DEFAULT_SOURCE=$(pactl get-default-source)
        DEFAULT_SINK=$(pactl get-default-sink)
        echo "Using real microphone: $DEFAULT_SOURCE"
        echo "Using default output: $DEFAULT_SINK"
        pactl load-module module-loopback source="$DEFAULT_SOURCE" sink="soundboard_combined" latency_msec=1
        pactl load-module module-loopback source="$VIRTUAL_MIC.monitor" sink="soundboard_combined" latency_msec=1
        pactl load-module module-loopback source="soundboard_local.monitor" sink="$DEFAULT_SINK" latency_msec=1
        pactl load-module module-remap-source source_name="${VIRTUAL_MIC}_mic" source_properties=device.description="SB-Microphone" master="soundboard_combined.monitor"
        pactl set-sink-volume soundboard_local 50%
        echo "Virtual microphone created! Select 'SB-Microphone' as input in your applications."
        echo "This will now pass through both your real microphone AND soundboard audio."
        echo "Volume controls in your mixer:"
        echo "  - 'Soundboard-Headphones' = Your local volume (what you hear)"
        echo "  - 'Soundboard-Output' = Volume others hear in Discord/games"
        echo "  - 'Soundboard-Combined' = Internal mixer (leave alone)"
        xbindkeys 2>/dev/null &  # Start keybind handler
    else
        echo "Virtual microphone already exists."
    fi
}
cleanup_virtual_mic() {
    echo "Cleaning up virtual microphone setup..."
    stop_all # Silence everything
    pactl list modules short | grep -E "(soundboard|$VIRTUAL_MIC)" | cut -f1 | while read module_id; do
        pactl unload-module "$module_id" 2>/dev/null
    done

    echo "Stopping xbindkeys..."
    if pgrep xbindkeys > /dev/null; then
        killall xbindkeys 2>/dev/null
        echo "xbindkeys stopped - numpad keys restored to normal function."
    else
        echo "xbindkeys was not running."
    fi
    echo "Virtual microphone cleanup complete."
}
update_config() {
    echo "Scanning for audio files and updating config..."
    if [ ! -f "$CONFIG_FILE" ]; then
        echo "# Soundboard Configuration" > "$CONFIG_FILE"
        echo "# Format: ID|filename|keybind|description" >> "$CONFIG_FILE"
        echo "# Lines starting with # are comments" >> "$CONFIG_FILE"
        echo "" >> "$CONFIG_FILE"
    fi
    # Create temporary files to track existing data
    local existing_files_temp=$(mktemp)
    local existing_keybinds_temp=$(mktemp)
    local existing_descriptions_temp=$(mktemp)
    local max_id=0
    # Read existing config into temporary files
    while IFS='|' read -r id filename keybind description; do
        [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]] && continue

        # Only process numeric IDs for max_id calculation
        if [[ "$id" =~ ^[0-9]+$ ]] && [ "$id" -gt "$max_id" ]; then
            max_id="$id"
        fi
        # Store data in temp files
        echo "$filename|$id" >> "$existing_files_temp"
        echo "$filename|$keybind" >> "$existing_keybinds_temp"
        echo "$filename|$description" >> "$existing_descriptions_temp"
    done < "$CONFIG_FILE"
    local temp_config=$(mktemp)
    echo "# Soundboard Configuration" > "$temp_config"
    echo "# Format: ID|filename|keybind|description" >> "$temp_config"
    echo "# Lines starting with # are comments" >> "$temp_config"
    echo "" >> "$temp_config"
    # Add existing files first (maintain their IDs)
    while IFS='|' read -r filename file_id; do
        if [ -f "$SOUNDBOARD_DIR/$filename" ]; then
            local keybind=$(grep "^$filename|" "$existing_keybinds_temp" | cut -d'|' -f2)
            local description=$(grep "^$filename|" "$existing_descriptions_temp" | cut -d'|' -f2-)
            echo "$file_id|$filename|$keybind|$description" >> "$temp_config"
        fi
    done < "$existing_files_temp"
    # Add new files with new IDs
    for ext in mp3 wav ogg flac m4a; do
        for filepath in "$SOUNDBOARD_DIR"/*.$ext; do
            if [ -f "$filepath" ]; then
                local filename=$(basename "$filepath")

                # Clean filename for UTF-8 safety
                local clean_filename=$(echo "$filename" | iconv -f UTF-8 -t UTF-8//IGNORE 2>/dev/null || echo "$filename")

                # Check if file already exists
                if ! grep -q "^$clean_filename|" "$existing_files_temp"; then
                    max_id=$((max_id + 1))

                    # Create clean description from filename
                    local description="${clean_filename%.*}"
                    # Replace problematic characters with safe alternatives
                    description=$(echo "$description" | sed 's/[^[:print:]]/?/g' | sed 's/|/?/g')

                    echo "$max_id|$clean_filename||$description" >> "$temp_config"
                    echo "Added new sound: $max_id) $description"
                fi
            fi
        done
    done
    # Clean up temp files
    rm "$existing_files_temp" "$existing_keybinds_temp" "$existing_descriptions_temp"
    # Replace old config with new one
    mv "$temp_config" "$CONFIG_FILE"
    echo "Config file updated!"
}
list_sounds() {
    if [ ! -f "$CONFIG_FILE" ]; then
        echo "No config file found. Run '$0 scan' first to populate sounds."
        return
    fi
    echo "Available sounds:"
    echo "ID | Keybind | Description"
    echo "---|---------|------------"
    while IFS='|' read -r id filename keybind description; do
        [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]] && continue

        if [ -f "$SOUNDBOARD_DIR/$filename" ]; then
            local keybind_display="${keybind:-'unset'}"
            printf "%2s | %-7s | %s\n" "$id" "$keybind_display" "$description"
        fi
    done < "$CONFIG_FILE"
    # Remove end comment wrapper for path to soundboard script instead of 'soundboard'
: <<'END_COMMENT'
    echo ""
    echo "Usage examples:"
    echo "  $0 1              # Play sound #1 to default output"
    echo "  $0 1 mic          # Play sound #1 to virtual microphone"
    echo "  $0 1 both         # Play sound #1 to both outputs"
    echo "  $0 bind 1 KP_1    # Bind sound #1 to Numpad 1 (auto-updates xbindkeys)"
    echo "  $0 unbind 1       # Remove keybind from sound #1"
    echo "  $0 bind stop KP_0 # Bind stop command to a key"
    echo "  $0 volume 75      # Set local soundboard volume to 75%"
    echo "  $0 keybinds       # Show current xbindkeys config"
    echo "  $0 refresh        # Force refresh xbindkeys config"
    echo "  $0 scan           # Scan for new audio files"
    echo "  $0 setup          # Set up virtual microphone"
    echo "  $0 stop           # Stop all playing sounds"
    echo "  $0 cleanup        # Remove virtual microphone setup"
END_COMMENT
# Aliased in bashrc so we can use 'soundboard' command instead of referencing entire path
    echo ""
    echo "Usage examples:"
    echo "  soundboard 1              # Play sound #1 to default output"
    echo "  soundboard 1 mic          # Play sound #1 to virtual microphone"
    echo "  soundboard 1 both         # Play sound #1 to both outputs"
    echo "  soundboard bind 1 KP_1    # Bind sound #1 to Numpad 1 (auto-updates xbindkeys)"
    echo "  soundboard unbind 1       # Remove keybind from sound #1"
    echo "  soundboard bind stop KP_0 # Bind stop command to a key"
    echo "  soundboard volume 75      # Set local soundboard volume to 75%"
    echo "  soundboard keybinds       # Show current xbindkeys config"
    echo "  soundboard refresh        # Force refresh xbindkeys config"
    echo "  soundboard scan           # Scan for new audio files"
    echo "  soundboard setup          # Set up virtual microphone"
    echo "  soundboard stop           # Stop all playing sounds"
    echo "  soundboard cleanup        # Remove virtual microphone setup"
}
update_xbindkeys() {
    local xbindkeys_config="$HOME/.xbindkeysrc"
    local temp_config=$(mktemp)

    echo "# Soundboard xbindkeys configuration - AUTO GENERATED" > "$temp_config"
    echo "# This file is automatically managed by soundboard.sh" >> "$temp_config"
    echo "# Manual changes will be overwritten!" >> "$temp_config"
    echo "" >> "$temp_config"

    # ... existing non-soundboard keybinds code ...

    echo "# Soundboard keybinds:" >> "$temp_config"
    while IFS='|' read -r id filename keybind description; do
        [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]] || [[ -z "$keybind" ]] && continue
        if [ -f "$SOUNDBOARD_DIR/$filename" ]; then
            echo "# $description" >> "$temp_config"
            echo "\"$SCRIPT_PATH $id both\"" >> "$temp_config"  # Use SCRIPT_PATH
            echo "    $keybind" >> "$temp_config"
            echo "" >> "$temp_config"
        fi
    done < "$CONFIG_FILE"

    # Stop command handling
    if grep -q "^stop|" "$CONFIG_FILE" 2>/dev/null; then
        local stop_keybind=$(grep "^stop|" "$CONFIG_FILE" | cut -d'|' -f3)
        if [ -n "$stop_keybind" ]; then
            echo "# Stop All Sounds" >> "$temp_config"
            echo "\"$SCRIPT_PATH stop\"" >> "$temp_config"  # Use SCRIPT_PATH
            echo "    $stop_keybind" >> "$temp_config"
            echo "" >> "$temp_config"
        fi
    fi

    mv "$temp_config" "$xbindkeys_config"

    if pgrep xbindkeys > /dev/null; then
        echo "Restarting xbindkeys with updated config..."
        killall xbindkeys 2>/dev/null
        xbindkeys 2>/dev/null &
        echo "xbindkeys updated and restarted!"
    else
        echo "xbindkeys config updated. Run 'xbindkeys' to start it."
    fi
}

bind_keybind() {
    local sound_id="$1"
    local keybind="$2"
    if [ -z "$sound_id" ] || [ -z "$keybind" ]; then
        echo "Usage: $0 bind <sound_id|stop> <keybind>"
        echo "Example: $0 bind 1 KP_1"
        echo "Example: $0 bind stop KP_0"
        return 1
    fi
    local temp_config=$(mktemp)
    local conflict_found=false
    local conflict_description=""
    while IFS='|' read -r id filename old_keybind description; do
        if [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]]; then
            echo "$id|$filename|$old_keybind|$description" >> "$temp_config"
        elif [ "$old_keybind" = "$keybind" ] && [ "$id" != "$sound_id" ]; then
            echo "$id|$filename||$description" >> "$temp_config"
            conflict_found=true
            conflict_description="$description (#$id)"
        else
            echo "$id|$filename|$old_keybind|$description" >> "$temp_config"
        fi
    done < "$CONFIG_FILE"
    mv "$temp_config" "$CONFIG_FILE"
    if [ "$conflict_found" = true ]; then
        echo "Removed conflicting keybind '$keybind' from: $conflict_description"
    fi
    if [ "$sound_id" = "stop" ]; then
        if grep -q "^stop|" "$CONFIG_FILE" 2>/dev/null; then
            local temp_config2=$(mktemp)
            while IFS='|' read -r id filename old_keybind description; do
                if [ "$id" = "stop" ]; then
                    echo "stop||$keybind|Stop All Sounds" >> "$temp_config2"
                else
                    echo "$id|$filename|$old_keybind|$description" >> "$temp_config2"
                fi
            done < "$CONFIG_FILE"
            mv "$temp_config2" "$CONFIG_FILE"
        else
            echo "stop||$keybind|Stop All Sounds" >> "$CONFIG_FILE"
        fi
        echo "Bound stop command to key: $keybind"
    else
        local temp_config2=$(mktemp)
        local found=false
        while IFS='|' read -r id filename old_keybind description; do
            if [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]]; then
                echo "$id|$filename|$old_keybind|$description" >> "$temp_config2"
            elif [ "$id" = "$sound_id" ]; then
                echo "$id|$filename|$keybind|$description" >> "$temp_config2"
                found=true
                echo "Bound sound #$id ($description) to key: $keybind"
            else
                echo "$id|$filename|$old_keybind|$description" >> "$temp_config2"
            fi
        done < "$CONFIG_FILE"

        if [ "$found" = false ]; then
            echo "Sound ID $sound_id not found!"
            rm "$temp_config2"
            return 1
        fi

        mv "$temp_config2" "$CONFIG_FILE"
    fi
    update_xbindkeys
}
unbind_keybind() {
    local sound_id="$1"
    if [ -z "$sound_id" ]; then
        echo "Usage: $0 unbind <sound_id|stop>"
        echo "Example: $0 unbind 5"
        return 1
    fi
    local temp_config=$(mktemp)
    local found=false
    while IFS='|' read -r id filename old_keybind description; do
        if [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]]; then
            echo "$id|$filename|$old_keybind|$description" >> "$temp_config"
        elif [ "$id" = "$sound_id" ]; then
            echo "$id|$filename||$description" >> "$temp_config"
            found=true
            if [ -n "$old_keybind" ]; then
                echo "Removed keybind '$old_keybind' from sound #$id ($description)"
            else
                echo "Sound #$id ($description) had no keybind to remove"
            fi
        else
            echo "$id|$filename|$old_keybind|$description" >> "$temp_config"
        fi
    done < "$CONFIG_FILE"

    if [ "$found" = false ]; then
        echo "Sound ID $sound_id not found!"
        rm "$temp_config"
        return 1
    fi

    mv "$temp_config" "$CONFIG_FILE"
    update_xbindkeys
}
show_keybind_commands() {
    if [ ! -f "$CONFIG_FILE" ]; then
        echo "No config file found. Run '$0 scan' first."
        return
    fi

    echo "Current xbindkeys configuration preview:"
    echo "========================================"

    while IFS='|' read -r id filename keybind description; do
        [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]] || [[ -z "$keybind" ]] && continue

        if [ -f "$SOUNDBOARD_DIR/$filename" ]; then
            echo "# $description"
            echo "\"$SCRIPT_PATH $id both\""
            echo "    $keybind"
            echo ""
        fi
    done < "$CONFIG_FILE"

    if grep -q "^stop|" "$CONFIG_FILE" 2>/dev/null; then
        local stop_keybind=$(grep "^stop|" "$CONFIG_FILE" | cut -d'|' -f3)
        if [ -n "$stop_keybind" ]; then
            echo "# Stop All Sounds"
            echo "\"$SCRIPT_PATH $id stop\""
            echo "    $stop_keybind"
            echo ""
        fi
    fi

    echo "========================================"
    echo "This config is automatically applied when you use 'bind' command"
    echo "Manual commands:"
    echo "  $0 refresh    # Force update xbindkeys config"
    echo "  $0 unbind 5   # Remove keybind from sound #5"
}
play_sound() {
    local sound_id=$1
    local output_mode=${2:-"default"}

    if [ ! -f "$CONFIG_FILE" ]; then
        echo "No config file found. Run 'soundboard scan' first."
        return 1
    fi

    # Check if virtual mic setup exists when using mic or both modes
    if [ "$output_mode" = "mic" ] || [ "$output_mode" = "both" ]; then
        if ! pactl list sinks short | grep -q "$VIRTUAL_MIC"; then
            echo "Soundboard not set up! Run 'soundboard setup' first."
            echo "This prevents accidental audio blasts to your default device."
            return 1
        fi
    fi

    local target_file=""
    local description=""

    while IFS='|' read -r id filename keybind desc; do
        [[ "$id" =~ ^#.*$ ]] || [[ -z "$id" ]] && continue

        if [ "$id" = "$sound_id" ]; then
            if [ -f "$SOUNDBOARD_DIR/$filename" ]; then
                target_file="$SOUNDBOARD_DIR/$filename"
                description="$desc"
                break
            else
                echo "Sound file not found: $filename"
                return 1
            fi
        fi
    done < "$CONFIG_FILE"

    if [ -z "$target_file" ]; then
        echo "Sound #$sound_id not found in config!"
        return 1
    fi

    echo "Playing: $description"

    case "$output_mode" in
        "mic")
            paplay --device="$VIRTUAL_MIC" "$target_file" &
            ;;
        "both")
            paplay --device="soundboard_local" "$target_file" &
            paplay --device="$VIRTUAL_MIC" "$target_file" &
            ;;
        "default"|*)
            paplay --device="soundboard_local" "$target_file" &
            ;;
    esac
}
set_volume() {
    local volume="$1"
    if [ -z "$volume" ]; then
        local current_volume=$(pactl get-sink-volume soundboard_local 2>/dev/null | grep -oP '\d+%' | head -1)
        if [ -n "$current_volume" ]; then
            echo "Current local soundboard volume: $current_volume"
        else
            echo "Soundboard not set up yet. Run '$0 setup' first."
        fi
        return
    fi

    if ! [[ "$volume" =~ ^[0-9]+%?$ ]]; then
        echo "Invalid volume. Use a number (0-100) optionally with %"
        echo "Examples: $0 volume 50, $0 volume 75%"
        return 1
    fi

    if [[ ! "$volume" =~ % ]]; then
        volume="${volume}%"
    fi

    if pactl set-sink-volume soundboard_local "$volume" 2>/dev/null; then
        echo "Set local soundboard volume to $volume"
        echo "This affects only your local playback, not what others hear in Discord/apps"
    else
        echo "Failed to set volume. Is the soundboard set up? Try '$0 setup'"
    fi
}
stop_all() {
    echo "Stopping all soundboard audio..."
    killall paplay 2>/dev/null
    echo "All sounds stopped."
}
mkdir -p "$SOUNDBOARD_DIR"
case "$1" in
    "setup")
        setup_virtual_mic
        bind_keybind "stop" "KP_0"  #COMMENT TO REBIND STOP COMMAND
        ;;
    "cleanup")
        cleanup_virtual_mic
        ;;
    "scan")
        update_config
        ;;
    "bind")
        bind_keybind "$2" "$3"
        ;;
    "unbind")
        unbind_keybind "$2"
        ;;
    "refresh")
        update_xbindkeys
        ;;
    "volume")
        set_volume "$2"
        ;;
    "keybinds")
        show_keybind_commands
        ;;
    "list"|"")
        list_sounds
        ;;
    "stop")
        stop_all
        ;;
    [0-9]*)
        if [ "$1" -ge 1 ]; then
            play_sound "$1" "$2"
        else
            echo "Invalid sound number: $1"
            list_sounds
        fi
        ;;
    *)
        echo "Invalid option: $1"
        list_sounds
        ;;
esac
