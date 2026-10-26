# Desktop behavior

Expected behavior of the GenOS session. `$mod` is Super.

## Session

`greetd` owns TTY 1 and draws the GenOS login screen with ReGreet inside Hyprland, started through `start-hyprland`. The login Hyprland reads `/etc/greetd/hyprland.lua`. The keyboard is ABNT2. After a successful login, the only session is Hyprland through `start-hyprland`, which reads `~/.config/hypr/hyprland.lua`. That session launches Waybar, Mako, the polkit agent, `workspace-follow.py`, and `wallpaper show`. Reloading the config restarts the workspace follower. If the login screen fails, another TTY is still available with Ctrl+Alt+F2.

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
| XF86AudioRaiseVolume / XF86AudioLowerVolume | Volume ±5% and show the OSD |
| XF86AudioMute | Toggle mute and show the OSD |
| XF86MonBrightnessUp / XF86MonBrightnessDown | Brightness ±5% (never below 1%) and show the OSD |
| Super+W | Next wallpaper |
| Super+Shift+W | Previous wallpaper |
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

When the follower starts with no windows open, it focuses workspace 1 on the external monitor (or the only monitor).

Closing the last window on a numbered workspace closes the gap. Higher occupied workspaces shift down to fill it. If nothing is higher, focus moves to the nearest lower occupied workspace.

## Bar

Waybar is a vertical island on the left edge, 52 px wide and fully rounded, floating `bar_margin` (12) from the screen. It has three sections: workspaces at the top, the clock (hours over minutes) in the middle, and the status cluster at the bottom. Windows tile in the remaining area. The active workspace is a circle in the theme accent with a soft glow.

The bar shows icons from Symbols Nerd Font. Names, percentages, and titles appear in the tooltip. The clock shows only the time, in EB Garamond. Its tooltip is Waybar's calendar. Scroll moves the month. Right click switches between month and year.

Each occupied workspace is a circle. The focused window's icon sits inside that workspace. Empty workspaces stay hidden. Names matching `ext-*` stay hidden.

The status cluster, from top to bottom, is media, CPU, memory, Bluetooth, network, volume, battery, and power.

- Media shows play or pause. The tooltip is the artist and the title. Left click toggles playback through `playerctl`. Right click skips to the next track. The module is absent when no player is running. Paused media uses the muted color.
- CPU and memory show an icon. The tooltip is the percentage. Warning starts at 70 and critical at 90.
- Wi-Fi shows signal strength. Ethernet and a disconnected radio use their own icons. The tooltip is the network name. Click opens the Wi-Fi popup.
- Bluetooth shows one icon. The tooltip lists the connected device. Off and disabled use the faint color. Click opens `bluetui` in Kitty.
- Volume shows a level icon, or a mute icon in steel. The tooltip is the percentage. Scroll changes the volume. Click opens the volume popup.
- Battery shows a level icon. The tooltip is the capacity and the remaining time. Charging uses green. Warning starts at 30 and critical at 15.
- Power is a power icon in the accent color. It offers Lock (`hyprlock`), Restart, and Power off.

The Wi-Fi popup is 340 px wide. The title "Wi-Fi" sits beside the radio switch, with a caption below it (`N networks available`, or `Off`, `Scanning...`, `Connecting to <name>...`). Networks are rows, sorted by signal. Each row shows the signal icon, a lock when the network is secured, the name, and a circular action. The connected row has an accent border, a bright name, and a filled action that disconnects when clicked. Any other row connects when clicked. Secured networks without a saved profile show the password field and the connect icon below the list. The error text appears only when needed. A full-width Rescan pill ends the panel. Rows fade in one after another. The panel namespace is `genos-popup`, which Hyprland blurs.

The volume popup has an output group and an input group. Each group shows a short label, the mute icon, the slider, and the percentage. Devices stay in a list. The active device name is bold and its row uses the accent color. A row gains a border on hover. The tooltip is the full device name, centered. Mute is an icon that follows the mute state.

The clock popup keeps the time, the date, and the calendar.

## Popups

