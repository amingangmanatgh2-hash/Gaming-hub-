package com.shadowcore.gui;

import java.awt.Color;

/**
 * ShadowCore monochrome + accent theme.
 */
public final class GuiTheme {

    private GuiTheme() {}

    // Backgrounds
    public static final int BG_MAIN = 0xEE121214;       // main panel
    public static final int BG_PANEL = 0xFF1E1E22;       // category panel
    public static final int BG_MODULE_ON = 0xFF7ED957;  // enabled
    public static final int BG_MODULE_OFF = 0xFF2D2D30;
    public static final int BG_HOVER = 0xFF3A3A3E;

    // Text
    public static final int TEXT_PRIMARY = 0xFFFFFFFF;
    public static final int TEXT_SECONDARY = 0xFFAAAAAA;
    public static final int TEXT_ACCENT = 0xFF7ED957;

    // Accent
    public static final Color ACCENT = new Color(0x7ED957);
    public static final int BORDER = 0xFF2A2A2E;
    public static final int SHADOW = 0x88000000;

    // Dimensions
    public static final int PANEL_WIDTH = 110;
    public static final int PANEL_HEADER_H = 18;
    public static final int MODULE_H = 16;
}
