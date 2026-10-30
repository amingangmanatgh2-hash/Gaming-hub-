package com.shadowcore.util;

import net.minecraft.client.gui.DrawContext;
import net.minecraft.util.math.MathHelper;

/**
 * Shared HUD / GUI rendering helpers for ShadowCore.
 */
public final class RenderUtils {

    private RenderUtils() {}

    /** Draw rounded-ish rect (actually chamfered — true round needs shader). */
    public static void drawPanel(DrawContext ctx, int x, int y, int w, int h, int color, int borderColor) {
        // Border
        ctx.fill(x - 1, y - 1, x + w + 1, y + h + 1, borderColor);
        // Fill
        ctx.fill(x, y, x + w, y + h, color);
    }

    /** Lerp helper for smooth GUI animations. */
    public static float lerp(float a, float b, float t) {
        return MathHelper.lerp(t, a, b);
    }

    /** ARGB with alpha override. */
    public static int withAlpha(int rgb, int alpha) {
        return (alpha << 24) | (rgb & 0xFFFFFF);
    }
}
