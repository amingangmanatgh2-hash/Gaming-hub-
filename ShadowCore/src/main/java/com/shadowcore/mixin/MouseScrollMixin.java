package com.shadowcore.mixin;

import com.shadowcore.module.ModuleManager;
import com.shadowcore.module.modules.ZoomModule;
import net.minecraft.client.Mouse;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Captures mouse scroll for infinite zoom.
 * When zooming (C or middle mouse held), scroll adjusts zoom level instead of hotbar.
 */
@Mixin(Mouse.class)
public class MouseScrollMixin {

    @Inject(method = "onMouseScroll", at = @At("HEAD"), cancellable = true)
    private void shadowcore$handleZoomScroll(long window, double horizontal, double vertical, CallbackInfo ci) {
        // Find zoom module
        var mgr = ModuleManager.getInstance();
        if (mgr == null) return;
        var opt = mgr.get("zoom");
        if (opt.isEmpty() || !opt.get().isEnabled()) return;
        ZoomModule zoom = (ZoomModule) opt.get();

        // If zoom key pressed, hijack scroll for zoom
        boolean zooming = false;
        try {
            // Check via same logic as module
            var client = net.minecraft.client.MinecraftClient.getInstance();
            long handle = client.getWindow().getHandle();
            boolean c = com.shadowcore.ShadowCoreClient.zoomKey != null && com.shadowcore.ShadowCoreClient.zoomKey.isPressed();
            boolean middle = false;
            if (com.shadowcore.config.ConfigManager.get().zoomMiddleMouse) {
                middle = org.lwjgl.glfw.GLFW.glfwGetMouseButton(handle, org.lwjgl.glfw.GLFW.GLFW_MOUSE_BUTTON_MIDDLE) == org.lwjgl.glfw.GLFW.GLFW_PRESS;
            }
            zooming = c || middle;
        } catch (Exception ignored) {}

        if (zooming && vertical != 0) {
            zoom.handleScroll(vertical * 0.5); // scale scroll amount
            ci.cancel(); // don't change hotbar while zooming
        }
    }
}
