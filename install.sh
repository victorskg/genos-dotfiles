#!/usr/bin/env bash
set -Eeuo pipefail

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

echo "==> Installing GenOS packages"
sudo pacman -S --needed "${packages[@]}"

echo "==> Enabling NetworkManager"
sudo systemctl enable NetworkManager

echo "==> Checking dotfile packages"
for package in hypr waybar kitty; do
  if [[ ! -d "$DOTFILES_DIR/home/$package" ]]; then
    echo "ERROR: Missing Stow package: $package" >&2
    exit 1
  fi
done

echo "==> Deploying configurations"
cd "$DOTFILES_DIR"

for package in hypr waybar kitty; do
  stow --no-folding --target="$HOME" --dir=home "$package"
done

echo
echo "GenOS base setup complete."
echo "Start the graphical session locally with: start-hyprland"


