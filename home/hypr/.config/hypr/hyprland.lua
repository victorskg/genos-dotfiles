package.loaded.colors = nil
local theme = require("colors")

local mod = "SUPER"
local home = os.getenv("HOME")

hl.monitor({
    output = "",
    mode = "preferred",
    position = "auto",
    scale = 1,
})

hl.config({
    general = {
        gaps_in = theme.gap_in,
        gaps_out = theme.gap_out,
        border_size = theme.border,
        col = {
            active_border = { colors = { theme.accent_bright, theme.accent_dim }, angle = 45 },
            inactive_border = theme.line,
        },
        layout = "dwindle",
    },

    decoration = {
        rounding = theme.radius_window,
        shadow = {
            enabled = true,
            range = theme.shadow_range,
            render_power = 3,
            color = "rgba(000000" .. string.format("%02x", math.floor(theme.shadow_alpha * 255 + 0.5)) .. ")",
        },
        blur = {
            enabled = theme.blur_enabled,
            size = theme.blur_size,
            passes = theme.blur_passes,
            noise = theme.blur_noise,
            brightness = theme.blur_brightness,
            contrast = 1.0,
            vibrancy = theme.blur_vibrancy,
            new_optimizations = true,
            xray = false,
            popups = true,
        },
    },

    animations = {
        enabled = true,
    },

    misc = {
        background_color = theme.bg,
        disable_hyprland_logo = true,
        disable_splash_rendering = true,
    },

    input = {
        kb_layout = "br",
        kb_variant = "abnt2",
    },
})

hl.curve("quint", { type = "bezier", points = { { 0.23, 1 }, { 0.32, 1 } } })
hl.curve("ease", { type = "bezier", points = { { 0.4, 0.0 }, { 0.2, 1.0 } } })
hl.animation({ leaf = "windows", enabled = true, speed = 4, bezier = "quint" })
hl.animation({ leaf = "windowsIn", enabled = true, speed = 4.5, bezier = "quint", style = "popin 90%" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 3, bezier = "ease", style = "popin 90%" })
hl.animation({ leaf = "fade", enabled = true, speed = 4, bezier = "ease" })
hl.animation({ leaf = "layers", enabled = true, speed = 4, bezier = "quint", style = "fade" })
hl.animation({ leaf = "border", enabled = true, speed = 6, bezier = "ease" })
hl.animation({ leaf = "workspaces", enabled = true, speed = 5, bezier = "quint", style = "slidefade 12%" })

local function layer_blur(name, namespace)
    hl.layer_rule({
        name = name,
        match = { namespace = namespace },
        blur = true,
        ignore_alpha = 0.15,
    })
end

layer_blur("bar-blur", "waybar")
layer_blur("launcher-blur", "fuzzel")
layer_blur("notification-blur", "notifications")

hl.window_rule({
    name = "open-alone",
    match = { modal = false },
    workspace = "emptynm",
})

hl.bind(mod .. " + T", hl.dsp.exec_cmd("kitty"))
hl.bind(mod .. " + SPACE", hl.dsp.exec_cmd("fuzzel"))
hl.bind(mod .. " + B", hl.dsp.exec_cmd("zen-browser"))
hl.bind(mod .. " + Q", hl.dsp.window.close())
hl.bind(mod .. " + F", hl.dsp.window.fullscreen({ mode = "fullscreen", action = "toggle" }))
hl.bind(mod .. " + V", hl.dsp.window.float({ action = "toggle" }))
hl.bind(mod .. " + M", hl.dsp.exit())

for i = 1, 10 do
    local key = i % 10
    hl.bind(mod .. " + " .. key, hl.dsp.focus({ workspace = i }))
    hl.bind(mod .. " + SHIFT + " .. key, hl.dsp.window.move({ workspace = i }))
end

hl.bind(mod .. " + comma", hl.dsp.focus({ workspace = "r-1" }))
hl.bind(mod .. " + period", hl.dsp.focus({ workspace = "r+1" }))
hl.bind(mod .. " + SHIFT + comma", hl.dsp.window.move({ workspace = "r-1" }))
hl.bind(mod .. " + SHIFT + period", hl.dsp.window.move({ workspace = "r+1" }))

hl.bind("Print", hl.dsp.exec_cmd(home .. "/.config/hypr/screenshot"))
hl.bind("SHIFT + Print", hl.dsp.exec_cmd(home .. "/.config/hypr/screenshot region"))
local osd = home .. "/.config/hypr/osd"
local held = { locked = true, repeating = true }
hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd(osd .. " volume-up"), held)
hl.bind("XF86AudioLowerVolume", hl.dsp.exec_cmd(osd .. " volume-down"), held)
hl.bind("XF86AudioMute", hl.dsp.exec_cmd(osd .. " volume-mute"), { locked = true })
hl.bind("XF86MonBrightnessUp", hl.dsp.exec_cmd(osd .. " brightness-up"), held)
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd(osd .. " brightness-down"), held)

hl.bind(mod .. " + W", hl.dsp.exec_cmd(home .. "/.config/hypr/wallpaper next"))
hl.bind(mod .. " + SHIFT + W", hl.dsp.exec_cmd(home .. "/.config/hypr/wallpaper prev"))

hl.bind(mod .. " + SHIFT + left", hl.dsp.window.move({ monitor = "eDP-1" }))
hl.bind(mod .. " + SHIFT + right", hl.dsp.window.move({ monitor = "HDMI-A-1" }))

hl.on("hyprland.start", function()
    hl.exec_cmd("waybar")
    hl.exec_cmd("mako")
    hl.exec_cmd("wl-clip-persist --clipboard regular")
    hl.exec_cmd("/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1")
    hl.exec_cmd(home .. "/.config/hypr/workspace-follow.py")
    hl.exec_cmd(home .. "/.config/hypr/wallpaper show")
end)

hl.on("config.reloaded", function()
    hl.exec_cmd(home .. "/.config/hypr/workspace-follow.py")
end)
