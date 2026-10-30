package com.shadowcore.waypoint;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig.WaypointEntry;
import net.minecraft.client.MinecraftClient;
import net.minecraft.util.math.BlockPos;
import org.jetbrains.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Manages waypoints — CRUD + persistence via {@link com.shadowcore.config.ShadowCoreConfig#waypoints}.
 */
public class WaypointManager {

    private final List<Waypoint> waypoints = new ArrayList<>();

    public void load() {
        waypoints.clear();
        for (WaypointEntry e : ConfigManager.get().waypoints) {
            waypoints.add(new Waypoint(e));
        }
    }

    public void save() {
        List<WaypointEntry> entries = new ArrayList<>();
        for (Waypoint w : waypoints) entries.add(w.getEntry());
        ConfigManager.get().waypoints = entries;
        ConfigManager.save();
    }

    public List<Waypoint> getAll() {
        return Collections.unmodifiableList(waypoints);
    }

    public int size() { return waypoints.size(); }

    public void add(Waypoint waypoint) {
        waypoints.add(waypoint);
        save();
    }

    public void remove(Waypoint waypoint) {
        waypoints.remove(waypoint);
        save();
    }

    public void clear() {
        waypoints.clear();
        save();
    }

    /** Create waypoint at player's current position. */
    public Waypoint createAtPlayer(String name) {
        MinecraftClient client = MinecraftClient.getInstance();
        if (client.player == null || client.world == null) return null;
        BlockPos pos = client.player.getBlockPos();
        String dim = client.world.getRegistryKey().getValue().toString();
        // Cycle color for variety
        int color = 0xFF00FF00 | ((waypoints.size() * 0x331111) & 0xFFFFFF);
        WaypointEntry entry = new WaypointEntry(name, dim, pos.getX(), pos.getY(), pos.getZ(), color);
        Waypoint wp = new Waypoint(entry);
        add(wp);
        return wp;
    }

    /** Create waypoint at arbitrary position. */
    public Waypoint createAt(String name, String dimension, int x, int y, int z, int color) {
        WaypointEntry entry = new WaypointEntry(name, dimension, x, y, z, color);
        Waypoint wp = new Waypoint(entry);
        add(wp);
        return wp;
    }

    @Nullable
    public Waypoint findByName(String name) {
        return waypoints.stream().filter(w -> w.getName().equalsIgnoreCase(name)).findFirst().orElse(null);
    }
}
