# Makefile to compile and upload saildrop Arduino project

# arduino-cli board listall | grep esp32
# arduino-cli board details -b esp32:esp32:esp32s3

BOARD_FQBN = esp32:esp32:esp32s3
PORT = /dev/ttyACM0
PROJECT_DIR = saildrop
JOBS ?= $(shell nproc)

# ~/.arduino15/packages/esp32/hardware/esp32/2.0.12/platform.txt
# cat ~/.arduino15/packages/esp32/hardware/esp32/2.0.12/boards.txt | grep esp32s3

BOARD_OPTIONS = --board-options PSRAM=enabled,FlashSize=16M # ,PartitionScheme=app3M_fat9M_16MB
BUILD_PROPERTY = --build-property build.flash_size=16MB \
	--build-property build.psram=enabled
# 	--build-property build.partitions=app3M_fat9M_16MB

.PHONY: all compile upload monitor opencpn opencpn-install opencpn-clean

all: compile upload monitor

install-prereq:
	wget https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/ESP32-S3-Touch-LCD-1.28-Demo.zip
	unzip ESP32-S3-Touch-LCD-1.28-Demo.zip
	cp -r ESP32-S3-Touch-LCD-1.28-Demo/Arduino/libraries/TFT* ~/Arduino/libraries
	arduino-cli core install esp32:esp32@2.0.12

compile:
	cp lv_conf.h ~/Arduino/libraries/lvgl/src/
	arduino-cli compile -v $(BUILD_PROPERTY) $(BOARD_OPTIONS) \
		--jobs $(JOBS) --fqbn $(BOARD_FQBN) $(PROJECT_DIR) 

upload:
	arduino-cli upload -v $(BOARD_OPTIONS) --fqbn $(BOARD_FQBN) -p $(PORT) $(PROJECT_DIR)

monitor:
	arduino-cli monitor -p $(PORT) --config 115200

# OpenCPN plugin
OPENCPN_BUILD_DIR = opencpn_plugin/build

opencpn:
	@mkdir -p $(OPENCPN_BUILD_DIR)
	cd $(OPENCPN_BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release
	$(MAKE) -C $(OPENCPN_BUILD_DIR)

opencpn-install: opencpn
	$(MAKE) -C $(OPENCPN_BUILD_DIR) install

opencpn-clean:
	rm -rf $(OPENCPN_BUILD_DIR)