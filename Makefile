CFLAGS ?= -Wall -Wextra -O3
LDFLAGS ?= -s

BIN := simple-hexedit
OBJS = simple-hexedit.o


all: $(BIN)

clean:
	-rm -f $(BIN) $(OBJS)

distclean: clean

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

