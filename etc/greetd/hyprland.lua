hl.monitor({
    output = "",
    mode = "preferred",
    position = "auto",
    scale = 1,
})

hl.config({
    general = {
        gaps_in = 0,
        gaps_out = 0,
        border_size = 0,
    },

    decoration = {
        shadow = { enabled = false },
        blur = { enabled = false },
    },

    animations = { enabled = false },

    input = {
        kb_layout = "br",
        kb_variant = "abnt2",
    },

    misc = {
        background_color = "rgb(0B0909)",
        disable_hyprland_logo = true,
        disable_splash_rendering = true,
        force_default_wallpaper = 0,
    },
})

hl.on("hyprland.start", function()
    hl.exec_cmd("/usr/bin/regreet; /usr/bin/hyprctl dispatch exit")
end)
