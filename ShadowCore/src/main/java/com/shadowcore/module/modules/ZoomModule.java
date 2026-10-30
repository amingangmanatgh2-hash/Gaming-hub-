package com.shadowcore.module.modules;

import com.shadowcore.ShadowCoreClient;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.MinecraftClient;
import net.minecraft.util.math.MathHelper;

/**
 * Zoom — hold C (configurable) to zoom FOV, like OptiFine / Logical Zoom.
 *
 * <p>Purely client-side FOV change. No server interaction.</p>
 */
public class ZoomModule extends Module {

    private double currentMultiplier = 1.0;
    private double originalFov = -1;

    public ZoomModule(boolean enabled) {
        super("zoom", "Zoom", "Hold C to zoom — smooth, configurable strength.", ModuleCategory.UTILITY, enabled);
    }

    @Override
    public void tick(MinecraftClient client) {
        if (client.options == null) return;
        var cfg = ConfigManager.get();

        boolean zooming = ShadowCoreClient.zoomKey != null && ShadowCoreClient.zoomKey.isPressed() && isEnabled();

        double target = zooming ? cfg.zoomFovMultiplier : 1.0;

        if (cfg.zoomSmooth) {
            currentMultiplier = MathHelper.lerp(cfg.zoomSmoothSpeed * 0.2, currentMultiplier, target);
            if (Math.abs(currentMultiplier - target) < 0.005) currentMultiplier = target;
        } else {
            currentMultiplier = target;
        }

        // Apply via FOV effect — we don't overwrite options FOV, we expose multiplier
        // for mixin / HUD to read. Actual FOV transform is in GameRenderer mixin if present,
        // otherwise we use a simple client.options fov trick:
        if (Math.abs(currentMultiplier - 1.0) > 0.01) {
            if (originalFov < 0) originalFov = client.options.getFov().getValue();
            // We expose multiplier statically for a renderer hook
            ZoomState.active = true;
            ZoomState.multiplier = currentMultiplier;
        } else {
            ZoomState.active = false;
            ZoomState.multiplier = 1.0;
            if (originalFov >= 0) {
                // restore not needed — we never overwrote persistent option
                originalFov = -1;
            }
        }
    }

    /**
     * Shared zoom state read by renderer.
     */
    public static final class ZoomState {
        public static boolean active = false;
        public static double multiplier = 1.0;

        private ZoomState() {}
    }
}
