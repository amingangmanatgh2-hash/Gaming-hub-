package com.shadowcore;

import com.shadowcore.command.ShadowCoreCommand;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.gui.ClickGuiScreen;
import com.shadowcore.hud.HudOverlay;
import com.shadowcore.minimap.MinimapRenderer;
import com.shadowcore.module.ModuleManager;
import com.shadowcore.waypoint.WaypointManager;
import com.shadowcore.waypoint.WaypointRenderer;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
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
 * ShadowCore — Client entrypoint.
 *
 * <p>This is a <b>legit-only</b> utility / performance mod. No cheats, no X-Ray, no KillAura,
 * no Fly exploits. Every feature is designed to be server-friendly and compatible with
 * anti-cheat, vanilla servers, and competitive play.</p>
 *
 * <p>Responsibilities:</p>
 * <ul>
 *   <li>Load / persist config</li>
 *   <li>Register keybinds (Toggle GUI, Zoom, Waypoints, etc.)</li>
 *   <li>Hook client tick for modules</li>
 *   <li>Register HUD + world render callbacks</li>
 * </ul>
 */
public class ShadowCoreClient implements ClientModInitializer {

    public static final String MOD_ID = "shadowcore";
    public static final Logger LOGGER = LoggerFactory.getLogger("ShadowCore");

    private static ShadowCoreClient instance;

    // ---- Keybinds -----------------------------------------------------------
    public static KeyBinding openClickGui;
    public static KeyBinding zoomKey;
    public static KeyBinding addWaypointKey;
    public static KeyBinding toggleMinimapKey;

    private ModuleManager moduleManager;
    private WaypointManager waypointManager;

    @Override
    public void onInitializeClient() {
        instance = this;
        LOGGER.info("[ShadowCore] Initializing legit utility edition v1.0.0");

        // 1) Config — load from config/shadowcore.json (creates defaults if missing)
        ConfigManager.load();

        // 2) Managers
        this.moduleManager = new ModuleManager();
        this.waypointManager = new WaypointManager();
        this.waypointManager.load();

        ModuleManager.getInstance().init();

        // 3) Keybinds
        registerKeybinds();

        // 4) Client tick — poll keybinds + tick modules
        ClientTickEvents.END_CLIENT_TICK.register(client -> onEndTick(client));

        // 5) HUD overlay (coords, FPS, biome, etc.)
        HudRenderCallback.EVENT.register((drawContext, tickDelta) -> HudOverlay.render(drawContext));

        // 6) World render — waypoints + minimap prep
        WorldRenderEvents.AFTER_TRANSLUCENT.register(context -> {
            WaypointRenderer.render(context);
            MinimapRenderer.onWorldRender(context);
        });

        // 7) Command: /shadowcore
        ShadowCoreCommand.register();

        LOGGER.info("[ShadowCore] Ready — press Right Shift for ClickGUI");
    }

    private void registerKeybinds() {
        // Right Shift — ClickGUI (industry standard for utility mods)
        openClickGui = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.shadowcore.clickgui",
                InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_RIGHT_SHIFT,
                "category.shadowcore.main"
        ));

        // C — Zoom (hold). Also configurable to V.
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

        // Open ClickGUI
        while (openClickGui.wasPressed()) {
            client.setScreen(new ClickGuiScreen());
        }

        // Quick waypoint
        while (addWaypointKey.wasPressed()) {
            if (client.player != null) {
                waypointManager.createAtPlayer("Waypoint-" + (waypointManager.size() + 1));
                if (client.player != null) {
                    client.player.sendMessage(
                            Text.literal("[ShadowCore] ").formatted(Formatting.DARK_GRAY)
                                    .append(Text.literal("Waypoint added").formatted(Formatting.GREEN)),
                            false);
                }
            }
        }

        // Toggle minimap visibility quickly
        while (toggleMinimapKey.wasPressed()) {
            boolean newVal = !ConfigManager.get().showMinimap;
            ConfigManager.get().showMinimap = newVal;
            ConfigManager.save();
            if (client.player != null) {
                client.player.sendMessage(
                        Text.literal("[ShadowCore] Minimap " + (newVal ? "ON" : "OFF")).formatted(Formatting.GRAY),
                        true);
            }
        }

        // Tick modules (BetterSprint, AutoTool hook, Fullbright gamma, Zoom)
        moduleManager.tick(client);
    }

    // ---- Singleton ----------------------------------------------------------

    public static ShadowCoreClient getInstance() {
        return instance;
    }

    public ModuleManager getModuleManager() {
        return moduleManager;
    }

    public WaypointManager getWaypointManager() {
        return waypointManager;
    }
}
