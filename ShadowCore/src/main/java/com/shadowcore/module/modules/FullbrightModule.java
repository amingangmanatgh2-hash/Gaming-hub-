package com.shadowcore.module.modules;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.MinecraftClient;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * Fullbright (legit) — boosts gamma to max (12.0) for night visibility.
 *
 * <p><b>Not X-Ray</b>: does not make blocks transparent or reveal ores/chests through walls.
 * It simply raises the brightness slider beyond vanilla limits, which any resource pack
 * or accessibility setting can also do. Server-safe.</p>
 *
 * <p>Implementation: directly sets {@code options.getGamma()} while enabled, restores on disable.</p>
 */
public class FullbrightModule extends Module {

    public FullbrightModule(boolean enabled) {
        super("fullbright", "Fullbright", "Legit gamma boost — no X-Ray, just brighter nights.", ModuleCategory.WORLD, enabled);
    }

    @Override
    protected void onEnable() {
        var cfg = ConfigManager.get();
        MinecraftClient client = MinecraftClient.getInstance();
        if (client.options == null) return;
        // Remember current gamma
        cfg.previousGamma = client.options.getGamma().getValue();
        client.options.getGamma().setValue(cfg.fullbrightGamma);
        ConfigManager.save();
    }

    @Override
    protected void onDisable() {
        var cfg = ConfigManager.get();
        MinecraftClient client = MinecraftClient.getInstance();
        if (client.options == null) return;
        if (cfg.fullbrightRememberGamma) {
            client.options.getGamma().setValue(cfg.previousGamma);
        } else {
            client.options.getGamma().setValue(1.0);
        }
    }

    @Override
    public void tick(MinecraftClient client) {
        var cfg = ConfigManager.get();
        // Keep gamma clamped in case something else changes it
        if (isEnabled() && client.options != null) {
            double desired = cfg.fullbrightGamma;
            if (Math.abs(client.options.getGamma().getValue() - desired) > 0.01) {
                client.options.getGamma().setValue(desired);
            }
        }
    }

    @Override
    public Text getHudText() {
        return Text.literal("Fullbright ON").formatted(Formatting.YELLOW);
    }
}
