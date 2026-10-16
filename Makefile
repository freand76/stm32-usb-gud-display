#
# USB GUD display for the STM32746G-Discovery
#
#   make                  build for USB High Speed (CN12), the default
#   make USB=FS           build for USB Full Speed (CN13)
#   make flash [USB=FS]   build and program the board through the ST-LINK
#   make toolchain        download the Arm GNU Toolchain and OpenOCD into toolchain/
#   make clean
#

# USB core: HS (OTG HS with the ULPI PHY, CN12) or FS (OTG FS, CN13)
USB ?= HS
ifeq ($(filter $(USB),HS FS),)
$(error USB must be HS or FS)
endif

PROJECT     := GUD_Display
CUBE        := STM32CubeF7
APP         := Projects/STM32746G-Discovery/Applications/USB_Device/GUD_Display
BUILD_DIR   := build/$(USB)
TARGET      := $(BUILD_DIR)/$(PROJECT)_$(USB)

# Toolchain, the downloaded one when present, else from PATH
TOOLCHAIN_BIN := $(wildcard toolchain/current/bin)
CROSS       := $(if $(TOOLCHAIN_BIN),$(TOOLCHAIN_BIN)/,)arm-none-eabi-
CC          := $(CROSS)gcc
OBJCOPY     := $(CROSS)objcopy
SIZE        := $(CROSS)size

OPENOCD     := $(firstword $(wildcard toolchain/openocd/bin/openocd) $(shell command -v openocd 2>/dev/null))

#
# Sources
#
HAL_DIR     := $(CUBE)/Drivers/STM32F7xx_HAL_Driver
BSP_DIR     := $(CUBE)/Drivers/BSP/STM32746G-Discovery
CMSIS_DIR   := $(CUBE)/Drivers/CMSIS
USBD_DIR    := $(CUBE)/Middlewares/ST/STM32_USB_Device_Library
GUD_DIR     := Middlewares/ST/STM32_USB_Device_Library/Class/GUD

C_SOURCES := \
	$(wildcard $(APP)/Src/*.c) \
	$(GUD_DIR)/Src/usbd_gud.c \
	$(USBD_DIR)/Core/Src/usbd_core.c \
	$(USBD_DIR)/Core/Src/usbd_ctlreq.c \
	$(USBD_DIR)/Core/Src/usbd_ioreq.c \
	$(BSP_DIR)/stm32746g_discovery.c \
	$(BSP_DIR)/stm32746g_discovery_lcd.c \
	$(BSP_DIR)/stm32746g_discovery_sdram.c \
	$(addprefix $(HAL_DIR)/Src/, \
		stm32f7xx_hal.c \
		stm32f7xx_hal_cortex.c \
		stm32f7xx_hal_dma.c \
		stm32f7xx_hal_dma2d.c \
		stm32f7xx_hal_gpio.c \
		stm32f7xx_hal_i2c.c \
		stm32f7xx_hal_ltdc.c \
		stm32f7xx_hal_pcd.c \
		stm32f7xx_hal_pcd_ex.c \
		stm32f7xx_hal_pwr.c \
		stm32f7xx_hal_pwr_ex.c \
		stm32f7xx_hal_rcc.c \
		stm32f7xx_hal_rcc_ex.c \
		stm32f7xx_hal_sdram.c \
		stm32f7xx_hal_uart.c \
		stm32f7xx_ll_fmc.c \
		stm32f7xx_ll_usb.c)

ASM_SOURCES := $(CMSIS_DIR)/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f746xx.s

LDSCRIPT    := $(APP)/GCC/STM32F746NGHx_FLASH.ld

C_INCLUDES := \
	-I$(APP)/Inc \
	-I$(GUD_DIR)/Inc \
	-I$(USBD_DIR)/Core/Inc \
	-I$(BSP_DIR) \
	-I$(HAL_DIR)/Inc \
	-I$(CMSIS_DIR)/Device/ST/STM32F7xx/Include \
	-I$(CMSIS_DIR)/Include

C_DEFS := -DSTM32F746xx -DUSE_HAL_DRIVER -DUSE_STM32746G_DISCOVERY -DUSE_USB_$(USB)

#
# Flags
#
MCU      := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
OPT      ?= -O2 -g3
CFLAGS   := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -std=gnu11 -Wall \
            -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS  := $(MCU) -x assembler-with-cpp
LDFLAGS  := $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
            -Wl,--gc-sections -Wl,-Map=$(TARGET).map,--cref -Wl,--print-memory-usage \
            -Wl,--no-warn-rwx-segments

#
# Rules
#
OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))

.PHONY: all clean flash flash-msd toolchain submodules install-udev

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex

$(BUILD_DIR)/obj/%.o: %.c | submodules-check
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/obj/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(CC) -c $(ASFLAGS) $< -o $@

$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@echo "LD $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SIZE) $@

%.bin: %.elf
	@$(OBJCOPY) -O binary -S $< $@

%.hex: %.elf
	@$(OBJCOPY) -O ihex $< $@

# Program over SWD with OpenOCD, or by copying to the ST-LINK mass storage
# drive when OpenOCD is not available
flash: all
ifneq ($(OPENOCD),)
	$(OPENOCD) -f board/stm32f7discovery.cfg \
		-c "program $(TARGET).elf verify reset exit"
else
	@$(MAKE) --no-print-directory flash-msd USB=$(USB)
endif

MSD := $(firstword $(wildcard /run/media/$(USER)/DIS_F746NG /media/$(USER)/DIS_F746NG))
flash-msd: all
	@test -n "$(MSD)" || { echo "The ST-LINK drive DIS_F746NG is not mounted"; exit 1; }
	cp $(TARGET).bin $(MSD)/ && sync
	@echo "Copied to $(MSD), the ST-LINK programs the board"

toolchain:
	tools/fetch_toolchain.sh
	tools/fetch_openocd.sh

# Initializes only the STM32CubeF7 submodules the build uses
CUBE_SUBMODULES := \
	Drivers/STM32F7xx_HAL_Driver \
	Drivers/CMSIS/Device/ST/STM32F7xx \
	Drivers/BSP/STM32746G-Discovery \
	Drivers/BSP/Components/Common \
	Drivers/BSP/Components/rk043fn48h \
	Drivers/BSP/Components/ft5336 \
	Drivers/BSP/Components/wm8994 \
	Drivers/BSP/Components/n25q128a

submodules:
	git submodule update --init $(CUBE)
	cd $(CUBE) && git submodule update --init $(CUBE_SUBMODULES)

.PHONY: submodules-check
submodules-check:
	@test -f $(HAL_DIR)/Inc/stm32f7xx_hal.h || \
		{ echo "STM32CubeF7 is not checked out, run 'make submodules'"; exit 1; }

install-udev:
	sudo install -m 644 udev/49-stlink.rules /etc/udev/rules.d/
	sudo udevadm control --reload-rules
	sudo udevadm trigger

clean:
	rm -rf build

-include $(OBJECTS:.o=.d)
