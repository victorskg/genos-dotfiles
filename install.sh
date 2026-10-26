#!/usr/bin/env bash
set -Eeuo pipefail

DOTFILES_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

packages=(
  git
  curl
  stow
  base-devel
  hyprland
  hyprlock
  greetd
  greetd-regreet
  waybar
  fuzzel
  kitty
  chromium
  mako
  wl-clip-persist
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
  swaybg
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

gaming=(
  steam
  ttf-liberation
  nvidia-prime
  lib32-nvidia-utils
  lib32-mesa
  lib32-vulkan-intel
  gamemode
  lib32-gamemode
  mangohud
  lib32-mangohud
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

enable_multilib() {
  if grep -q '^\[multilib\]' /etc/pacman.conf; then
    return 0
  fi
  echo "==> Enabling [multilib]"
  sudo sed -i '/^#\[multilib\]/{
    s/^#//
    n
    s/^#Include/Include/
  }' /etc/pacman.conf
}

echo "==> Installing GenOS packages"
enable_multilib
sudo pacman -Syu --needed "${packages[@]}" "${hardware[@]}" "${gaming[@]}"

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

echo "==> Enabling NVIDIA Dynamic Boost"
sudo systemctl enable nvidia-powerd.service

if getent group gamemode >/dev/null; then
  echo "==> Adding $USER to gamemode"
  sudo usermod -aG gamemode "$USER"
fi

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

echo "==> Installing webapp"
install -Dm755 "$DOTFILES_DIR/bin/webapp" "$HOME/.local/bin/webapp"
sudo install -Dm755 "$DOTFILES_DIR/bin/game-run" /usr/local/bin/game-run
path_line='export PATH="$HOME/.local/bin:$PATH"'
if [[ ! -f $HOME/.bashrc ]]; then
  printf '%s\n' "$path_line" >"$HOME/.bashrc"
elif ! grep -qF '.local/bin' "$HOME/.bashrc"; then
  tmp=$(mktemp)
  printf '%s\n\n' "$path_line" | cat - "$HOME/.bashrc" >"$tmp"
  mv "$tmp" "$HOME/.bashrc"
fi

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

ln -sfn "$HOME/.config/hypr/wallpaper" "$HOME/.local/bin/wallpaper"

fc-cache -f

echo "==> Installing the login screen"
sudo install -d -m 755 /etc/greetd
sudo install -m 644 "$DOTFILES_DIR/etc/greetd/config.toml" /etc/greetd/config.toml
sudo install -m 644 "$DOTFILES_DIR/etc/greetd/hyprland.lua" /etc/greetd/hyprland.lua
sudo rm -f /etc/greetd/hyprland.conf
sudo install -m 644 "$DOTFILES_DIR/etc/greetd/regreet.toml" /etc/greetd/regreet.toml
sudo install -m 644 "$DOTFILES_DIR/etc/greetd/regreet.css" /etc/greetd/regreet.css
sudo systemctl disable getty@tty1.service
sudo systemctl enable greetd.service

echo
echo "GenOS base setup complete."
echo "The next boot opens the GenOS login screen and starts Hyprland."
