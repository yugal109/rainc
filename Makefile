CC = clang
CFLAGS = -Wall -Wextra -pedantic -g -fsanitize=address -Isrc -Isrc/modules -I/opt/local/include
LDFLAGS = -fsanitize=address -L/opt/local/lib -lcurl -lsqlite3 -framework Accelerate


SRC_DIR = src
BUILD_DIR = build
TARGET = rain

SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/modules/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/modules $(TARGET)
