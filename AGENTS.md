# GenOS

These dotfiles rebuild this Arch Linux machine. One commit and `./install.sh` on another Arch system should produce the same development and gaming desktop.

## Philosophy

Every change is judged by minimalism, elegance, beauty, and performance, in that order of restraint. Add a package, daemon, binding, or visual only when it earns its place. Prefer a small native config over a new dependency. Keep the idle desktop light.

## How to change this repo

- Stow packages live under `home/`. `install.sh` is the only installer.
- Color, type, gaps, and radii come from `home/theme/.config/genos/palette.conf`. Run `apply-theme`. Do not hand-edit generated color files.
- Desktop behavior is specified in `docs/desktop.md`. If a change alters Hyprland, bindings, workspaces, monitors, the bar, or the lock screen, update that file in the same change.
- Gaming support grows inside this same base. Do not fork a second environment for it.
