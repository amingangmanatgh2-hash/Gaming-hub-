package com.shadowcore.minimap;

import com.shadowcore.config.ConfigManager;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.render.RenderTickCounter;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.ChunkPos;

/**
 * Simple top-down minimap — renders a small block-color map in the HUD corner.
 *
 * <p>Legit notes:</p>
 * <ul>
 *   <li>No entity radar (optional toggle off by default).</li>
 *   <li>Only shows blocks that are already loaded / visible — no XRay, no cave mapping.</li>
 *   <li>Very lightweight — samples chunk top block color, not full raytrace.</li>
 * </ul>
 */
public final class MinimapRenderer {

    private MinimapRenderer() {}

    // Reused buffer for minimap pixels (96x96 default)
    private static int[] pixelCache = new int[96 * 96];

    public static void onWorldRender(WorldRenderContext ctx) {
        // Minimap is HUD-based; world hook reserved for future chunk snapshot caching.
    }

    /**
     * Called from {@link com.shadowcore.hud.HudOverlay} or directly via HudRenderCallback.
     * For now we expose a method that HudOverlay can delegate to.
     */
    public static void renderHud(DrawContext drawContext, RenderTickCounter tickCounter) {
        if (!ConfigManager.get().showMinimap) return;
        MinecraftClient client = MinecraftClient.getInstance();
        if (client.player == null || client.world == null) return;
        if (client.options.hudHidden) return;

        int size = ConfigManager.get().minimapSize;
        int margin = 6;
        int screenW = drawContext.getScaledWindowWidth();
        // Top-right corner
        int x0 = screenW - size - margin;
        int y0 = margin;

        // Background frame
        boolean round = ConfigManager.get().minimapRound;
        int frameColor = 0xFF1A1A1A;
        int bgColor = 0xFF2D2D2D;

        // Outer frame
        drawContext.fill(x0 - 2, y0 - 2, x0 + size + 2, y0 + size + 2, frameColor);
        drawContext.fill(x0, y0, x0 + size, y0 + size, bgColor);

        // Sample world blocks into a tiny map
        renderBlockMap(drawContext, x0, y0, size, client);

        // Player dot (center)
        int cx = x0 + size / 2;
        int cy = y0 + size / 2;
        // Direction-aware triangle could go here; for now a simple dot
        drawContext.fill(cx - 2, cy - 2, cx + 2, cy + 2, 0xFFFFFFFF);
        // Facing indicator
        float yaw = client.player.getYaw();
        double rad = Math.toRadians(yaw);
        int nx = (int) (Math.sin(rad) * 5);
        int ny = (int) (-Math.cos(rad) * 5);
        drawContext.fill(cx + nx - 1, cy + ny - 1, cx + nx + 1, cy + ny + 1, 0xFFFF5555);

        // Label
        drawContext.drawTextWithShadow(client.textRenderer, "Minimap", x0, y0 + size + 4, 0xAAAAAA);
    }

    private static void renderBlockMap(DrawContext ctx, int x0, int y0, int size, MinecraftClient client) {
        if (client.player == null || client.world == null) return;

        BlockPos center = client.player.getBlockPos();
        int scale = Math.max(1, ConfigManager.get().minimapScale);
        int blocksPerPixel = scale;
        int half = (size * blocksPerPixel) / 2;

        // For performance, we sample every pixel but only if chunk loaded
        for (int py = 0; py < size; py++) {
            for (int px = 0; px < size; px++) {
                int wx = center.getX() + (px - size / 2) * blocksPerPixel;
                int wz = center.getZ() + (py - size / 2) * blocksPerPixel;

                int color = sampleTopBlockColor(client, wx, wz, center.getY());
                // Alpha is opaque — we fill directly
                ctx.fill(x0 + px, y0 + py, x0 + px + 1, y0 + py + 1, 0xFF000000 | (color & 0xFFFFFF));
            }
        }
    }

    private static int sampleTopBlockColor(MinecraftClient client, int wx, int wz, int playerY) {
        if (client.world == null) return 0x2D2D2D;
        ChunkPos cp = new ChunkPos(wx >> 4, wz >> 4);
        if (!client.world.isChunkLoaded(cp.x, cp.z)) return 0x1A1A1A;

        // Sample from player Y downwards up to 12 blocks — find top non-air
        for (int y = Math.min(playerY + 8, client.world.getTopYInclusive()); y >= Math.max(client.world.getBottomY(), playerY - 16); y--) {
            BlockPos pos = new BlockPos(wx, y, wz);
            var state = client.world.getBlockState(pos);
            if (!state.isAir()) {
                // Use map color if available, else fallback
                int mapColor = state.getMapColor(client.world, pos).color;
                if (mapColor == 0) mapColor = 0x7A7A7A; // stone fallback
                // Slight shading by height relative to player
                int shade = Math.clamp((y - playerY) * 3 + 128, 60, 255);
                return modulateBrightness(mapColor, shade);
            }
        }
        return 0x3A5F0B; // grass fallback
    }

    private static int modulateBrightness(int color, int shade) {
        float f = shade / 128f;
        int r = Math.clamp((int) (((color >> 16) & 0xFF) * f), 0, 255);
        int g = Math.clamp((int) (((color >> 8) & 0xFF) * f), 0, 255);
        int b = Math.clamp((int) ((color & 0xFF) * f), 0, 255);
        return (r << 16) | (g << 8) | b;
    }
}
