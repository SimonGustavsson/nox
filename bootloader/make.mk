AS := nasm

# Get the name of the module directory we're in
MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

IMAGE_ASSETS += $(BUILD)/mbr.bin $(BUILD)/vbr.bin $(BUILD)/BOOT.SYS

ASM_INCLUDE := $(MODULE)include/
SOURCE := $(MODULE)source

$(BUILD)/mbr.bin: $(SOURCE)/mbr.asm
	@echo "AS $<"
	
	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)

$(BUILD)/vbr.bin: $(SOURCE)/vbr.asm
	@echo "AS $<"

	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)

$(BUILD)/vbr_OLD.bin: $(SOURCE)/vbr_OLD.asm
	@echo "AS $<"
	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)

$(BUILD)/BOOT.SYS: $(SOURCE)/kloader.asm
	@echo "AS $<"

	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)
