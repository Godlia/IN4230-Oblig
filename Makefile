CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
CPPFLAGS = -Isrc

TARGETS = mipd ping_client ping_server

all: $(TARGETS)

mipd: src/mipd.o src/mip_common.o
	$(CC) $(CFLAGS) -o $@ src/mipd.o src/mip_common.o

ping_client: src/ping_client.o src/mip_common.o
	$(CC) $(CFLAGS) -o $@ src/ping_client.o src/mip_common.o

ping_server: src/ping_server.o src/mip_common.o
	$(CC) $(CFLAGS) -o $@ src/ping_server.o src/mip_common.o

src/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) src/*.o

.PHONY: all clean
