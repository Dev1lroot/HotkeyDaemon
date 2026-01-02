HKD - Hotkey Daemon (Linux)
===================================================

HKD is a simple, kernel-level hotkey daemon designed to intercept hardware 
scan-codes directly from /dev/input/ devices. It executes user-defined 
commands as a non-privileged user while maintaining a persistent background 
presence.


INTRODUCTION
------------

Most modern desktop environments handle keyboard shortcuts via high-level 
APIs (X11/Wayland). HKD operates at the evdev layer, which provides 
several advantages:

* Reliability: Keystrokes are captured before they reach the window manager.
* Hardware Support: Native support for high-index function keys (F13-F24) 
    and non-standard scan-codes (e.g., the Copilot key on MSI Katana laptops).
* Process Isolation: Launched applications are detached from the daemon's 
    process tree using a double-fork technique, ensuring they survive daemon 
    restarts.

DEPENDENCIES
------------

The following libraries and tools are required for compilation:

* libevdev
* libyaml
* pkg-config
* build-essential (gcc, make)

On Debian-based distributions:

```bash
sudo apt update
sudo apt install libevdev-dev libyaml-dev pkg-config build-essential
```

COMPILATION AND INSTALLATION
----------------------------

To build the binary, run:

```bash
make
```

To install the binary to /usr/local/bin:

```bash
sudo make install
```

CONFIGURATION
-------------

HKD reads macros from a 'commands.yaml' file located in the working directory. 
Macros are defined using raw hardware scan-codes.

Example configuration:

```yaml
macros:
  # Example with the Copilot "key" (crutch)
  # Meta (125) + Shift (42) + F23 (193)
  - keys: [125, 42, 193]
    command: "firefox https://gemini.google.com/app && sleep 0.2 && wmctrl -xa firefox"

  # Meta (125) + S (31)
  - keys: [125, 31]
    command: "spectacle"
```

USAGE
-----

The daemon requires read access to /dev/input/ devices, therefore it must 
be started with root privileges.

1. Find your device scan-codes:
```bash
sudo ./hkd --debug
```

2. Run the daemon:
```bash
sudo ./hkd /dev/input/eventX
```

SYSTEMD INTEGRATION
-------------------

To run HKD as a system service, create /etc/systemd/system/hkd.service:

```ini
[Unit]
Description=Hotkey Daemon
After=graphical.target

[Service]
ExecStart=/usr/local/bin/hkd
WorkingDirectory=/home/dev1lroot/.config/hkd
Restart=always
User=root

[Install]
WantedBy=multi-user.target
```
And now run it:

```bash
sudo systemctl enable hkd
sudo systemctl start hkd
```

LICENSE
-------

This software is released under the MIT License.