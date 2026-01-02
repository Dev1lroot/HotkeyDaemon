CC = gcc
CFLAGS = -Wall -O2 $(shell pkg-config --cflags libevdev yaml-0.1)
LDFLAGS = $(shell pkg-config --libs libevdev yaml-0.1)
TARGET = hkd
SRC = hkd.c

REAL_USER = $(shell logname || echo $$USER)
CONFIG_DIR = /home/$(REAL_USER)/.config/hkd

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: all
	@echo "Installing binary to /usr/local/bin..."
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	
	@echo "Creating configuration directory at $(CONFIG_DIR)..."
	install -d -m 755 $(CONFIG_DIR)
	
	@if [ ! -f $(CONFIG_DIR)/commands.yaml ]; then \
		echo "Installing default commands.yaml..."; \
		install -m 644 commands.yaml $(CONFIG_DIR)/commands.yaml; \
		chown $(REAL_USER):$(REAL_USER) $(CONFIG_DIR)/commands.yaml; \
	else \
		echo "Configuration file already exists, skipping to avoid overwriting."; \
	fi

uninstall:
	@echo "Removing binary..."
	rm -f /usr/local/bin/$(TARGET)
	@echo "Binary removed. Configuration at $(CONFIG_DIR) was preserved."

.PHONY: all clean install uninstall