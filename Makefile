# コンパイラ設定
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

# ディレクトリ定義
CORE_DIR = core_c
SERVER_DIR = server
CLIENT_DIR = client

# インクルードパス
INCLUDES = -I$(CORE_DIR)/include -I$(CORE_DIR)/src/include

# ライブラリパスとリンク設定
LIBS = -L$(CORE_DIR) -lcontrast_c

# 生成ターゲット
TARGET_SERVER = $(SERVER_DIR)/server
TARGET_CLIENT = $(CLIENT_DIR)/client

# サーバーのソースファイル群
SERVER_SRCS = $(SERVER_DIR)/main.c \
              $(SERVER_DIR)/network.c \
              $(SERVER_DIR)/room.c \
              $(SERVER_DIR)/command.c

.PHONY: all clean core_c_build

all: core_c_build $(TARGET_SERVER) $(TARGET_CLIENT)

# core_c ライブラリのビルド
core_c_build:
	$(MAKE) -C $(CORE_DIR)

# サーバーのビルド (分割ファイルをコンパイル)
$(TARGET_SERVER): $(SERVER_SRCS) $(SERVER_DIR)/server.h
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $@ $(INCLUDES) $(LIBS)

# クライアントのビルド
$(TARGET_CLIENT): $(CLIENT_DIR)/client.c
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDES) $(LIBS)

clean:
	$(MAKE) -C $(CORE_DIR) clean
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)