PROGRAM = main

EXTRA_COMPONENTS = \
	extras/dht \
	extras/http-parser \
	extras/rboot-ota \
	extras/dhcpserver \
	extras/sntp \
	$(abspath esp-wolfssl) \
	$(abspath esp-cjson) \
	$(abspath esp-homekit) \
	$(abspath UDPlogger) \
	$(abspath esp-adv-button) \
	$(abspath HLW8012_BL0937_ESP)

#	$(abspath ../components/esp-homekit-common-functions/led_codes)\
#	$(abspath ../components/esp-homekit-common-functions/ota)\
#	$(abspath ../components/esp-homekit-common-functions/custom_characteristics)\
#	$(abspath ../components/esp-homekit-common-functions/shared_functions)\

FLASH_SIZE ?= 8
#FLASH_MODE ?= dout
#FLASH_SPEED ?= 40

HOMEKIT_MAX_CLIENTS = 16
HOMEKIT_SMALL = 0
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x8c000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_VERIFY
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_SETUP
EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL

ifdef VERSION
EXTRA_CFLAGS += -DVERSION=\"$(VERSION)\"
endif

LIBS ?= m

include $(SDK_PATH)/common.mk

sig: 
	openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin
	printf "%08x" `cat firmware/main.bin | wc -c`| xxd -r -p >>firmware/main.bin.sig
	ls -l firmware
 
monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
