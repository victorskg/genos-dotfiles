#!/usr/bin/env bash
set -euo pipefail

DOTFILES_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

packages=(
  git
  stow
  hyprland
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
)

echo "Installing GenOS base packages..."
sudo pacman -S --needed "${packages[@]}"

echo "Enabling networking..."
sudo systemctl enable NetworkManager

echo "Deploying GenOS configuration..."
cd "$DOTFILES_DIR"

for package in hypr waybar kitty; do
  stow --no-folding --target="$HOME" --dir=home "$package"
done

echo
echo "GenOS base setup complete."
echo "Reboot, log in on a TTY, and run: Hyprland"


