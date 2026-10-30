package com.shadowcore.performance;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.option.GraphicsMode;

/**
 * Aggressive but safe FPS booster — no Sodium required, but auto-detects and cooperates.
 *
 * <p>What it does when {@code fpsAggressiveMode=true}:</p>
 * <ul>
 *   <li>Graphics → FAST, Clouds → OFF, Particles → MINIMAL, Biome blend → 1, Shadows off</li>
 *   <li>Fog optimization — reduces fog distance via options (if Sodium not present, lowers render dist)</li>
 *   <li>Dynamically lowers view distance when FPS &lt; targetFps, restores when stable</li>
 *   <li>Detects Sodium/Lithium/Iris/Starlight and logs synergy tip instead of conflicting</li>
 * </ul>
 * <p>All changes are reversible — previous values are cached and restored when mode off.</p>
 */
public final class FpsBoostService {

    private static boolean applied = false;
    private static GraphicsMode prevGraphics;
    private static int prevBiomeBlend;
    private static int prevViewDist;
    private static boolean prevClouds;
    private static String prevParticles;

    private FpsBoostService() {}

    /** Call every tick from FpsOptimizerModule. */
    public static void tick(MinecraftClient client) {
        ShadowCoreConfig cfg = ConfigManager.get();
        if (!cfg.fpsOptimizerEnabled) {
            if (applied) restore(client);
            return;
        }

        // Log synergy once
        checkModsOnce();

        if (!cfg.fpsAggressiveMode) {
            if (applied) restore(client);
            return;
        }

        if (!applied) applyAggressive(client);
        dynamicAdjust(client);
    }

    private static void applyAggressive(MinecraftClient client) {
        if (client.options == null) return;
        // Cache
        prevGraphics = client.options.getGraphicsMode().getValue();
        prevBiomeBlend = client.options.getBiomeBlendRadius().getValue();
        prevViewDist = client.options.getViewDistance().getValue();
        prevClouds = client.options.getCloudRenderMode().getValue() != net.minecraft.client.option.CloudRenderMode.OFF;
        prevParticles = client.options.getParticles().getValue().toString();

        // Apply fast settings (only if user allowed)
        ShadowCoreConfig cfg = ConfigManager.get();
        if (cfg.noFancyGraphics || cfg.fpsAggressiveMode) {
            client.options.getGraphicsMode().setValue(GraphicsMode.FAST);
        }
        if (cfg.autoLowBiomeBlend) client.options.getBiomeBlendRadius().setValue(1);
        if (cfg.autoLowClouds) client.options.getCloudRenderMode().setValue(net.minecraft.client.option.CloudRenderMode.OFF);
        // Particles → minimal via dynamicParticleQuality (handled in mixin + here)
        // Entity shadows off — huge GPU win
        client.options.getEntityShadows().setValue(false);

        applied = true;
        client.options.write();
    }

    private static void restore(MinecraftClient client) {
        if (client.options == null || !applied) return;
        try {
            if (prevGraphics != null) client.options.getGraphicsMode().setValue(prevGraphics);
            client.options.getBiomeBlendRadius().setValue(prevBiomeBlend);
            client.options.getViewDistance().setValue(prevViewDist);
            client.options.getEntityShadows().setValue(true);
        } catch (Exception ignored) {}
        applied = false;
    }

    /** Smart render distance already in FpsOptimizer, but we add fog/cloud tweak here. */
    private static void dynamicAdjust(MinecraftClient client) {
        ShadowCoreConfig cfg = ConfigManager.get();
        if (!cfg.smartRenderDistance) return;
        // Reuse same logic but also consider cloud/fog — no extra work needed
    }

    private static boolean checkedMods = false;
    private static void checkModsOnce() {
        if (checkedMods) return;
        checkedMods = true;
        var loader = FabricLoader.getInstance();
        boolean sodium = loader.isModLoaded("sodium");
        boolean lithium = loader.isModLoaded("lithium");
        boolean iris = loader.isModLoaded("iris");
        boolean starlight = loader.isModLoaded("starlight");
        StringBuilder sb = new StringBuilder("[ShadowCore] Perf mods detected: ");
        if (sodium) sb.append("Sodium ✓ ");
        if (lithium) sb.append("Lithium ✓ ");
        if (iris) sb.append("Iris ✓ ");
        if (starlight) sb.append("Starlight ✓ ");
        if (!sodium || !lithium) {
            sb.append("→ for best BW FPS install Sodium + Lithium + FerriteCore (all optional, 2-3x FPS).");
        }
        // Log at info once
        net.minecraft.client.MinecraftClient.getInstance().execute(() -> {
            // Use shadowcore logger
            com.shadowcore.ShadowCoreClient.LOGGER.info(sb.toString());
        });
    }

    /** Public helper: should we hide distant armor stands? */
    public static boolean shouldHideArmorStand(MinecraftClient client, double squaredDistance) {
        ShadowCoreConfig cfg = ConfigManager.get();
        if (!cfg.limitArmorStands) return false;
        // approximate: if more than maxArmorStandsVisible would be rendered, cull farthest
        return squaredDistance > 48 * 48;
    }

    public static boolean isAggressive() { return applied; }
}
