package com.shadowcore.module.modules;

import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;

/**
 * HUD Info — toggle for coords/FPS/biome overlay.
 * Actual rendering in {@link com.shadowcore.hud.HudOverlay}.
 */
public class HudInfoModule extends Module {
    public HudInfoModule(boolean enabled) {
        super("hud_info", "HUD Info", "Shows coords, FPS, biome, time & direction.", ModuleCategory.HUD, enabled);
    }
}
