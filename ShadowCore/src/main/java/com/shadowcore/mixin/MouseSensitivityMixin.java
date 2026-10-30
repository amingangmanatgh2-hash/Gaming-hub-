package com.shadowcore.mixin;

import net.minecraft.client.Mouse;
import org.spongepowered.asm.mixin.Mixin;

/**
 * Reserved for future mouse sensitivity fine-tuning while zoomed.
 * Currently zoom sensitivity is handled directly in ZoomModule via options scaling.
 * This mixin is kept as placeholder and does not inject (require=0 safe).
 */
@Mixin(Mouse.class)
public class MouseSensitivityMixin {
    // No injection — ZoomModule handles sensitivity by temporarily scaling
    // MouseOptions.getMouseSensitivity() value while ZoomState.active.
    // Keeping class ensures refmap entry remains valid if we need advanced delta scaling later.
}
