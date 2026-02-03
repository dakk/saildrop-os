# Makefile to compile and upload saildrop Arduino project
# Supports multiple Waveshare ESP32-S3 display boards

# arduino-cli board listall | grep esp32
# arduino-cli board details -b esp32:esp32:esp32s3

BOARD_FQBN = esp32:esp32:esp32s3
PORT = /dev/ttyACM0
PROJECT_DIR = saildrop
JOBS ?= $(shell nproc)
BOARD_CONFIG = $(PROJECT_DIR)/board_config.h
ESPTOOL = ~/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool

# Board selection: lcd128 (default) or lcd4
# Usage: make compile BOARD=lcd4
BOARD ?= lcd4

# Board-specific configuration
ifeq ($(BOARD),lcd128)
    # Waveshare ESP32-S3 1.28" Round Touch LCD
    BOARD_NAME = ESP32-S3-Touch-LCD-1.28
    BOARD_DEFINE = BOARD_LCD_128
    LV_CONF_FILE = lv_conf_lcd128.h
    BOARD_OPTIONS = --board-options PSRAM=enabled,FlashSize=16M
    BUILD_PROPERTY = --build-property build.flash_size=16MB \
        --build-property build.psram=enabled
else ifeq ($(BOARD),lcd4)
    # Waveshare ESP32-S3 4" Touch LCD
    BOARD_NAME = ESP32-S3-Touch-LCD-4
    BOARD_DEFINE = BOARD_LCD_4
    LV_CONF_FILE = lv_conf_lcd4.h
    # LCD-4 uses ESP32-S3N16R8 with 8MB Octal PSRAM (OPI mode)
    # USBMode=hwcdc + CDCOnBoot=cdc enables serial output through USB port
    BOARD_OPTIONS = --board-options PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc
    BUILD_PROPERTY = --build-property build.flash_size=16MB \
        --build-property build.psram=opi
else
    $(error Unknown BOARD type: $(BOARD). Use lcd128 or lcd4)
endif

.PHONY: all compile upload monitor clean info install-prereq install-prereq-lcd4 set-board reset erase-nvs erase-flash

# Default target - compile for 1.28" LCD
all: compile upload monitor

# Show build configuration
info:
	@echo "Board: $(BOARD_NAME)"
	@echo "FQBN: $(BOARD_FQBN)"
	@echo "Port: $(PORT)"
	@echo "Define: $(BOARD_DEFINE)"

# Generate board_config.h for selected board
set-board:
	@echo "Configuring for $(BOARD_NAME)..."
	@echo "/*" > $(BOARD_CONFIG)
	@echo " * Board Configuration - AUTO-GENERATED" >> $(BOARD_CONFIG)
	@echo " * Target: $(BOARD_NAME)" >> $(BOARD_CONFIG)
	@echo " * DO NOT EDIT - Run 'make compile BOARD=lcd128' or 'make compile BOARD=lcd4'" >> $(BOARD_CONFIG)
	@echo " */" >> $(BOARD_CONFIG)
	@echo "#ifndef BOARD_CONFIG_H" >> $(BOARD_CONFIG)
	@echo "#define BOARD_CONFIG_H" >> $(BOARD_CONFIG)
	@echo "" >> $(BOARD_CONFIG)
	@echo "#define $(BOARD_DEFINE)" >> $(BOARD_CONFIG)
	@echo "" >> $(BOARD_CONFIG)
	@echo "#endif // BOARD_CONFIG_H" >> $(BOARD_CONFIG)

# Install prerequisites for 1.28" LCD
install-prereq:
	wget https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/ESP32-S3-Touch-LCD-1.28-Demo.zip
	unzip ESP32-S3-Touch-LCD-1.28-Demo.zip
	cp -r ESP32-S3-Touch-LCD-1.28-Demo/Arduino/libraries/TFT* ~/Arduino/libraries
	arduino-cli core install esp32:esp32@2.0.12

