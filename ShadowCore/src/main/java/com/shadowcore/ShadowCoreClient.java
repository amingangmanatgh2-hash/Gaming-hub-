package com.shadowcore;

import com.shadowcore.command.ShadowCoreCommand;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.entity.VerityRenderer;
import com.shadowcore.gui.ClickGuiScreen;
import com.shadowcore.hud.HudOverlay;
import com.shadowcore.minimap.MinimapRenderer;
import com.shadowcore.module.ModuleManager;
import com.shadowcore.shader.ShaderAutoInstaller;
import com.shadowcore.waypoint.WaypointManager;
import com.shadowcore.waypoint.WaypointRenderer;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.fabricmc.fabric.api.client.rendering.v1.EntityRendererRegistry;
import net.fabricmc.fabric.api.client.rendering.v1.HudRenderCallback;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.option.KeyBinding;
import net.minecraft.client.util.InputUtil;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import org.lwjgl.glfw.GLFW;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * ShadowCore — Client entrypoint v1.1
 *
 * <p>This is a <b>legit-only</b> utility / performance mod. No cheats, no X-Ray, no KillAura,
 * no Fly exploits. Every feature is server-friendly and anti-cheat compatible.</p>
 *
 * <p>v1.1: Added Sodium/Iris shader auto-install, aggressive FPS boost, infinite scroll Zoom,
 * and Verity companion (no external API, pure vanilla AI).</p>
 */
public class ShadowCoreClient implements ClientModInitializer {

    public static final String MOD_ID = "shadowcore";
    public static final Logger LOGGER = LoggerFactory.getLogger("ShadowCore");

    private static ShadowCoreClient instance;

    // ---- Keybinds -----------------------------------------------------------
    public static KeyBinding openClickGui; // Left Shift primary
    public static KeyBinding openClickGuiRight; // Right Shift alias
    public static KeyBinding zoomKey;
    public static KeyBinding addWaypointKey;
    public static KeyBinding toggleMinimapKey;

    private ModuleManager moduleManager;
    private WaypointManager waypointManager;

    @Override
    public void onInitializeClient() {
        instance = this;
        LOGGER.info("[ShadowCore] Initializing legit utility edition v1.1 — FPS Boost + BW Shader + Verity");

        // 1) Config
        ConfigManager.load();

        // 2) Managers
        this.moduleManager = new ModuleManager();
        this.waypointManager = new WaypointManager();
        this.waypointManager.load();
        ModuleManager.getInstance().init();

        // 3) Keybinds
        registerKeybinds();

        // 4) Entity renderer — Verity companion
        try {
            EntityRendererRegistry.register(ShadowCore.VERITY_TYPE, VerityRenderer::new);
            LOGGER.info("[ShadowCore] Verity renderer registered");
        } catch (Throwable t) {
            // In case common init hasn't run yet (client-only launch), log but continue
            LOGGER.warn("[ShadowCore] Verity renderer deferred: {}", t.getMessage());
        }

        // 5) Client tick
        ClientTickEvents.END_CLIENT_TICK.register(this::onEndTick);

        // 6) HUD overlay — includes minimap + coords
        HudRenderCallback.EVENT.register((drawContext, tickDelta) ->
                HudOverlay.render(drawContext));

        // Minimap is rendered inside HudOverlay; also register direct callback for overlay stacking
        HudRenderCallback.EVENT.register((drawContext, tickDelta) -> {
            MinimapRenderer.renderHud(drawContext, tickDelta);
        });

        // 7) World render — waypoints
        WorldRenderEvents.AFTER_TRANSLUCENT.register(ctx -> {
            WaypointRenderer.render(ctx);
            MinimapRenderer.onWorldRender(ctx);
        });

        // 8) Command
        ShadowCoreCommand.register();

        // 9) Shader auto-install — deferred 3 sec after world load to avoid spam
        ClientTickEvents.END_CLIENT_TICK.register(client -> {
            if (client.player != null) {
                // Run once when player exists
                ShaderAutoInstaller.tryInstall(client);
            }
        });

        LOGGER.info("[ShadowCore] Ready — Right Shift GUI | C/MiddleMouse+Scroll Zoom | B waypoint | M minimap | Verity egg in creative");
    }

    private void registerKeybinds() {
        // Left Shift is now main (your request), Right Shift kept as alias
        openClickGui = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.clickgui",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_LEFT_SHIFT,
                "category.shadowcore.main"
        ));
        openClickGuiRight = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.clickgui_right",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_RIGHT_SHIFT,
                "category.shadowcore.main"
        ));
        zoomKey = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.zoom",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_C,
                "category.shadowcore.main"
        ));
        addWaypointKey = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.add_waypoint",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_B,
                "category.shadowcore.main"
        ));
        toggleMinimapKey = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.toggle_minimap",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_M,
                "category.shadowcore.main"
        ));
    }

    private void onEndTick(MinecraftClient client) {
        if (client.player == null) return;

        while (openClickGui.wasPressed() || openClickGuiRight.wasPressed()) client.setScreen(new ClickGuiScreen());

        while (addWaypointKey.wasPressed()) {
            waypointManager.createAtPlayer("Waypoint-" + (waypointManager.size() + 1));
            client.player.sendMessage(Text.literal("[ShadowCore] ").formatted(Formatting.DARK_GRAY)
                    .append(Text.literal("Waypoint added").formatted(Formatting.GREEN)), false);
        }

        while (toggleMinimapKey.wasPressed()) {
            boolean newVal = !ConfigManager.get().showMinimap;
            ConfigManager.get().showMinimap = newVal;
            ConfigManager.save();
            client.player.sendMessage(Text.literal("[ShadowCore] Minimap " + (newVal ? "ON" : "OFF")).formatted(Formatting.GRAY), true);
        }

        moduleManager.tick(client);
    }

    // ---- Singleton ----------------------------------------------------------
    public static ShadowCoreClient getInstance() { return instance; }
    public ModuleManager getModuleManager() { return moduleManager; }
    public WaypointManager getWaypointManager() { return waypointManager; }
}
