# Install
BIN = ubnt_lcm_demo

# Flags
CFLAGS += -std=c99 -pedantic -Wunused -Os -DNDEBUG
#CFLAGS += -ggdb -O0
SRC = main.c
OBJ = $(SRC:.c=.o)

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -o bin/$(BIN) -lm
	$(STRIP) -s bin/$(BIN)
