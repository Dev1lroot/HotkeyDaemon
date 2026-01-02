CC = gcc
CFLAGS = -Wall -O2 $(shell pkg-config --cflags libevdev yaml-0.1)
LDFLAGS = $(shell pkg-config --libs libevdev yaml-0.1)
TARGET = hkd
SRC = hkd.c

PKG_NAME = hotkeydaemon
PKG_VERSION = 1.0.1
ARCH = amd64

PKG_SRC_DIR = target/debian-based
PKG_BUILD_DIR = build/$(PKG_NAME)_$(PKG_VERSION)

POOL_DIR = pool/main/h/$(PKG_NAME)
DIST_DIR = dists/stable/main/binary-$(ARCH)

DEB_FILE = $(PKG_NAME)_$(PKG_VERSION)_$(ARCH).deb

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

package: clean all
	@echo "Building deb package..."

	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	cp -r $(PKG_SRC_DIR)/* $(PKG_BUILD_DIR)/

	mkdir -p $(PKG_BUILD_DIR)/usr/local/bin
	install -m 755 $(TARGET) $(PKG_BUILD_DIR)/usr/local/bin/$(TARGET)

	# TODO: Copy config

	chmod -R 755 $(PKG_BUILD_DIR)/DEBIAN

	dpkg-deb --build $(PKG_BUILD_DIR)
	mv build/$(PKG_NAME)_$(PKG_VERSION).deb $(DEB_FILE)

	@echo "Placing package.deb into pool..."

	mkdir -p $(POOL_DIR)
	mv $(DEB_FILE) $(POOL_DIR)/

	@echo "Generating Packages index (optional)..."

	mkdir -p $(DIST_DIR)
	dpkg-scanpackages pool /dev/null > $(DIST_DIR)/Packages
	gzip -9kf $(DIST_DIR)/Packages

	@echo "Done!"
	@echo "APT repo is ready"

uninstall:
	@echo "Removing binary..."
	rm -f /usr/local/bin/$(TARGET)
	@echo "Binary removed. Configuration at $(CONFIG_DIR) was preserved."

.PHONY: all clean install uninstall