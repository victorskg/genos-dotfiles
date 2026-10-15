# Desktop behavior

Expected behavior of the GenOS session. `$mod` is Super.

## Session

`start-hyprland` starts Hyprland. On start it launches Waybar, Mako, the polkit agent, and `workspace-follow.py`. Reloading the config restarts the workspace follower.

Layout is dwindle. Keyboard layout is Brazilian ABNT2. New windows open on an empty workspace (`open-alone`).

## Bindings

| Keys | Action |
| --- | --- |
| Super+T | Kitty |
| Super+Space | Fuzzel |
| Super+B | Zen Browser |
| Super+Q | Close the active window |
| Super+F | Fullscreen |
| Super+V | Toggle floating |
| Super+M | Exit Hyprland |
| Super+1 … Super+9, Super+0 | Workspace 1–10 |
| Super+Comma / Super+Period | Previous / next workspace |
| Super+Shift+1 … Super+Shift+0 | Move the window to that workspace |
| Super+Shift+Comma / Super+Shift+Period | Move the window to the previous / next workspace |
| Super+Shift+Left | Move the window to `eDP-1` |
| Super+Shift+Right | Move the window to `HDMI-A-1` |

## Workspaces and monitors

The laptop panel is the monitor whose name starts with `eDP`. Any other connected monitor is external. If several externals exist, the focused one is chosen, otherwise the first.

With an external monitor connected:

- Numbered workspaces stay on the external monitor.
- The laptop shows a companion workspace named `ext-N`, where `N` is the focused numbered workspace.
- Waybar lists numbered workspaces only. It hides names matching `ext-*`.
- Unplugging the external monitor moves windows off `ext-*` back onto their numbered workspaces and focuses the last main workspace.

Closing the last window on a numbered workspace closes the gap. Higher occupied workspaces shift down to fill it. If nothing is higher, focus moves to the nearest lower occupied workspace.

## Bar

Waybar sits on top. Left to right: clock, workspaces, Bluetooth, network, volume, battery, power.

- Clock, Wi-Fi, and volume open their own popups.
- Bluetooth opens `bluetui` in Kitty.
- Power offers Lock (`hyprlock`), Restart, and Power off.

## Lock screen

`hyprlock` shows the time in EB Garamond over a blurred screenshot and asks for the password.

## Look

The theme is Cinder, defined in `palette.conf`. UI type is Inter, the terminal is JetBrains Mono, and the lock clock is EB Garamond. GTK prefers the dark theme.
