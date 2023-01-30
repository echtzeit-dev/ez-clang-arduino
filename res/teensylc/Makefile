# Relink the output firmware.elf during PlatformIO build process
#
# This Makefile is supposed to be invoked from relink.py, which takes care of
# registering the a build step with PlatformIO. Required input files are the
# object files of the firmware itself and Arduino archives suitable for the
# device. The relink.py script provides the following environment variables.
#
# Tools:
# HOST_CXX: C++ compiler for the host machine (clang++ 13 or g++ 9.4)
# CXX: 			C++ compiler for the target device (PlatformIO arm-none-eabi-g++ @1.50401.190816)
# NM:				Name mangling tool for target device binaries (llvm-nm 13)
# OBJCOPY: 	LLVM object tool (llvm-objcopy 13)
#
# Locations are absolute paths:
# DEVICE_LIB_DIR: Target device library folders
# RELINK_DIR: Local directory for temporary files (/path/to/.relink/due)
# BUILD_DIR: Local PlatformIO build directory (/path/to/.pio/build/due)
# TOOLS_DIR: Local  directory (/path/to/tools)
#
.DEFAULT_GOAL := $(RELINK_DIR)/firmware.elf

# libFrameworkArduino.a and objects from our own firmware code
arduino_archives := $(shell find $(BUILD_DIR) -name '*.a')
firmware_objects := $(shell find $(BUILD_DIR)/src -name '*.o')

# Debug output:
# $(info Firmware objects: $(firmware_objects))
# $(info Arduino archives: $(arduino_archives))
# $(info Device library paths: $(DEVICE_LIB_DIR))

# Linker flags (TODO: integrate with platformio)
input_gc := $(firmware_objects) $(arduino_archives)
input_no_gc := $(addprefix -L,$(DEVICE_LIB_DIR)) -larm_cortexM0l_math
common_flags := -specs=nano.specs -Wl,--warn-common -Wl,--warn-section-align -Wl,--unresolved-symbols=report-all
LDFLAGS := -T ez-clang.ld -mcpu=cortex-m0plus -mthumb $(common_flags)

# Linker debug output:
# LDFLAGS := -v $(LDFLAGS)

# Link regular firmware ELF (without input_ez)
$(RELINK_DIR)/firmware-template.elf: $(input_gc)
	$(CXX) $(LDFLAGS) -Wl,--start-group $(input_no_gc) $(input_gc) -Wl,--end-group -o $@
	$(NM) $@ > $(RELINK_DIR)/symbols-before-relink.log

# Extract symbol table sections
$(RELINK_DIR)/symtab.section $(RELINK_DIR)/strtab.section: $(RELINK_DIR)/firmware-template.elf
	$(OBJCOPY) --dump-section=.strtab=$(RELINK_DIR)/strtab.section \
	           --dump-section=.symtab=$(RELINK_DIR)/symtab.section $<

# We have our own tool for post-processing
$(RELINK_DIR)/ez-exports: $(TOOLS_DIR)/ez-exports.cc
	$(HOST_CXX) -std=c++17 -g -o $@ $<

# Post-process symbol table sections
whitelists := $(shell find whitelists -name '*.txt')
# $(info Whitelists for exported symbols: $(whitelists))
$(RELINK_DIR)/symtab.exports $(RELINK_DIR)/strtab.exports: $(RELINK_DIR)/ez-exports $(RELINK_DIR)/symtab.section $(RELINK_DIR)/strtab.section
	$(RELINK_DIR)/ez-exports --log $(RELINK_DIR)/symbols-exported.log \
	                       				 $(RELINK_DIR)/strtab.section \
	                       				 $(RELINK_DIR)/symtab.section $(whitelists)

# Store sections as "ez" data blobs in separate object files
$(RELINK_DIR)/ez-%.o: $(RELINK_DIR)/%.exports
	$(OBJCOPY) -I binary -O elf32-littlearm --rename-section=.data=.ez.$*,rom,load,alloc $< $@

# Relink firmware ELF with ez data section in flash
input_ez := $(RELINK_DIR)/ez-strtab.o $(RELINK_DIR)/ez-symtab.o
$(RELINK_DIR)/firmware.elf: $(input_gc) $(input_ez)
	$(CXX) $(LDFLAGS) -Wl,--start-group $(input_no_gc) $(input_gc) $(input_ez) -Wl,--end-group -o $@
	$(NM) $@ > $(RELINK_DIR)/symbols-after-relink.log

# Note: We want to allow exposing all/arbitrary stdlib symbols for use in the REPL.
# Revisit usage of --gc-sections once we can load the stdlib at runtime.
# -Wl,--gc-sections
# -Wl,--check-sections
# -Wl,--print-gc-sections
