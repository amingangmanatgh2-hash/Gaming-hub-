package com.shadowcore.gui.components;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.gui.GuiTheme;
import com.shadowcore.module.Module;
import com.shadowcore.module.modules.InventorySorterModule;
import com.shadowcore.util.InventoryUtils;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * Row button for a single module — toggle on left-click, expanded controls on right-click.
 */
public class ModuleButton {

    private final Module module;
    private final CategoryPanel parent;
    private int x, y, w, h;
    private boolean expanded = false;

    public ModuleButton(Module module, CategoryPanel parent) {
        this.module = module;
        this.parent = parent;
    }

    public void setBounds(int x, int y, int w, int h) {
        this.x = x; this.y = y; this.w = w; this.h = h;
    }

    public void render(DrawContext ctx, int mouseX, int mouseY, float delta) {
        boolean hovered = mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
        int bg = module.isEnabled() ? GuiTheme.BG_MODULE_ON : GuiTheme.BG_MODULE_OFF;
        if (hovered) bg = GuiTheme.BG_HOVER;
        // Keep enabled green tint
        if (module.isEnabled() && !hovered) bg = GuiTheme.BG_MODULE_ON;

        ctx.fill(x, y, x + w, y + h, bg);

        var mc = MinecraftClient.getInstance();
        int textColor = module.isEnabled() ? 0xFF0A1F0A : GuiTheme.TEXT_PRIMARY;
        if (hovered && !module.isEnabled()) textColor = GuiTheme.TEXT_ACCENT;
        // Truncate name if too long
        String label = module.getName();
        ctx.drawTextWithShadow(mc.textRenderer, label, x + 6, y + 4, textColor);

        // Right indicator dot
        int dot = module.isEnabled() ? 0xFF2E7D32 : 0xFF555555;
        ctx.fill(x + w - 8, y + 5, x + w - 4, y + 9, dot);

        // Expanded extra row (e.g., sorter button)
        if (expanded && module.getId().equals("inventory_sorter")) {
            // Render extra action button below
            // For simplicity we just handle click; rendering is inline
        }
    }

    public boolean mouseClicked(double mx, double my, int button) {
        if (mx < x || mx > x + w || my < y || my > y + h) return false;
        if (button == 0) {
            // Special case: Inventory Sorter left-click performs sort
            if (module.getId().equals("inventory_sorter")) {
                // Toggle + sort
                module.toggle();
                if (module.isEnabled()) {
                    MinecraftClient client = MinecraftClient.getInstance();
                    InventorySorterModule.sortPlayerInventory(client);
                }
            } else {
                module.toggle();
                // Persist
                ConfigManager.save();
            }
            return true;
        } else if (button == 1) {
            expanded = !expanded;
            return true;
        }
        return false;
    }

    public void mouseReleased(double mx, double my, int button) {
        // no-op
    }
}
