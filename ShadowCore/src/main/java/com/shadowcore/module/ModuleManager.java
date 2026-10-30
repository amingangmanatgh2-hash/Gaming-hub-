package com.shadowcore.module;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.config.ShadowCoreConfig;
import com.shadowcore.module.modules.*;
import net.minecraft.client.MinecraftClient;

import java.util.*;

/**
 * Central registry for all modules. Handles persistence to config.
 */
public class ModuleManager {

    private static ModuleManager INSTANCE;

    private final Map<String, Module> byId = new LinkedHashMap<>();
    private final List<Module> modules = new ArrayList<>();

    public ModuleManager() {
        INSTANCE = this;
    }

    public static ModuleManager getInstance() {
        return INSTANCE;
    }

    /** Instantiate modules and wire their enabled state from config. */
    public void init() {
        ShadowCoreConfig cfg = ConfigManager.get();

        register(new FpsOptimizerModule(cfg.fpsOptimizerEnabled));
        register(new BetterSprintModule(cfg.betterSprintEnabled));
        register(new AutoToolModule(cfg.autoToolEnabled));
        register(new InventorySorterModule(cfg.inventorySorterEnabled));
        register(new FullbrightModule(cfg.fullbrightEnabled));
        register(new ZoomModule(cfg.zoomEnabled));
        register(new HudInfoModule(cfg.hudEnabled));

        // Waypoints / Minimap are handled outside modules (they have dedicated managers)
    }

    private void register(Module m) {
        modules.add(m);
        byId.put(m.getId(), m);
    }

    public List<Module> getAll() {
        return Collections.unmodifiableList(modules);
    }

    public List<Module> byCategory(ModuleCategory cat) {
        return modules.stream().filter(m -> m.getCategory() == cat).toList();
    }

    public Optional<Module> get(String id) {
        return Optional.ofNullable(byId.get(id));
    }

    /** Called every client tick. */
    public void tick(MinecraftClient client) {
        for (Module m : modules) {
            if (m.isEnabled()) m.tick(client);
        }
        syncToConfig();
    }

    /** Push enabled states back to config (cheap, only writes if changed). */
    private void syncToConfig() {
        ShadowCoreConfig c = ConfigManager.get();
        boolean dirty = false;

        // Map each module id → config field
        for (Module m : modules) {
            switch (m.getId()) {
                case "fps_optimizer" -> { if (c.fpsOptimizerEnabled != m.isEnabled()) { c.fpsOptimizerEnabled = m.isEnabled(); dirty = true; } }
                case "better_sprint" -> { if (c.betterSprintEnabled != m.isEnabled()) { c.betterSprintEnabled = m.isEnabled(); dirty = true; } }
                case "auto_tool" -> { if (c.autoToolEnabled != m.isEnabled()) { c.autoToolEnabled = m.isEnabled(); dirty = true; } }
                case "inventory_sorter" -> { if (c.inventorySorterEnabled != m.isEnabled()) { c.inventorySorterEnabled = m.isEnabled(); dirty = true; } }
                case "fullbright" -> { if (c.fullbrightEnabled != m.isEnabled()) { c.fullbrightEnabled = m.isEnabled(); dirty = true; } }
                case "zoom" -> { if (c.zoomEnabled != m.isEnabled()) { c.zoomEnabled = m.isEnabled(); dirty = true; } }
                case "hud_info" -> { if (c.hudEnabled != m.isEnabled()) { c.hudEnabled = m.isEnabled(); dirty = true; } }
            }
        }
        if (dirty) ConfigManager.save();
    }
}
