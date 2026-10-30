package com.shadowcore.config;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.shadowcore.ShadowCoreClient;
import net.fabricmc.loader.api.FabricLoader;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Loads / saves {@link ShadowCoreConfig} to {@code config/shadowcore.json}.
 *
 * <p>Gson is used because it's bundled with Minecraft — no extra dependency.</p>
 */
public final class ConfigManager {

    private static final Gson GSON = new GsonBuilder()
            .setPrettyPrinting()
            .disableHtmlEscaping()
            .create();

    private static final Path CONFIG_PATH =
            FabricLoader.getInstance().getConfigDir().resolve("shadowcore.json");

    private static ShadowCoreConfig config = new ShadowCoreConfig();

    private ConfigManager() {}

    public static ShadowCoreConfig get() {
        return config;
    }

    /** Load from disk, or keep defaults if file missing/corrupt. */
    public static void load() {
        if (!Files.exists(CONFIG_PATH)) {
            save(); // create default file
            return;
        }
        try {
            String json = Files.readString(CONFIG_PATH);
            ShadowCoreConfig loaded = GSON.fromJson(json, ShadowCoreConfig.class);
            if (loaded != null) {
                config = loaded;
                ShadowCoreClient.LOGGER.info("[ShadowCore] Config loaded from {}", CONFIG_PATH);
            }
        } catch (IOException | com.google.gson.JsonSyntaxException e) {
            ShadowCoreClient.LOGGER.warn("[ShadowCore] Failed to load config, using defaults", e);
        }
    }

    /** Persist current config to disk. */
    public static void save() {
        try {
            Files.createDirectories(CONFIG_PATH.getParent());
            String json = GSON.toJson(config);
            Files.writeString(CONFIG_PATH, json);
        } catch (IOException e) {
            ShadowCoreClient.LOGGER.error("[ShadowCore] Failed to save config", e);
        }
    }

    /** Convenience — sets and saves atomically. */
    public static void saveAsync() {
        // For simplicity we save synchronously; Fabric config writes are cheap.
        // If needed, this can be moved to a background executor.
        save();
    }
}
