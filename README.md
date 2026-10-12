# GenOS

Minimal personal Arch Linux dotfiles, designed as the foundation for a lightweight, developer-focused desktop.

## Current stack

- Arch Linux
- Hyprland (Wayland)
- Kitty terminal
- Fuzzel launcher
- Waybar
- Mako notifications
- Thunar file manager
- PipeWire audio
- GNU Stow for configuration management

## Goals

- Minimal, maintainable configuration
- Modular dotfiles
- Efficient developer workflow
- Gaming support added incrementally
- Idle memory target below 3 GiB, measured on real hardware

## Installation

On a fresh Arch Linux installation with internet access:

```bash
sudo pacman -S --needed git
git clone YOUR_REPOSITORY_URL ~/.dotfiles
cd ~/.dotfiles
chmod +x install.sh
./install.sh
```

Replace `YOUR_REPOSITORY_URL` with your Git repository URL.

After installation, log in on a local TTY and run:

```bash
start-hyprland
```

## Notes

- Review `install.sh` before running it.
- Back up existing configuration files before deploying dotfiles.
- The initial setup is intended for real hardware; virtual machine graphics compatibility may vary.
- The installer does not configure NVIDIA drivers or create an Arch ISO.