The clock, Wi-Fi and volume popups open beside the bar, 80 px from the left edge, on the monitor under the cursor. A popup opened from the upper half of the screen grows downward from the cursor. One opened from the lower half grows upward. Each slides 12 px out of the bar while fading in over 180 ms (`popup.h`). Panels have 16 px corners and a soft shadow.

## On-screen display

`~/.config/hypr/osd` changes volume or brightness and sends a Mako notification with the app name `genos-osd` and a `value` hint. Mako draws it at the bottom center, 260 px wide, as an accent title over a progress fill in the accent color. Each change replaces the previous slip and closes after 1.2 seconds. Brightness is written through logind (`busctl ... SetBrightness`), so no extra package or udev rule is needed.

## Notifications

Mako draws a slip at the top right, 200 px wide and at most 36 px tall. The title is the bright accent, the body is faint, and both sit on one line. The frame is a 1 px line in the surface line color. Every corner uses `radius_popup`. The icon is 12 px. The slip closes after 5 seconds. A critical notification takes the bright border and stays until it is closed.

Left click runs the default action when the sending program defined one, then closes the card. A notification with no action only closes. Right click closes it.

## Lock screen

`hyprlock` shows the time in EB Garamond over a darkened, blurred screenshot and asks for the password.

## Look

Still wallpapers live in `~/.local/share/genos/wallpapers`. `swaybg` draws the file linked from `~/.local/state/genos/wallpaper`, scaled and cropped to fill each output. Super+W cycles forward. Super+Shift+W cycles backward. With no image, the desktop stays the abyss color. The login screen does not draw a wallpaper. See `docs/wallpaper.md`.

The theme uses one accent hue per variant. Surfaces, overlays, and lines are neutrals tinted by that hue (`<variant>_bg`, `_surface`, `_overlay`, `_line`). Gold appears only for warnings. The theme is defined in `home/theme/.config/genos/palette.conf`. `variant=blood` uses crimson for focus. `variant=eclipse` uses violet. `variant=verdigris` uses oxidized teal. `home/theme/.config/genos/variants.conf` maps each wallpaper file name to a variant. `wallpaper` runs `set-variant` on every change and at login, which rewrites `variant` in `palette.conf`, regenerates the theme, and reloads Hyprland, Waybar, and Mako. Run `set-variant <name>` to force a variant until the next wallpaper change. A wallpaper missing from `variants.conf` keeps the current variant. Run `apply-theme`, then `hyprctl reload`. `apply-theme` writes `colors.lua` for Hyprland and `colors.conf` for hyprlock. UI type is Inter, the lock clock is EB Garamond, and GTK prefers the dark theme. `apply-theme` writes Kitty `colors.conf` (background, foreground, cursor, selection) and `opacity.conf`.

Kitty pastes the clipboard with Ctrl+Shift+V and Shift+Insert (remapped from the default primary selection). Ctrl+V is not bound, so terminal programs receive it. Kitty sets `copy_on_select clipboard`. Selecting text with the mouse writes to both the primary selection and the clipboard, so Ctrl+Shift+V pastes the latest selection. Without it, a selection reaches only the primary selection (middle click) and Ctrl+Shift+V pastes an older clipboard entry. On Wayland the source window owns the clipboard, so its content is lost when that window closes. `wl-clip-persist --clipboard regular` starts with Hyprland and keeps a copy of the clipboard after the source closes. It keeps no history.

The window border, the launcher border, and the power icon use the bright accent. Fills that carry foreground text keep the darker accent. Blur brightness is 1. `blur_vibrancy` keeps saturation in the glass.

Window corners are `radius_window` (12). Bar groups, notifications and popups are `radius_popup` (16). Windows cast a shadow (`shadow_range`, `shadow_alpha`). The active border is a gradient from the bright accent to the dim accent. Workspace pills stay circles.

`~/.config/genos/opacity-mode` switches `opacity_mode` between `transparent` and `opaque`, regenerates the theme, and reloads Hyprland, Waybar, and Mako. Opaque mode turns blur off.
