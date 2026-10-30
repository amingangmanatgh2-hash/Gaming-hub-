package com.shadowcore.gui.components;

import com.shadowcore.gui.GuiTheme;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import net.minecraft.client.gui.DrawContext;

import java.util.ArrayList;
import java.util.List;

/**
 * Vertical panel for one {@link ModuleCategory}. Draggable, collapsible-ready.
 */
public class CategoryPanel {

    public final ModuleCategory category;
    public int x, y;
    public int width = GuiTheme.PANEL_WIDTH;
    private final List<ModuleButton> buttons = new ArrayList<>();
    private boolean dragging = false;
    private int dragOffX, dragOffY;

    public CategoryPanel(ModuleCategory category, int x, int y, List<Module> modules) {
        this.category = category;
        this.x = x; this.y = y;
        for (Module m : modules) buttons.add(new ModuleButton(m, this));
    }

    public int getHeight() {
        return GuiTheme.PANEL_HEADER_H + buttons.size() * GuiTheme.MODULE_H + 4;
    }

    public void render(DrawContext ctx, int mouseX, int mouseY, float delta) {
        // Shadow
        ctx.fill(x + 2, y + 2, x + width + 2, y + getHeight() + 2, GuiTheme.SHADOW);
        // Panel bg
        ctx.fill(x, y, x + width, y + getHeight(), GuiTheme.BG_PANEL);
        // Header
        int headerColor = GuiTheme.BORDER;
        // Slight accent tint per category
        ctx.fill(x, y, x + width, y + GuiTheme.PANEL_HEADER_H, headerColor);
        // Left accent stripe
        int accent = category.color.getRGB() | 0xFF000000;
        ctx.fill(x, y, x + 3, y + GuiTheme.PANEL_HEADER_H, accent);

        var mc = net.minecraft.client.MinecraftClient.getInstance();
        ctx.drawTextWithShadow(mc.textRenderer, category.displayName.toUpperCase(), x + 8, y + 5, GuiTheme.TEXT_PRIMARY);

        // Buttons
        int by = y + GuiTheme.PANEL_HEADER_H + 2;
        for (ModuleButton b : buttons) {
            b.setBounds(x + 2, by, width - 4, GuiTheme.MODULE_H - 2);
            b.render(ctx, mouseX, mouseY, delta);
            by += GuiTheme.MODULE_H;
        }
    }

    public boolean mouseClicked(double mx, double my, int button) {
        // Header drag
        if (mx >= x && mx <= x + width && my >= y && my <= y + GuiTheme.PANEL_HEADER_H) {
            if (button == 0) {
                dragging = true;
                dragOffX = (int) (mx - x);
                dragOffY = (int) (my - y);
                return true;
            }
        }
        for (ModuleButton b : buttons) if (b.mouseClicked(mx, my, button)) return true;
        return false;
    }

    public boolean mouseDragged(double mx, double my, int button, double dx, double dy) {
        if (dragging) {
            x = (int) (mx - dragOffX);
            y = (int) (my - dragOffY);
            return true;
        }
        return false;
    }

    public void mouseReleased(double mx, double my, int button) {
        dragging = false;
        for (ModuleButton b : buttons) b.mouseReleased(mx, my, button);
    }

    public boolean isDragging() { return dragging; }
}
