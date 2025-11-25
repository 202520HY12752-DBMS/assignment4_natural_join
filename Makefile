CC = gcc
CFLAGS = -Wall

# Directories
SRCDIR = src
DBBPT_SRCDIR = $(SRCDIR)/dbbpt
INMEMBPT_SRCDIR = $(SRCDIR)/inmembpt
BINDIR = bin
TESTOUT_DIR = test_out

# Source files
INMEMBPT_SRC = $(INMEMBPT_SRCDIR)/inmembpt.c
DBBPT_MAIN_SRC = $(DBBPT_SRCDIR)/main.c
DBBPT_BPT_SRC = $(DBBPT_SRCDIR)/dbbpt.c
FILE_MANAGER_SRC = $(DBBPT_SRCDIR)/file_manager.c

# Object files to be provided
PROVIDED_OBJS = $(GIFTDIR)/dbbpt.o

# Targets
INMEMBPT_TARGET = $(BINDIR)/inmembpt
DBBPT_TARGET = $(BINDIR)/dbbpt

.PHONY: all clean inmembpt dbbpt reset_test_env

all: inmembpt dbbpt

inmembpt: $(INMEMBPT_TARGET)
$(INMEMBPT_TARGET): $(INMEMBPT_SRC)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $<

dbbpt: $(DBBPT_TARGET)
$(DBBPT_TARGET): $(DBBPT_MAIN_SRC) $(DBBPT_BPT_SRC) $(FILE_MANAGER_SRC)
	@mkdir -p $(BINDIR)
	@echo "Build dbbpt..."
	$(CC) $(CFLAGS) -o $@ $^

clean:
	@echo "Cleaning up..."
	rm -rf $(BINDIR)

clean_all:
	@echo "Cleaning up all generated files..."
	rm -rf $(BINDIR)
	rm -rf $(GIFTDIR)

reset_test_env:
	@echo "Setting up test environment..."
	rm -rf $(TESTOUT_DIR)
	mkdir -p $(TESTOUT_DIR)