# Make sure we use the correct compilers
CLANG ?= clang
FUSE_LD ?= lld-link

default: all

.PHONY: all clean

########################################################################################################################
# All the source files
########################################################################################################################

# Sources for the core
SRCS += $(shell find src/ -name '*.c')
SRCS += $(shell find src/ -name '*.S')

# Runtime support
SRCS += $(shell find lib/runtime -name '*.c')

# A hand-picked group of edk2 sources
SRCS += $(shell cat edk2core.txt)

# Make sure we build the guids c file if it does not exists
SRCS += guids.c

# Get the objects and their dirs
OBJS := $(SRCS:%=./build/%.o)
DEPS := $(OBJS:%.o=%.d)

# The UEFI headers, as a dependency for the guid generator
UEFI_HDRS := $(shell find edk2/MdePkg/Include -name '*.h')

########################################################################################################################
# Include directories
########################################################################################################################

INCLUDE_DIRS += edk2/MdePkg/Include
INCLUDE_DIRS += edk2/MdePkg/Library/BaseLib
INCLUDE_DIRS += edk2/MdePkg/Include/X64
INCLUDE_DIRS += edk2/OvmfPkg/Include
INCLUDE_DIRS += edk2/UefiPayloadPkg/PayloadLoaderPeim
INCLUDE_DIRS += src/

########################################################################################################################
# EDK2 flags
########################################################################################################################

# These are mostly taken from the default configuration of MdePkg

EDK2_OPTS_BOOL := PcdVerifyNodeInList=FALSE
EDK2_OPTS_BOOL += PcdComponentNameDisable=FALSE
EDK2_OPTS_BOOL += PcdDriverDiagnostics2Disable=FALSE
EDK2_OPTS_BOOL += PcdComponentName2Disable=FALSE
EDK2_OPTS_BOOL += PcdDriverDiagnosticsDisable=FALSE
EDK2_OPTS_BOOL += PcdUgaConsumeSupport=TRUE

EDK2_OPTS_UINT32 := PcdMaximumLinkedListLength=1000000
EDK2_OPTS_UINT32 += PcdMaximumUnicodeStringLength=1000000
EDK2_OPTS_UINT32 += PcdMaximumAsciiStringLength=1000000
EDK2_OPTS_UINT32 += PcdSpinLockTimeout=10000000
EDK2_OPTS_UINT32 += PcdFixedDebugPrintErrorLevel=0xFFFFFFFF
EDK2_OPTS_UINT32 += PcdUefiLibMaxPrintBufferSize=320
EDK2_OPTS_UINT32 += PcdMaximumDevicePathNodeCount=0
EDK2_OPTS_UINT32 += PcdCpuLocalApicBaseAddress=0xfee00000
EDK2_OPTS_UINT32 += PcdCpuInitIpiDelayInMicroSeconds=10000
EDK2_OPTS_UINT32 += PcdDebugPrintErrorLevel=0x00400000

EDK2_OPTS_UINT16 := PcdUefiFileHandleLibPrintBufferSize=1536

EDK2_OPTS_UINT8 := PcdSpeculationBarrierType=0x1
# Enable all debug macros
EDK2_OPTS_UINT8 += PcdDebugPropertyMask=0x3f
EDK2_OPTS_UINT8 += PcdDebugClearMemoryValue=0xAF

EDK2_FLAGS := $(EDK2_OPTS_UINT32:%=-D_PCD_GET_MODE_32_%)
EDK2_FLAGS += $(EDK2_OPTS_UINT16:%=-D_PCD_GET_MODE_16_%)
EDK2_FLAGS += $(EDK2_OPTS_UINT8:%=-D_PCD_GET_MODE_8_%)
EDK2_FLAGS += $(EDK2_OPTS_BOOL:%=-D_PCD_GET_MODE_BOOL_%)

EDK2_FLAGS += -DgEfiCallerBaseName=\"UEFI\"

########################################################################################################################
# Flags
########################################################################################################################

CFLAGS := \
	-target x86_64-unknown-windows \
	-ffreestanding \
	-fshort-wchar \
	-nostdinc \
	-nostdlib \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wno-microsoft-static-assert \
	-Os \
	-flto \
	-fno-PIC \
	-fno-stack-protector

# Add all the includes
CFLAGS += $(INCLUDE_DIRS:%=-I%)

LDFLAGS := \
	-target x86_64-unknown-windows \
	-nostdlib \
	-Wl,-entry:EfiMain \
	-Wl,-subsystem:efi_application \
	-fuse-ld=$(FUSE_LD)

ASMFLAGS := \
	-target x86_64-unknown-windows \
	-c \
	-masm=intel \
	-fno-PIC

########################################################################################################################
# Cleaning
########################################################################################################################

clean:
	rm -rf ./build ./bin

clean-all: clean
	rm -rf ./image

########################################################################################################################
# Build process
########################################################################################################################

-include $(DEPS)

all: ./bin/BOOTX64.EFI

./bin/BOOTX64.EFI: $(OBJS)
	@echo LD $@
	@mkdir -p $(@D)
	@$(CLANG) $(LDFLAGS) -o $@ $(OBJS)

# We need to generate guids.c for edk2
./guids.c: $(UEFI_HDRS)
	@echo Generating EFI guids
	@mkdir -p $(@D)
	@python3 gen_guids.py

# Add pcd flags for edk2 sources
./build/edk2/%.c.o: edk2/%.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CLANG) $(CFLAGS) $(EDK2_FLAGS) -c -o $@ $<

./build/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CLANG) $(CFLAGS) -D__FILENAME__="\"$<\"" -D__MODULE__="\"$(notdir $(basename $<))\"" -MMD -c -o $@ $<

./build/%.S.o: %.S
	@echo ASM $@
	@mkdir -p $(@D)
	@$(CLANG) $(ASMFLAGS) -o $@ $<
