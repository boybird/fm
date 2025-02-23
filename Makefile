
CC = clang
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -lsqlite3 -lcrypto
SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = fm
COMPILE_DB = compile_commands.json

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "{\"directory\": \"$(CURDIR)\", \"command\": \"$(CC) $(CFLAGS) -c $< -o $@\", \"file\": \"$<\"}" > $(BUILD_DIR)/$*.json

# Generate compile_commands.json
$(COMPILE_DB): $(OBJS)
	@echo "[" > $(COMPILE_DB)
	@ls $(BUILD_DIR)/*.json | xargs cat | sed '$$s/$$/,/' | head -n -1 >> $(COMPILE_DB)
	@echo "{}]" >> $(COMPILE_DB)
	@rm -f $(BUILD_DIR)/*.json

compile-db: $(COMPILE_DB)

clean:
	rm -rf $(BUILD_DIR) $(COMPILE_DB)

.PHONY: all clean compile-db
