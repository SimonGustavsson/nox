MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

INCLUDE_DIR := $(MODULE)include
SOURCE_DIR := $(MODULE)source
OBJ_DIR := $(MODULE)obj
DEP_DIR := $(MODULE)deps

CLEAN_DIRS += $(OBJ_DIR) $(DEP_DIR)

IMAGE_ASSETS += $(BUILD)/USERLAND.ELF

C_HEADERS := $(patsubst %,-I%, $(shell find $(INCLUDE_DIR) -type d))
-include $(addprefix $(DEP_DIR)/, $(notdir $(COBJECTS:.o=.d)))

$(BUILD)/USERLAND.ELF: user_directories $(OBJ_DIR)/userland.o
	@echo "LD $<"

	@$(TOOL)-ld -T kernel.ld $(filter-out user_directories, $^) -o $@

$(OBJ_DIR)/%.o : $(SOURCE_DIR)/%.c

	mkdir -p $(dir $@)
	mkdir -p $(DEP_DIR)/$(dir $*)
	@echo "CC $<"

	@$(TOOL)-gcc $< -o $@ $(C_HEADERS) $(CFLAGS) -MD -MF $(DEP_DIR)/$*.d

user_directories:
	@mkdir -p $(DEP_DIR)
	@mkdir -p $(OBJ_DIR)

.PHONY: user_directories
