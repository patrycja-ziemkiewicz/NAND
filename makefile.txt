CC=gcc
#flagi kompilacji
C_FLAGS=-Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
#flagi linkowania
L_FLAGS=-shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup

HEADERS=nand.h memory_tests.h
OBJECTS=$(SOURCES:.c=.o)
SOURCES=nand.c memory_tests.c
LIBRARY=libnand.so
 
.PHONY: all clean
 
all: $(LIBRARY)
 
$(LIBRARY): $(OBJECTS)
	$(CC) $(L_FLAGS) -o $@ $^
  
%.o: %.c $(HEADERS)
	$(CC) $(C_FLAGS) -c -o $@ $<
 
clean:
	rm -f $(OBJECTS) $(LIBRARY) 
