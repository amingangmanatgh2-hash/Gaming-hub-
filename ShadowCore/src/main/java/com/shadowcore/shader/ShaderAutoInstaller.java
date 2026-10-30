package com.shadowcore.shader;

import com.shadowcore.ShadowCoreClient;
import com.shadowcore.config.ConfigManager;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.client.MinecraftClient;
import net.minecraft.text.ClickEvent;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Auto-installs bundled BedWars-optimized shaderpack if Iris is present.
 *
 * <p>Legit flow:</p>
 * <ol>
 *   <li>On first launch, checks {@code shaderpacks/ShadowCore_BW.zip} exists.</li>
 *   <li>If Iris is installed, copies bundled shader from mod jar {@code assets/shadowcore/shaders/ShadowCore_BW.zip} to {@code shaderpacks/}.</li>
 *   <li>If Iris missing, shows toast with link to install Iris+Sodium and recommended shader (Complementary / Sildur's).</li>
 *   <li>Never overwrites user shader, never downloads without consent.</li>
 * </ol>
 * <p>BedWars optimizations in bundled shader:</p>
 * <ul>
 *   <li>Bright, high-contrast, no dark caves — see enemies clearly (like Fullbright but via shader)</li>
 *   <li>Lightweight — minimal shadow sampling, 30-60 FPS on low-end</li>
 *   <li>Custom light color — slight cyan tint matching ShadowCore theme</li>
 * </ul>
 */
public final class ShaderAutoInstaller {

    private static final String BUNDLED_PATH = "/assets/shadowcore/shaders/ShadowCore_BW.zip";
    private static boolean checked = false;

    private ShaderAutoInstaller() {}

    public static void tryInstall(MinecraftClient client) {
        if (checked) return;
        checked = true;
        if (!ConfigManager.get().shaderAutoInstall) return;

        Path gameDir = FabricLoader.getInstance().getGameDir();
        Path shaderPacksDir = gameDir.resolve("shaderpacks");
        Path target = shaderPacksDir.resolve(ConfigManager.get().preferredShaderPack);

        boolean irisLoaded = FabricLoader.getInstance().isModLoaded("iris") || FabricLoader.getInstance().isModLoaded("oculus");

        if (Files.exists(target)) {
            // Already installed — maybe notify once
            if (!ConfigManager.get().shaderAutoInstallNotified && irisLoaded) {
                notifyInstalled(client, target.getFileName().toString());
                ConfigManager.get().shaderAutoInstallNotified = true;
                ConfigManager.save();
            }
            return;
        }

        if (!irisLoaded) {
            // No Iris — guide user to install Sodium+Iris for BW
            if (!ConfigManager.get().shaderAutoInstallNotified) {
                notifyNeedIris(client);
                ConfigManager.get().shaderAutoInstallNotified = true;
                ConfigManager.save();
            }
            return;
        }

        // Iris present — try to copy bundled shader
        try {
            Files.createDirectories(shaderPacksDir);
            try (InputStream in = ShaderAutoInstaller.class.getResourceAsStream(BUNDLED_PATH)) {
                if (in == null) {
                    // Bundled shader not in jar yet — create placeholder + guide to download
                    createPlaceholderReadme(shaderPacksDir);
                    notifyPlaceholder(client);
                    return;
                }
                Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
                notifyInstalled(client, target.getFileName().toString());
                ShadowCoreClient.LOGGER.info("[ShadowCore] Shaderpack auto-installed to {}", target);
            }
        } catch (IOException e) {
            ShadowCoreClient.LOGGER.warn("[ShadowCore] Failed to auto-install shader", e);
        }
    }

    private static void createPlaceholderReadme(Path dir) throws IOException {
        Path readme = dir.resolve("ShadowCore_BW_README.txt");
        if (Files.exists(readme)) return;
        String content = """
                ShadowCore BW Shader — lightweight BedWars optimized.

                The bundled shader was not found in this build (placeholder mode).
                For best BW experience (bright, light, beautiful, 60+ FPS):

                1. Install Iris + Sodium:
                   https://modrinth.com/mod/iris  +  https://modrinth.com/mod/sodium
                2. Download a BW-friendly shader (all free):
                   - Complementary Shaders (Unbound) — bright & vibrant: https://modrinth.com/shader/complementary
                   - Sildur's Vibrant Lite — lightweight: https://sildurs-shaders.github.io/
                   - MakeUp Ultra Fast — fastest: https://modrinth.com/shader/makeup-ultrafast
                3. Drop the .zip into this shaderpacks folder
                4. In-game: Options > Video Settings > Shader Packs > select it

                ShadowCore's Fullbright + Zoom already help without shader.
                """;
        Files.writeString(readme, content);
    }

    private static void notifyInstalled(MinecraftClient client, String name) {
        client.execute(() -> {
            if (client.player != null) {
                client.player.sendMessage(
                        Text.literal("[ShadowCore] ").formatted(Formatting.DARK_AQUA)
                                .append(Text.literal("Shader installed: " + name + " → Options > Video > Shader Packs to enable").formatted(Formatting.AQUA)),
                        false);
            }
        });
    }

    private static void notifyNeedIris(MinecraftClient client) {
        client.execute(() -> {
            if (client.player != null) {
                Text text = Text.literal("[ShadowCore] ").formatted(Formatting.DARK_AQUA)
                        .append(Text.literal("For BW shaders install Iris + Sodium — ").formatted(Formatting.GRAY))
                        .append(Text.literal("[Open Modrinth]")
                                .formatted(Formatting.AQUA, Formatting.UNDERLINE)
                                .styled(s -> s.withClickEvent(new ClickEvent(ClickEvent.Action.OPEN_URL, "https://modrinth.com/mod/iris"))));
                client.player.sendMessage(text, false);
            }
        });
    }

    private static void notifyPlaceholder(MinecraftClient client) {
        client.execute(() -> {
            if (client.player != null) {
                client.player.sendMessage(
                        Text.literal("[ShadowCore] Bundled BW shader placeholder created — check shaderpacks/ShadowCore_BW_README.txt").formatted(Formatting.YELLOW),
                        false);
            }
        });
    }
}
