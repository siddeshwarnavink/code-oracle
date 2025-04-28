CC = cc
CFLAGS = -lcurl -Ilib -Wall -ggdb

BUILD_DIR = .build
SRC_DIR = src
LIB_DIR = lib

EXECUTABLES = copypasta trashman

TARGETS = $(addprefix $(BUILD_DIR)/, $(EXECUTABLES))
SOURCES = $(addsuffix .c, $(addprefix $(SRC_DIR)/, $(EXECUTABLES)))

all: $(TARGETS)

$(BUILD_DIR)/%: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) -o $@ $< $(CFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
