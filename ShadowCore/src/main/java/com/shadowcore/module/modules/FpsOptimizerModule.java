package com.shadowcore.module.modules;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import com.shadowcore.performance.FpsBoostService;
import net.minecraft.client.MinecraftClient;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * FPS Optimizer — legit performance suite v1.1
 * <ul>
 *   <li>Entity culling: skips rendering entities beyond configurable distance (via mixin).</li>
 *   <li>Particle limiter: throttles particle spawn (via mixin).</li>
 *   <li>Smart render distance: lowers view distance when FPS drops below target.</li>
 *   <li><b>v1.1 New:</b> Aggressive anti-lag mode — graphics FAST, clouds OFF, biome blend 1, shadows OFF, optimized fog.</li>
 *   <li><b>v1.1 New:</b> Auto-detects Sodium/Lithium/Iris and synergizes instead of conflicting.</li>
 * </ul>
 */
public class FpsOptimizerModule extends Module {

    private int fpsSampleAccum = 0;
    private int sampleCount = 0;
    private int tickCounter = 0;

    public FpsOptimizerModule(boolean enabled) {
        super("fps_optimizer", "FPS Optimizer", "Culls entities, limits particles, smart distance, aggressive anti-lag.", ModuleCategory.PERFORMANCE, enabled);
    }

    @Override
    public void tick(MinecraftClient client) {
        ShadowCoreConfig cfg = ConfigManager.get();

        // Delegate aggressive mode + synergy to service
        FpsBoostService.tick(client);

        // Smart render distance (only if enabled)
        if (!cfg.smartRenderDistance) return;

        if (client.getCurrentFps() > 0) {
            fpsSampleAccum += client.getCurrentFps();
            sampleCount++;
        }
        tickCounter++;
        if (tickCounter < 20) return;
        tickCounter = 0;

        int avgFps = sampleCount > 0 ? fpsSampleAccum / sampleCount : 60;
        fpsSampleAccum = 0;
        sampleCount = 0;

        int currentView = client.options.getViewDistance().getValue();
        if (avgFps < cfg.targetFps - 5 && currentView > cfg.minRenderDistance) {
            client.options.getViewDistance().setValue(currentView - 1);
        } else if (avgFps > cfg.targetFps + 10 && currentView < cfg.maxRenderDistance) {
            client.options.getViewDistance().setValue(currentView + 1);
        }
    }

    @Override
    public Text getHudText() {
        ShadowCoreConfig cfg = ConfigManager.get();
        String mode = cfg.fpsAggressiveMode ? "AGGRESSIVE" : (FpsBoostService.isAggressive() ? "ON" : "Smart");
        String info = cfg.entityCulling ? "Culling:" + cfg.entityCullingDistance : "Culling OFF";
        if (cfg.particleLimiter) info += " P:" + cfg.maxParticlesPerTick;
        info += " [" + mode + "]";
        return Text.literal("FPS Optimizer [" + info + "]").formatted(Formatting.GREEN);
    }
}
