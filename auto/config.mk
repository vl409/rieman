CFLAGS=-Wall -Werror -g -ggdb -Ibuild/
LDFLAGS=

ASAN_CFLAGS=-fsanitize=address
ASAN_LDFLAGS=-fsanitize=address

# initial set of libraries, others are added by autotests
LIBS=-lm

.PHONY: rieman

rieman: build/rieman
