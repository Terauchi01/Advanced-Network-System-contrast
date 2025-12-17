# core_c ビルド設定

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -I./include

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

LIBRARY = libcontrast_c.a

all: $(BUILD_DIR) $(LIBRARY)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(LIBRARY)

.PHONY: all clean
