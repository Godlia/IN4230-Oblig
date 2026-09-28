CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
CPPFLAGS = -Isrc
BUILD_DIR = build
TARGET_DIR = target

TARGETS = $(TARGET_DIR)/mipd $(TARGET_DIR)/ping_client $(TARGET_DIR)/ping_server

all: $(TARGETS)

$(TARGET_DIR)/mipd: $(BUILD_DIR)/mipd.o $(BUILD_DIR)/mip_common.o
	mkdir -p $(TARGET_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/mipd.o $(BUILD_DIR)/mip_common.o

$(TARGET_DIR)/ping_client: $(BUILD_DIR)/ping_client.o $(BUILD_DIR)/mip_common.o
	mkdir -p $(TARGET_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/ping_client.o $(BUILD_DIR)/mip_common.o

$(TARGET_DIR)/ping_server: $(BUILD_DIR)/ping_server.o $(BUILD_DIR)/mip_common.o
	mkdir -p $(TARGET_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/ping_server.o $(BUILD_DIR)/mip_common.o

$(BUILD_DIR)/%.o: src/%.c src/*.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_DIR)

.PHONY: all clean
