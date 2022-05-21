# Dimitrios Koropoulis 3967
# csd3967@csd.uoc.gr
# CS345 - Fall 2020
# sish
# Makefile

CC     = gcc
CFLAGS = -Wall
BIN    = sish

SRC_DIR = src
OBJ_DIR = obj

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES))

#$(info $$SRC_FILES: ${SRC_FILES})
#$(info $$OBJ_FILES: ${OBJ_FILES})

.PHONY: all clean

all:
	@$(MAKE) --no-print-directory clean
	@mkdir $(OBJ_DIR)
	@$(MAKE) --no-print-directory $(BIN)

$(BIN): $(OBJ_FILES)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR) $(BIN)

