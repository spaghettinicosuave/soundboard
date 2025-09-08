# Soundboard Makefile

# Compiler and flags
CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0` -lm

# Directories
SRC_DIR = src
BUILD_DIR = build
APPDIR = soundboard.AppDir

# Target executable
TARGET = soundboardgui

# Build tools (downloaded automatically)
LINUXDEPLOY = linuxdeploy-x86_64.AppImage
APPIMAGETOOL = appimagetool-x86_64.AppImage

# Default target
all: $(TARGET)

# Compile the C program
$(TARGET): $(SRC_DIR)/soundboardgui.c
	$(CC) -o $(TARGET) $(SRC_DIR)/soundboardgui.c $(CFLAGS) $(LIBS)

# Download build tools
$(LINUXDEPLOY):
	wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
	chmod +x $(LINUXDEPLOY)

$(APPIMAGETOOL):
	wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x $(APPIMAGETOOL)

# Create AppImage
appimage: $(TARGET) $(LINUXDEPLOY) $(APPIMAGETOOL)
	@echo "Creating AppImage..."

	# Create AppDir structure
	mkdir -p $(APPDIR)/usr/bin
	mkdir -p $(APPDIR)/usr/share/applications

	# Copy files
	cp $(TARGET) $(APPDIR)/usr/bin/
	cp $(SRC_DIR)/soundboard.sh $(APPDIR)/usr/bin/
	cp $(BUILD_DIR)/AppRun $(APPDIR)/
	cp $(BUILD_DIR)/soundboard.desktop $(APPDIR)/
	cp $(BUILD_DIR)/soundboard.desktop $(APPDIR)/usr/share/applications/
	cp $(BUILD_DIR)/soundboard.png $(APPDIR)/

	# Set permissions
	chmod +x $(APPDIR)/AppRun
	chmod +x $(APPDIR)/usr/bin/soundboard.sh

	# Bundle dependencies
	./$(LINUXDEPLOY) --appdir $(APPDIR) --executable $(APPDIR)/usr/bin/$(TARGET) --desktop-file $(APPDIR)/soundboard.desktop --icon-file $(APPDIR)/soundboard.png

	# Create final AppImage
	./$(APPIMAGETOOL) $(APPDIR) Soundboard-x86_64.AppImage

	@echo "AppImage created: Soundboard-x86_64.AppImage"

# Install dependencies (Arch Linux)
# Install dependencies
deps:
	@echo "Installing build dependencies..."
	@if command -v pacman >/dev/null 2>&1; then \
		echo "Detected Arch Linux"; \
		sudo pacman -S base-devel gtk3; \
	elif command -v apt >/dev/null 2>&1; then \
		echo "Detected Debian/Ubuntu"; \
		sudo apt update && sudo apt install build-essential libgtk-3-dev; \
	elif command -v dnf >/dev/null 2>&1; then \
		echo "Detected Fedora"; \
		sudo dnf install gcc gtk3-devel pkg-config; \
	elif command -v zypper >/dev/null 2>&1; then \
		echo "Detected openSUSE"; \
		sudo zypper install gcc gtk3-devel pkg-config; \
	else \
		echo "Unknown package manager. Please install manually:"; \
		echo "  - C compiler (gcc)"; \
		echo "  - GTK3 development headers"; \
		echo "  - pkg-config"; \
		echo ""; \
		echo "Common package names:"; \
		echo "  Arch: base-devel gtk3"; \
		echo "  Ubuntu/Debian: build-essential libgtk-3-dev"; \
		echo "  Fedora: gcc gtk3-devel pkg-config"; \
		echo "  openSUSE: gcc gtk3-devel pkg-config"; \
	fi

# Create a simple icon if one doesn't exist
icon:
	@if [ ! -f $(BUILD_DIR)/soundboard.png ]; then \
		echo "Creating placeholder icon..."; \
		convert -size 256x256 xc:blue -pointsize 24 -fill white -gravity center -annotate +0+0 "SB" $(BUILD_DIR)/soundboard.png 2>/dev/null || \
		echo "Warning: ImageMagick not found. Please create $(BUILD_DIR)/soundboard.png manually"; \
	fi

# Test the compiled program
test: $(TARGET)
	./$(TARGET)

# Clean build files
clean:
	rm -f $(TARGET)
	rm -rf $(APPDIR)
	rm -f Soundboard-x86_64.AppImage

# Clean everything including downloaded tools
distclean: clean
	rm -f $(LINUXDEPLOY) $(APPIMAGETOOL)

# Show help
help:
	@echo "Soundboard Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Compile the program (default)"
	@echo "  appimage  - Create AppImage (includes compilation)"
	@echo "  deps      - Install build dependencies (Arch Linux)"
	@echo "  icon      - Create placeholder icon if missing"
	@echo "  test      - Compile and run the program"
	@echo "  clean     - Remove build files"
	@echo "  distclean - Remove all files including tools"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Quick start:"
	@echo "  make deps     # Install dependencies"
	@echo "  make icon     # Create icon (optional)"
	@echo "  make appimage # Build everything"

.PHONY: all appimage deps icon test clean distclean help
