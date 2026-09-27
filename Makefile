CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c17
LDLIBS ?= -lreadline

TARGET := src/shell
SOURCES := src/main.c src/hash_table.c src/job_list.c src/command_history.c
DEMO_TARGET := src/pipeline_demo
DEMO_SOURCE := src/pipeline_demo.c

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

$(DEMO_TARGET): $(DEMO_SOURCE)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET) $(DEMO_TARGET)
	@history_file=$$(mktemp); \
	printf 'echo smoke-test\npwd\ntype echo\nexit\n' | HISTFILE="$$history_file" $(TARGET) | grep -q 'smoke-test'; \
	rm -f "$$history_file"

clean:
	rm -f $(TARGET) $(DEMO_TARGET)
