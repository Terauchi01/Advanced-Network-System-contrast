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

# ターゲット
TARGET_SERVER = $(SERVER_DIR)/server
TARGET_CLIENT = $(CLIENT_DIR)/client

.PHONY: all clean core_c_build

all: core_c_build $(TARGET_SERVER) $(TARGET_CLIENT)

# core_c ライブラリのビルド
core_c_build:
	$(MAKE) -C $(CORE_DIR)

# サーバーのビルド
$(TARGET_SERVER): $(SERVER_DIR)/server.c
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDES) $(LIBS)

# クライアントのビルド
$(TARGET_CLIENT): $(CLIENT_DIR)/client.c
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDES) $(LIBS)

clean:
	$(MAKE) -C $(CORE_DIR) clean
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)