package com.shadowcore.module;

import java.awt.Color;

/**
 * Visual categories for ClickGUI panels.
 */
public enum ModuleCategory {
    PERFORMANCE("Performance", new Color(0x7ED957)),
    MOVEMENT("Movement", new Color(0x5DADE2)),
    UTILITY("Utility", new Color(0xF4D03F)),
    WORLD("World", new Color(0xAF7AC5)),
    HUD("HUD", new Color(0xF1948A));

    public final String displayName;
    public final Color color;

    ModuleCategory(String displayName, Color color) {
        this.displayName = displayName;
        this.color = color;
    }
}
