#!/usr/bin/env bash
set -Eeuo pipefail

DOTFILES_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

packages=(
  git
  stow
  hyprland
  hyprlock
  waybar
  fuzzel
  kitty
  mako
  thunar
  xdg-desktop-portal
  xdg-desktop-portal-hyprland
  polkit-gnome
  networkmanager
  pipewire
  pipewire-pulse
  wireplumber
  sof-firmware
  upower
  bluez
  bluez-utils
  bluetui
  gcc
  pkgconf
)

echo "==> Installing GenOS packages"
sudo pacman -S --needed "${packages[@]}"

echo "==> Enabling NetworkManager"
sudo systemctl enable NetworkManager

echo "==> Enabling Bluetooth"
sudo systemctl enable bluetooth.service

echo "==> Checking dotfile packages"
for package in hypr waybar kitty; do
  if [[ ! -d "$DOTFILES_DIR/home/$package" ]]; then
    echo "ERROR: Missing Stow package: $package" >&2
    exit 1
  fi
done

echo "==> Building waybar popups"
mkdir -p "$HOME/.local/bin"
gtk_libs="$(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)"
gcc -O2 -o "$HOME/.local/bin/waybar-clock" \
  "$DOTFILES_DIR/home/waybar/.config/waybar/clock.c" \
  $gtk_libs
gcc -O2 -o "$HOME/.local/bin/waybar-wifi" \
  "$DOTFILES_DIR/home/waybar/.config/waybar/wifi.c" \
  $gtk_libs
gcc -O2 -o "$HOME/.local/bin/waybar-audio" \
  "$DOTFILES_DIR/home/waybar/.config/waybar/audio.c" \
  $gtk_libs

echo "==> Deploying configurations"
cd "$DOTFILES_DIR"

for package in hypr waybar kitty; do
  stow --no-folding --target="$HOME" --dir=home "$package"
done

echo
echo "GenOS base setup complete."
echo "Start the graphical session locally with: start-hyprland"


