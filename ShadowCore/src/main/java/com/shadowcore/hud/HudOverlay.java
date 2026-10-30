package com.shadowcore.hud;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.registry.RegistryKey;
import net.minecraft.util.math.BlockPos;
import net.minecraft.world.biome.Biome;

import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;

/**
 * Legit info HUD — coords, FPS, biome, time, direction.
 *
 * <p>Renders in top-left corner with semi-transparent background. Respects
 * {@link ShadowCoreConfig#hudEnabled} and per-field toggles.</p>
 */
public final class HudOverlay {

    private HudOverlay() {}

    private static final DateTimeFormatter TIME_FMT = DateTimeFormatter.ofPattern("HH:mm:ss");

    public static void render(DrawContext ctx) {
        ShadowCoreConfig cfg = ConfigManager.get();
        if (!cfg.hudEnabled) return;

        MinecraftClient client = MinecraftClient.getInstance();
        if (client.player == null || client.world == null) return;
        if (client.options.hudHidden) return;

        List<String> lines = new ArrayList<>();

        if (cfg.hudShowFps) {
            lines.add("FPS: " + client.getCurrentFps());
        }
        if (cfg.hudShowCoords) {
            ClientPlayerEntity p = client.player;
            BlockPos pos = p.getBlockPos();
            lines.add(String.format("XYZ: %d / %d / %d", pos.getX(), pos.getY(), pos.getZ()));
        }
        if (cfg.hudShowDirection) {
            lines.add("Facing: " + directionString(client));
        }
        if (cfg.hudShowBiome) {
            String biome = biomeName(client);
            if (biome != null) lines.add("Biome: " + biome);
        }
        if (cfg.hudShowTime) {
            long time = client.world.getTimeOfDay() % 24000;
            String dayTime = formatMcTime(time);
            lines.add("Time: " + dayTime + " (" + LocalTime.now().format(TIME_FMT) + ")");
        }

        // Module status line (Fullbright indicator etc.)
        // Could be extended to show active modules

        if (lines.isEmpty()) return;

        // Render background + text
        int x = 6;
        int y = 6;
        int bg = 0xAA000000; // ARGB
        int maxWidth = 0;
        for (String s : lines) {
            int w = client.textRenderer.getWidth(s);
            if (w > maxWidth) maxWidth = w;
        }
        int lineHeight = 10;
        int padding = 4;
        int totalHeight = lines.size() * lineHeight + padding * 2 - 2;

        // Background
        ctx.fill(x - padding, y - padding, x + maxWidth + padding, y + totalHeight - padding, bg);

        // Text
        for (String line : lines) {
            ctx.drawTextWithShadow(client.textRenderer, line, x, y, 0xFFFFFF);
            y += lineHeight;
        }
    }

    private static String directionString(MinecraftClient client) {
        if (client.player == null) return "?";
        float yaw = client.player.getYaw() % 360;
        if (yaw < 0) yaw += 360;
        return switch ((int) ((yaw + 45) / 90) % 4) {
            case 0 -> "South (+Z)";
            case 1 -> "West (-X)";
            case 2 -> "North (-Z)";
            case 3 -> "East (+X)";
            default -> "?";
        };
    }

    private static String biomeName(MinecraftClient client) {
        if (client.player == null || client.world == null) return null;
        BlockPos pos = client.player.getBlockPos();
        RegistryKey<Biome> key = client.world.getBiome(pos).getKey().orElse(null);
        if (key == null) return null;
        String path = key.getValue().getPath();
        // Title-case biome path: "plains" -> "Plains", "snowy_plains" -> "Snowy Plains"
        String[] parts = path.split("_");
        StringBuilder sb = new StringBuilder();
        for (String p : parts) {
            if (!p.isEmpty()) {
                if (sb.length() > 0) sb.append(' ');
                sb.append(Character.toUpperCase(p.charAt(0))).append(p.substring(1));
            }
        }
        return sb.toString();
    }

    private static String formatMcTime(long time) {
        // 0 = 06:00, 6000 = 12:00, 12000 = 18:00, 18000 = 00:00
        long totalMinutes = (time * 24 * 60 / 24000 + 6 * 60) % (24 * 60);
        long h = totalMinutes / 60;
        long m = totalMinutes % 60;
        return String.format("%02d:%02d", h, m);
    }
}
