CC = cc
CFLAGS = -I$(LIB_DIR) -Wall -ggdb $(shell curl-config --cflags) -fPIC -Wno-unused-function
LDFLAGS = $(shell curl-config --libs) -shared -L./lib/ -l:libtree-sitter.a -l:libtree-sitter-javascript.a

BUILD_DIR = .build
SRC_DIR = src
LIB_DIR = lib

all: $(BUILD_DIR)/libcopypasta.so $(BUILD_DIR)/libtrashman.so $(BUILD_DIR)/libjiraiya.so

$(BUILD_DIR)/libcopypasta.so:
	mkdir -p $(BUILD_DIR)
	$(CC) src/copypasta.c -o $(BUILD_DIR)/libcopypasta.so $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/libtrashman.so:
	mkdir -p $(BUILD_DIR)
	$(CC) src/trashman.c -o $(BUILD_DIR)/libtrashman.so $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/libjiraiya.so:
	mkdir -p $(BUILD_DIR)
	$(CC) src/jiraiya.c -o $(BUILD_DIR)/libjiraiya.so $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
