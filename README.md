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
- Zen Browser
- Chromium, for web apps (`webapp add`, `webapp remove`)
- Cursor
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

After installation, reboot. The GenOS login screen starts Hyprland.

## Theme

Colors, gaps, radii, and opacity live in `home/theme/.config/genos/palette.conf`.

```bash
~/.config/genos/apply-theme
hyprctl reload
```

Set `variant` to `blood` or `eclipse`. Set `opacity_mode` to `transparent` or `opaque`, or run `~/.config/genos/opacity-mode`. A preview of both variants is `home/theme/.config/genos/preview.html`.

Kitty colors and the shell prompt are not generated. Only `background_opacity` is.

Rollback to the pre-theme snapshot:

```bash
git checkout pre-ricing -- . && git switch main && ~/.config/genos/apply-theme && hyprctl reload
```

Wallpaper is the abyss color until a still image is chosen. Usable sources are a public-domain engraving, an original steel-and-ash texture, or a file placed in the theme directory. `swaybg` can show that file after it is approved. No packaged cursor or icon theme is installed. The candidates from the official repositories are `capitaine-cursors` and `papirus-icon-theme`.

These official packages are not installed. Each one waits for its own approval:

- `imv` for images
- `mpv` for video
- `zathura` for PDFs
- `wl-clipboard` for the clipboard
- `hyprpicker` for a color picker
- `brightnessctl` for the laptop backlight

## Notes

- Review `install.sh` before running it.
- Back up existing configuration files before deploying dotfiles.
- The initial setup is intended for real hardware; virtual machine graphics compatibility may vary.
- The installer installs the NVIDIA open driver, Intel media and Vulkan drivers, zram, and the ABNT2 console keymap for this machine. It does not create an Arch ISO.
