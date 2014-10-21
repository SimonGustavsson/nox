SOURCE=source
BUILD=build
INCLUDE=include

nox: directories $(BUILD)/nox-disk.img

$(BUILD)/nox-disk.img: $(BUILD)/mbr.bin

$(BUILD)/mbr.bin: $(SOURCE)/mbr.asm
	nasm $< -o $@ -f bin

.PHONY: clean directories
clean:
	@rm -r $(BUILD)

directories:
	@mkdir -p $(BUILD)
