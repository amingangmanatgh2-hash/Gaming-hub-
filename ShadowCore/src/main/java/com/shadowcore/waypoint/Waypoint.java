package com.shadowcore.waypoint;

import com.shadowcore.config.ShadowCoreConfig.WaypointEntry;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Vec3d;

/**
 * Runtime waypoint — wraps a serializable {@link WaypointEntry}.
 */
public class Waypoint {

    private final WaypointEntry entry;

    public Waypoint(WaypointEntry entry) {
        this.entry = entry;
    }

    public WaypointEntry getEntry() { return entry; }

    public String getName() { return entry.name; }
    public String getDimension() { return entry.dimension; }
    public BlockPos getPos() { return new BlockPos(entry.x, entry.y, entry.z); }
    public Vec3d getVec() { return new Vec3d(entry.x + 0.5, entry.y + 0.5, entry.z + 0.5); }
    public int getColor() { return entry.color; }
    public boolean isVisible() { return entry.visible; }

    public void setVisible(boolean visible) { entry.visible = visible; }

    public double distanceTo(Vec3d from) {
        return from.distanceTo(getVec());
    }
}
