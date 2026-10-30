package com.shadowcore.module.modules;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.MinecraftClient;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * FPS Optimizer — legit performance suite:
 * <ul>
 *   <li>Entity culling: skips rendering entities beyond configurable distance
 *       (implemented via {@link com.shadowcore.mixin.EntityCullingMixin}).</li>
 *   <li>Particle limiter: throttles particle spawn (via {@link com.shadowcore.mixin.ParticleMixin}).</li>
 *   <li>Smart render distance: lowers view distance when FPS drops below target,
 *       raises it when stable. Disabled by default to avoid surprise.</li>
 * </ul>
 */
public class FpsOptimizerModule extends Module {

    // For smart render distance smoothing
    private int fpsSampleAccum = 0;
    private int sampleCount = 0;
    private int tickCounter = 0;

    public FpsOptimizerModule(boolean enabled) {
        super("fps_optimizer", "FPS Optimizer", "Culls distant entities, limits particles, smart render distance.", ModuleCategory.PERFORMANCE, enabled);
    }

    @Override
    public void tick(MinecraftClient client) {
        ShadowCoreConfig cfg = ConfigManager.get();
        if (!cfg.smartRenderDistance) return;

        // Sample FPS every 20 ticks (1 sec)
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
        String info = cfg.entityCulling ? "Culling:" + cfg.entityCullingDistance : "Culling OFF";
        if (cfg.particleLimiter) info += " Particles:" + cfg.maxParticlesPerTick;
        return Text.literal("FPS Optimizer [" + info + "]").formatted(Formatting.GREEN);
    }
}
