package com.shadowcore.config;

import java.util.ArrayList;
import java.util.List;

/**
 * Persistent config — serialized as JSON to {@code config/shadowcore.json}.
 *
 * <p>All defaults are legit-friendly: no cheats, no exploits, only QoL + perf.</p>
 */
public class ShadowCoreConfig {

    // --- General -------------------------------------------------------------
    public boolean clickGuiBlur = true;
    public float clickGuiAnimationSpeed = 1.0f;

    // --- FPS Optimizer -------------------------------------------------------
    public boolean fpsOptimizerEnabled = true;
    public boolean entityCulling = true;
    public int entityCullingDistance = 64; // blocks — hide distant entities outside this radius
    public boolean particleLimiter = true;
    public int maxParticlesPerTick = 64;
    public boolean smartRenderDistance = false;
    public int targetFps = 60;
    public int minRenderDistance = 4;
    public int maxRenderDistance = 12;

    // --- Fullbright (legit gamma) --------------------------------------------
    /** -1 = use vanilla; 12.0 = max legit brightness (server-safe, no XRay). */
    public boolean fullbrightEnabled = false;
    public double fullbrightGamma = 12.0;
    public double previousGamma = 1.0;
    public boolean fullbrightRememberGamma = true;

    // --- Better Sprint -------------------------------------------------------
    public boolean betterSprintEnabled = true;
    public boolean sprintLock = true; // double-tap W locks sprint
    public boolean flyBoostBypass = false; // kept false — placeholder to show we DON'T implement fly hack

    // --- AutoTool ------------------------------------------------------------
    public boolean autoToolEnabled = true;
    public boolean autoToolPreferSilkTouch = false;
    public boolean autoToolDurabilityCheck = true;
    public int autoToolMinDurability = 10; // don't select tool below this durability

    // --- Inventory Sorter ----------------------------------------------------
    public boolean inventorySorterEnabled = true;
    public String sortMode = "TYPE"; // TYPE | NAME | QUANTITY

    // --- Waypoints -----------------------------------------------------------
    public boolean waypointsEnabled = true;
    public boolean waypointTracer = true;
    public boolean waypointShowDistance = true;

    // --- Minimap -------------------------------------------------------------
    public boolean showMinimap = true;
    public int minimapSize = 96; // px
    public int minimapScale = 1; // block per pixel
    public boolean minimapShowEntities = false; // off by default — legit only
    public boolean minimapRound = true;

    // --- Zoom ----------------------------------------------------------------
    public boolean zoomEnabled = true;
    public double zoomFovMultiplier = 0.25; // 4x zoom
    public boolean zoomSmooth = true;
    public double zoomSmoothSpeed = 0.6;

    // --- HUD -----------------------------------------------------------------
    public boolean hudEnabled = true;
    public boolean hudShowCoords = true;
    public boolean hudShowFps = true;
    public boolean hudShowBiome = true;
    public boolean hudShowTime = true;
    public boolean hudShowDirection = true;

    // --- Waypoint storage (inline) -------------------------------------------
    public List<WaypointEntry> waypoints = new ArrayList<>();

    /**
     * Minimal serializable waypoint entry.
     */
    public static class WaypointEntry {
        public String name = "Waypoint";
        public String dimension = "minecraft:overworld";
        public int x, y, z;
        public int color = 0xFF55FFFF; // ARGB — cyan default
        public boolean visible = true;

        public WaypointEntry() {}

        public WaypointEntry(String name, String dimension, int x, int y, int z, int color) {
            this.name = name;
            this.dimension = dimension;
            this.x = x; this.y = y; this.z = z;
            this.color = color;
        }
    }
}
