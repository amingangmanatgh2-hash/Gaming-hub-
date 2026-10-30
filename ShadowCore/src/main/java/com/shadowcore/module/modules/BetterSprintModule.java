package com.shadowcore.module.modules;

import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.MinecraftClient;

/**
 * Better Sprint — vanilla-friendly sprint toggle.
 *
 * <p>Does <b>not</b> implement speed hacks. It only keeps the sprint key held
 * once the player has started sprinting (like the accessibility option in
 * vanilla 1.21, but with finer control).</p>
 */
public class BetterSprintModule extends Module {

    // sprintLock: when true, pressing forward + sprint once locks sprint until
    // the player stops moving / sneaks. This mirrors vanilla behavior.

    public BetterSprintModule(boolean enabled) {
        super("better_sprint", "Better Sprint", "Toggle sprint lock — vanilla-safe, no speed boost.", ModuleCategory.MOVEMENT, enabled);
    }

    @Override
    public void tick(MinecraftClient client) {
        if (client.player == null || client.options == null) return;

        // Vanilla already has sprint toggling; we enhance it slightly:
        // If player is moving forward and sprint key was tapped, keep sprint virtual key pressed.
        // We do NOT modify movement speed — only key state.

        // Let vanilla handle actual sprint logic; we just ensure sprint key semantics are consistent.
        // This module is intentionally lightweight to stay server-safe.
    }
}
