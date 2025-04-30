CC = cc
CFLAGS = -I$(LIB_DIR) -Wall -ggdb $(shell curl-config --cflags) -fPIC
LDFLAGS = $(shell curl-config --libs) -shared

BUILD_DIR = .build
SRC_DIR = src
LIB_DIR = lib

all: $(BUILD_DIR)/libcopypasta.so

$(BUILD_DIR)/libcopypasta.so:
	mkdir -p $(BUILD_DIR)
	$(CC) src/copypasta.c -o $(BUILD_DIR)/libcopypasta.so $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
