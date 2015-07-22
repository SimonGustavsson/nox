MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

INCLUDE_DIR := $(MODULE)include
SOURCE_DIR := $(MODULE)source
OBJ_DIR := $(MODULE)obj
DEP_DIR := $(MODULE)deps

# Ensure module build directories are removed by clean target
CLEAN_DIRS += $(OBJ_DIR) $(DEP_DIR)

# Tell the main makefile to copy this to the harddisk image
FS_FILES += $(FS_DIR)/USERLAND.ELF

C_HEADERS := $(patsubst %,-I%, $(shell find $(INCLUDE_DIR) -type d))
-include $(addprefix $(DEP_DIR)/, $(notdir $(COBJECTS:.o=.d)))

userland: $(BUILD_DIR)/USERLAND.ELF

$(BUILD_DIR)/USERLAND.ELF: $(MODULE)userland.ld $(OBJ_DIR)/userland.o
	@echo "$(TIME) LD       $@"
	@$(TOOL)-ld -T $< $(filter-out $<,$^) -o $@

$(OBJ_DIR)/%.o : $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	@mkdir -p $(DEP_DIR)/$(dir $*)

	@echo "$(TIME) CC       $<"
	@$(TOOL)-gcc $< -o $@ $(C_HEADERS) $(CFLAGS) -MD -MF $(DEP_DIR)/$*.d

.PHONY: userland
