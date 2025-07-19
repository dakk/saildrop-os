# Makefile to compile and upload saildrop Arduino project

# arduino-cli board listall | grep esp32
# arduino-cli board details -b esp32:esp32:esp32s3

BOARD_FQBN = esp32:esp32:esp32s3
PORT = /dev/ttyACM0
PROJECT_DIR = saildrop
JOBS ?= $(shell nproc)

# ~/.arduino15/packages/esp32/hardware/esp32/2.0.12/platform.txt
# cat ~/.arduino15/packages/esp32/hardware/esp32/2.0.12/boards.txt | grep esp32s3

BOARD_OPTIONS = --board-options PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB,FlashSize=16M
BUILD_PROPERTY = --build-property build.flash_size=16MB \
	--build-property build.partitions=app3M_fat9M_16MB \
	--build-property build.psram=enabled

.PHONY: all compile upload monitor

all: compile upload monitor

install-prereq:
	cp -r Esp32-s3-touch-lcd-lib/* ~/Arduino/libraries
	cp lv_conf.h ~/Arduino/libraries/lvgl/
	cp lv_conf.h ~/Arduino/libraries/lvgl/src/
	arduino-cli core install esp32:esp32@2.0.12

compile:
	cp lv_conf.h ~/Arduino/libraries/lvgl/
	cp lv_conf.h ~/Arduino/libraries/lvgl/src/
	arduino-cli compile -v $(BUILD_PROPERTY) $(BOARD_OPTIONS) \
		--jobs $(JOBS) --fqbn $(BOARD_FQBN) $(PROJECT_DIR) 

upload:
	arduino-cli upload -v $(BOARD_OPTIONS) --fqbn $(BOARD_FQBN) -p $(PORT) $(PROJECT_DIR)

monitor:
	arduino-cli monitor -p $(PORT) --config 115200