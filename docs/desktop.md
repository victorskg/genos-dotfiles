# Desktop behavior

Expected behavior of the GenOS session. `$mod` is Super.

## Session

`greetd` owns TTY 1 and draws the GenOS login screen with ReGreet inside Hyprland, started through `start-hyprland`. The login Hyprland reads `/etc/greetd/hyprland.lua`. The keyboard is ABNT2. After a successful login, the only session is Hyprland through `start-hyprland`, which reads `~/.config/hypr/hyprland.lua`. That session launches Waybar, Mako, the polkit agent, and `workspace-follow.py`. Reloading the config restarts the workspace follower. If the login screen fails, another TTY is still available with Ctrl+Alt+F2.

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
| Print | Save a screenshot of every output to `~/Pictures` |
| Shift+Print | Save a selected region to `~/Pictures` |
| Super+Shift+Left | Move the window to `eDP-1` |
| Super+Shift+Right | Move the window to `HDMI-A-1` |

## Web apps

`webapp add <name> <url>` writes `~/.local/share/applications/<name>.desktop`. Fuzzel lists it. The launcher runs `chromium --ozone-platform-hint=wayland --app=<url>`, so the site opens without the tab strip. `StartupWMClass` is `chrome-<host>_<path>-Default`, which is the class Chromium gives that window.

The icon is a PNG in `~/.local/share/icons/hicolor/256x256/apps/`. The script requests the page's `apple-touch-icon`, then `<origin>/apple-touch-icon.png`, then `https://www.google.com/s2/favicons?domain=<url>&sz=256`.

`webapp remove <name>` deletes that launcher and its icon. It leaves any desktop file whose `Exec` line was not written by `webapp add`.

Sign-in for these windows is Chromium's default profile. Zen remains the handler for ordinary links.

## Workspaces and monitors

The laptop panel is the monitor whose name starts with `eDP`. Any other connected monitor is external. If several externals exist, the focused one is chosen, otherwise the first.

With an external monitor connected:

- Numbered workspaces stay on the external monitor.
- The laptop shows a companion workspace named `ext-N`, where `N` is the focused numbered workspace.
- Waybar lists numbered workspaces only. It hides names matching `ext-*`.
- Unplugging the external monitor moves windows off `ext-*` back onto their numbered workspaces and focuses the last main workspace.

Closing the last window on a numbered workspace closes the gap. Higher occupied workspaces shift down to fill it. If nothing is higher, focus moves to the nearest lower occupied workspace.

## Bar

Waybar sits at the top as three frosted groups: the clock, workspaces, and the status cluster. Its top margin matches `gap_out` (12). Each group has a gilt top edge. The active workspace is a pill in the theme accent.

The bar shows icons from Symbols Nerd Font. Names, percentages, and titles appear in the tooltip. The clock shows only the time, in EB Garamond. Its tooltip is Waybar's calendar. Scroll moves the month. Right click switches between month and year.

Each occupied workspace is a circle. The focused window's icon sits inside that workspace. Empty workspaces stay hidden. Names matching `ext-*` stay hidden.

The status cluster, from left to right, is media, CPU, memory, Bluetooth, network, volume, battery, and power.

- Media shows play or pause. The tooltip is the artist and the title. Left click toggles playback through `playerctl`. Right click skips to the next track. The module is absent when no player is running. Paused media uses the muted color.
- CPU and memory show an icon. The tooltip is the percentage. Warning starts at 70 and critical at 90.
- Wi-Fi shows signal strength. Ethernet and a disconnected radio use their own icons. The tooltip is the network name. Click opens the Wi-Fi popup.
- Bluetooth shows one icon. The tooltip lists the connected device. Off and disabled use the faint color. Click opens `bluetui` in Kitty.
- Volume shows a level icon, or a mute icon in steel. The tooltip is the percentage. Scroll changes the volume. Click opens the volume popup.
- Battery shows a level icon. The tooltip is the capacity and the remaining time. Charging uses gold. Warning starts at 30 and critical at 15.
- Power is a power icon in the accent color. It offers Lock (`hyprlock`), Restart, and Power off.

The Wi-Fi popup has no title. When a network is active, an arrow and its name sit on one row with refresh, disconnect, and the radio switch. Networks appear in a grid of three. Each tile shows the signal icon, then the name and, when the network is locked, a lock, both centered. The connected network's name is bold. A tile gains a border on hover. The connected tile uses the accent color. The tooltip shows the full network name and, on the next line, the signal percentage. Both lines are centered. The password field and the error text appear only when needed. Refresh, disconnect, and connect are icons. Their names are tooltips.

The volume popup has an output group and an input group. Each group shows a short label, the mute icon, the slider, and the percentage. Devices stay in a list. The active device name is bold and its row uses the accent color. A row gains a border on hover. The tooltip is the full device name, centered. Mute is an icon that follows the mute state.

The clock popup keeps the time, the date, and the calendar.

## Lock screen

`hyprlock` shows the time in EB Garamond over a darkened, blurred screenshot and asks for the password.

## Look

The theme is defined in `home/theme/.config/genos/palette.conf`. `variant=blood` uses crimson for focus. `variant=eclipse` uses violet. Run `apply-theme`, then `hyprctl reload`. `apply-theme` writes `colors.lua` for Hyprland and `colors.conf` for hyprlock. UI type is Inter, the lock clock is EB Garamond, and GTK prefers the dark theme. Kitty keeps its own colors, matched to the palette background, foreground, and gold. `apply-theme` only sets Kitty `background_opacity` from `opacity_terminal`.

The window border, the launcher border, and the power icon use the bright accent. Fills that carry foreground text keep the darker accent. Blur brightness is 1. `blur_vibrancy` keeps saturation in the glass.

Window corners are `radius_window` (6). Notifications are `radius_popup` (10). Bar groups are 10. The clock popup is 12, and the Wi-Fi and volume popups are 8. Workspace pills stay circles.

`~/.config/genos/opacity-mode` switches `opacity_mode` between `transparent` and `opaque`, regenerates the theme, and reloads Hyprland, Waybar, and Mako. Opaque mode turns blur off.
