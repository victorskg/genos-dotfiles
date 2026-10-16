#!/usr/bin/env bash
set -Eeuo pipefail

DOTFILES_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

packages=(
  git
  stow
  base-devel
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
  playerctl
  grim
  slurp
  sof-firmware
  upower
  bluez
  bluez-utils
  bluetui
  gcc
  pkgconf
  inter-font
  ttf-jetbrains-mono
  ttf-nerd-fonts-symbols
  zram-generator
)

hardware=(
  nvidia-open
  nvidia-utils
  intel-ucode
  intel-media-driver
  vulkan-intel
)

aur_packages=(
  zen-browser-bin
  cursor-bin
  claude-desktop
)

stow_packages=(
  hypr
  waybar
  kitty
  theme
  fuzzel
  mako
  gtk
  xdg
  cursor
  ai
)

install_yay() {
  local build_dir archive
  build_dir="$(mktemp -d)"
  git clone --depth 1 https://aur.archlinux.org/yay.git "$build_dir"
  makepkg --dir "$build_dir" --syncdeps --rmdeps --noconfirm
  archive="$(find "$build_dir" -maxdepth 1 -type f -name 'yay-*.pkg.tar.zst' ! -name 'yay-debug-*')"
  sudo pacman -U --needed --noconfirm "$archive"
  rm -rf "$build_dir"
}

echo "==> Installing GenOS packages"
sudo pacman -S --needed "${packages[@]}" "${hardware[@]}"

echo "==> Configuring zram"
sudo install -Dm644 "$DOTFILES_DIR/etc/systemd/zram-generator.conf" /etc/systemd/zram-generator.conf
if [[ -f /etc/kernel/cmdline ]] && ! grep -qF 'zswap.enabled=0' /etc/kernel/cmdline; then
  echo "==> Disabling zswap"
  sudo sed -i 's/$/ zswap.enabled=0/' /etc/kernel/cmdline
  sudo mkinitcpio -P
fi

echo "==> Setting console keymap"
sudo localectl set-keymap br-abnt2

echo "==> Enabling NetworkManager"
sudo systemctl enable NetworkManager

echo "==> Enabling Bluetooth"
sudo systemctl enable bluetooth.service

if ! command -v yay >/dev/null 2>&1; then
  echo "==> Building yay"
  install_yay
fi

echo "==> Installing AUR packages"
yay -S --needed --noconfirm --removemake --answerdiff None --answerclean None \
  "${aur_packages[@]}"

echo "==> Checking dotfile packages"
for package in "${stow_packages[@]}"; do
  if [[ ! -d "$DOTFILES_DIR/home/$package" ]]; then
    echo "ERROR: Missing Stow package: $package" >&2
    exit 1
  fi
done

echo "==> Applying the GenOS theme"
"$DOTFILES_DIR/home/theme/.config/genos/apply-theme"

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

for package in "${stow_packages[@]}"; do
  stow --no-folding --target="$HOME" --dir=home "$package"
done

fc-cache -f

echo
echo "GenOS base setup complete."
echo "Start the graphical session locally with: start-hyprland"
