package com.shadowcore.module;

import net.minecraft.client.MinecraftClient;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * Base for all legit utility modules.
 *
 * <p>Modules are <b>not</b> cheats — they only automate QoL or improve performance
 * in a way that is allowed on vanilla servers.</p>
 */
public abstract class Module {

    protected final String id;
    protected final String name;
    protected final String description;
    protected final ModuleCategory category;

    protected boolean enabled;
    protected boolean togglable = true;

    protected Module(String id, String name, String description, ModuleCategory category, boolean enabledByDefault) {
        this.id = id;
        this.name = name;
        this.description = description;
        this.category = category;
        this.enabled = enabledByDefault;
    }

    public String getId() { return id; }
    public String getName() { return name; }
    public String getDescription() { return description; }
    public ModuleCategory getCategory() { return category; }
    public boolean isEnabled() { return enabled; }
    public boolean isTogglable() { return togglable; }

    /** Enable / disable and notify. */
    public void setEnabled(boolean enabled) {
        if (this.enabled == enabled) return;
        this.enabled = enabled;
        if (enabled) onEnable();
        else onDisable();
    }

    public void toggle() {
        setEnabled(!enabled);
    }

    /** Called once when module is enabled. */
    protected void onEnable() {}

    /** Called once when module is disabled. */
    protected void onDisable() {}

    /** Called every client tick if enabled. */
    public void tick(MinecraftClient client) {}

    /** Optional HUD line. Return null to hide. */
    public Text getHudText() { return null; }

    protected Text statusText(String suffix, Formatting color) {
        return Text.literal(name + " " + suffix).formatted(color);
    }
}
