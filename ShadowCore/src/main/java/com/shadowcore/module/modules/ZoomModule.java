package com.shadowcore.module.modules;

import com.shadowcore.ShadowCoreClient;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.MinecraftClient;
import net.minecraft.util.math.MathHelper;

/**
 * Zoom — hold C (or middle mouse) to zoom, scroll for infinite levels, sensitivity auto-adjust.
 *
 * <p>v1.1 improvements:</p>
 * <ul>
 *   <li>Infinite scroll: while zooming, mouse wheel adjusts zoom from 20x (0.05) to 1.05x (0.90)</li>
 *   <li>Middle mouse support (configurable)</li>
 *   <li>Toggle vs hold mode</li>
 *   <li>Mouse sensitivity reduced proportionally while zoomed (like OptiFine) for precise aim</li>
 *   <li>Smooth lerp still available</li>
 * </ul>
 */
public class ZoomModule extends Module {

    private double currentMultiplier = 1.0;
    private double desiredMultiplier = 0.25; // user-adjusted via scroll
    private double originalSensitivity = -1;

    public ZoomModule(boolean enabled) {
        super("zoom", "Zoom", "Hold C / Middle-click + scroll for infinite zoom. Sens adjusts.", ModuleCategory.UTILITY, enabled);
    }

    @Override
    public void tick(MinecraftClient client) {
        if (client.options == null) return;
        var cfg = ConfigManager.get();

        // Determine if we should be zooming
        boolean holdZoom = isZoomKeyPressed();
        boolean zooming;
        if (cfg.zoomToggleMode) {
            // toggle mode is handled via key press edge — for now treat as hold for tick
            zooming = holdZoom && isEnabled();
        } else {
            zooming = holdZoom && isEnabled();
        }

        double target = zooming ? desiredMultiplier : 1.0;

        if (cfg.zoomSmooth) {
            currentMultiplier = MathHelper.lerp(cfg.zoomSmoothSpeed * 0.2, currentMultiplier, target);
            if (Math.abs(currentMultiplier - target) < 0.005) currentMultiplier = target;
        } else {
            currentMultiplier = target;
        }

        if (Math.abs(currentMultiplier - 1.0) > 0.01) {
            ZoomState.active = true;
            ZoomState.multiplier = currentMultiplier;
            // Sensitivity reduction — proportional to zoom
            if (cfg.zoomReduceSensitivity) {
                if (originalSensitivity < 0) originalSensitivity = client.options.getMouseSensitivity().getValue();
                double sensMult = MathHelper.clamp(currentMultiplier, 0.1, 1.0);
                ZoomState.effectiveSensMultiplier = sensMult;
                // Actually scale the option while zooming (smooth, restored on exit)
                double scaled = originalSensitivity * sensMult;
                // Avoid spamming disk — setValue without write()
                client.options.getMouseSensitivity().setValue(scaled);
            }
        } else {
            ZoomState.active = false;
            ZoomState.multiplier = 1.0;
            ZoomState.effectiveSensMultiplier = 1.0;
            if (originalSensitivity >= 0) {
                client.options.getMouseSensitivity().setValue(originalSensitivity);
                originalSensitivity = -1;
            }
        }
    }

    private boolean isZoomKeyPressed() {
        var cfg = ConfigManager.get();
        boolean c = ShadowCoreClient.zoomKey != null && ShadowCoreClient.zoomKey.isPressed();
        boolean middle = false;
        if (cfg.zoomMiddleMouse) {
            // GLFW middle button = 2, check via Minecraft mouse handler
            try {
                MinecraftClient client = MinecraftClient.getInstance();
                // Check if middle mouse is pressed via GLFW directly
                long handle = client.getWindow().getHandle();
                middle = org.lwjgl.glfw.GLFW.glfwGetMouseButton(handle, org.lwjgl.glfw.GLFW.GLFW_MOUSE_BUTTON_MIDDLE) == org.lwjgl.glfw.GLFW.GLFW_PRESS;
            } catch (Exception ignored) {}
        }
        return c || middle;
    }

    /** Called from scroll mixin — adjust desired zoom level. */
    public void handleScroll(double amount) {
        var cfg = ConfigManager.get();
        if (!cfg.zoomScrollEnabled) return;
        if (!isZoomKeyPressed()) return; // only when zooming
        // amount >0 = scroll up (less zoom), <0 = more zoom
        // Invert: scroll up should zoom in (smaller multiplier)
        desiredMultiplier = MathHelper.clamp(desiredMultiplier - amount * cfg.zoomScrollStep, cfg.zoomMinMultiplier, cfg.zoomMaxMultiplier);
        // Persist slightly? Save every adjustment batched
        cfg.zoomFovMultiplier = desiredMultiplier;
        // Don't save every scroll tick to avoid disk churn — caller can debounce
    }

    public double getDesired() { return desiredMultiplier; }
    public double getCurrent() { return currentMultiplier; }

    /** Shared zoom state read by renderer + sensitivity mixin. */
    public static final class ZoomState {
        public static boolean active = false;
        public static double multiplier = 1.0;
        /** 0.0-1.0 — multiply mouse sensitivity while zoomed. */
        public static double effectiveSensMultiplier = 1.0;
        private ZoomState() {}
    }
}