# Install prerequisites for 4" LCD
# The 4" LCD uses Arduino_GFX library for ST7701 RGB display
install-prereq-lcd4:
	arduino-cli core install esp32:esp32@3.3.5
	arduino-cli lib install 'GFX Library for Arduino@1.5.3'
	@echo "Installed ESP32 core 3.3.5 and GFX Library for Arduino 1.5.3"

# Compile for selected board
compile: set-board
	@echo "Compiling for $(BOARD_NAME)..."
	@echo "Using LVGL config: $(LV_CONF_FILE)"
	cp $(LV_CONF_FILE) ~/Arduino/libraries/lvgl/src/lv_conf.h
	arduino-cli compile -v $(BUILD_PROPERTY) $(BOARD_OPTIONS) \
		--jobs $(JOBS) --fqbn $(BOARD_FQBN) $(PROJECT_DIR)

# Compile for 1.28" LCD (explicit target)
compile-lcd128:
	$(MAKE) compile BOARD=lcd128

# Compile for 4" LCD (explicit target)
compile-lcd4:
	$(MAKE) compile BOARD=lcd4

# Upload to device
upload:
	arduino-cli upload -v $(BOARD_OPTIONS) --fqbn $(BOARD_FQBN) -p $(PORT) $(PROJECT_DIR)

# Serial monitor
monitor:
	arduino-cli monitor -p $(PORT) --config 115200

# Clean build artifacts
clean:
	rm -rf $(PROJECT_DIR)/build
	rm -rf ~/Arduino/libraries/lvgl/src/lv_conf.h

# Reset settings (erase NVS partition only)
# NVS partition is at 0x9000 with size 0x6000 (24KB)
reset: erase-nvs

erase-nvs:
	@echo "Erasing NVS partition (settings will be cleared)..."
	$(ESPTOOL) --port $(PORT) erase_region 0x9000 0x6000
	@echo "NVS erased. Device will boot in portal mode."

# Erase entire flash (factory reset)
erase-flash:
	@echo "Erasing entire flash..."
	$(ESPTOOL) --port $(PORT) erase_flash
	@echo "Flash erased. Re-upload firmware with 'make upload'"

# Full build cycle for 1.28" LCD
build-lcd128: compile-lcd128 upload monitor

# Full build cycle for 4" LCD
build-lcd4: compile-lcd4 upload monitor

# Help
help:
	@echo "Saildrop-OS Build System"
	@echo "========================"
	@echo ""
	@echo "Supported boards:"
	@echo "  lcd128  - Waveshare ESP32-S3 1.28\" Round Touch LCD (240x240)"
	@echo "  lcd4    - Waveshare ESP32-S3 4\" Touch LCD (480x480)"
	@echo ""
	@echo "Targets:"
	@echo "  make compile BOARD=lcd128  - Compile for 1.28\" LCD (default)"
	@echo "  make compile BOARD=lcd4    - Compile for 4\" LCD"
	@echo "  make compile-lcd128        - Compile for 1.28\" LCD"
	@echo "  make compile-lcd4          - Compile for 4\" LCD"
	@echo "  make upload                - Upload to device"
	@echo "  make monitor               - Open serial monitor"
	@echo "  make build-lcd128          - Full build for 1.28\" LCD"
	@echo "  make build-lcd4            - Full build for 4\" LCD"
	@echo "  make info                  - Show current build configuration"
	@echo "  make install-prereq        - Install dependencies for 1.28\" LCD"
	@echo "  make install-prereq-lcd4   - Install dependencies for 4\" LCD"
	@echo "  make clean                 - Clean build artifacts"
	@echo "  make reset                 - Reset settings (erase NVS, boot to portal)"
	@echo "  make erase-flash           - Erase entire flash (factory reset)"
	@echo ""
	@echo "Examples:"
	@echo "  make build-lcd128          # Build and upload for 1.28\" display"
	@echo "  make build-lcd4            # Build and upload for 4\" display"
	@echo "  make compile BOARD=lcd4    # Just compile for 4\" display"
