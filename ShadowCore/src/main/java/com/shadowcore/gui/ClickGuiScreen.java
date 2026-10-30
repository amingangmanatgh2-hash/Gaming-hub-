package com.shadowcore.gui;

import com.shadowcore.config.ConfigManager;
import com.shadowcore.gui.components.CategoryPanel;
import com.shadowcore.module.ModuleCategory;
import com.shadowcore.module.ModuleManager;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

import java.util.ArrayList;
import java.util.List;

/**
 * Clean ClickGUI — opened with Right Shift.
 *
 * <p>Design:</p>
 * <ul>
 *   <li>Dark, minimal panels per category — draggable</li>
 *   <li>Module rows toggle on left-click</li>
 *   <li>ESC to close; background blur if enabled</li>
 *   <li>Search / settings placeholders for extensibility</li>
 * </ul>
 */
public class ClickGuiScreen extends Screen {

    private final List<CategoryPanel> panels = new ArrayList<>();
    private CategoryPanel draggingPanel = null;

    public ClickGuiScreen() {
        super(Text.literal("ShadowCore"));
    }

    @Override
    protected void init() {
        panels.clear();
        int startX = 20;
        int startY = 30;
        int gap = GuiTheme.PANEL_WIDTH + 10;
        int idx = 0;
        for (ModuleCategory cat : ModuleCategory.values()) {
            var modules = ModuleManager.getInstance().byCategory(cat);
            if (modules.isEmpty()) continue; // skip empty category
            int px = startX + idx * gap;
            // Wrap to second row if needed
            if (px + GuiTheme.PANEL_WIDTH > width - 10) {
                px = startX + (idx % 3) * gap;
                startY = 160;
            }
            panels.add(new CategoryPanel(cat, px, startY, modules));
            idx++;
        }
    }

    @Override
    public void render(DrawContext ctx, int mouseX, int mouseY, float delta) {
        // Semi-transparent backdrop
        if (ConfigManager.get().clickGuiBlur) {
            ctx.fill(0, 0, width, height, 0xAA0A0A0C);
        } else {
            ctx.fill(0, 0, width, height, 0x88000000);
        }

        // Title bar
        String title = "ShadowCore 1.0  —  Legit Utility Edition  •  Right-Shift to open  •  ESC to close";
        ctx.drawCenteredTextWithShadow(textRenderer, Text.literal(title).formatted(Formatting.GRAY), width / 2, 10, 0xFFFFFF);

        // Subtitle about ethics
        String subtitle = "All features are server-safe — no cheats, no X-Ray, no exploits.";
        ctx.drawCenteredTextWithShadow(textRenderer, Text.literal(subtitle).formatted(Formatting.DARK_GREEN), width / 2, 20, 0x7ED957);

        // Panels
        for (CategoryPanel p : panels) p.render(ctx, mouseX, mouseY, delta);

        // Footer hints
        String hint = "Left-click = toggle  •  Right-click = expand  •  Drag header to move";
        ctx.drawTextWithShadow(textRenderer, hint, 6, height - 12, 0xFF888888);

        super.render(ctx, mouseX, mouseY, delta);
    }

    @Override
    public boolean mouseClicked(double mouseX, double mouseY, int button) {
        for (int i = panels.size() - 1; i >= 0; i--) {
            if (panels.get(i).mouseClicked(mouseX, mouseY, button)) {
                // Bring to front
                CategoryPanel p = panels.remove(i);
                panels.add(p);
                draggingPanel = p;
                return true;
            }
        }
        return super.mouseClicked(mouseX, mouseY, button);
    }

    @Override
    public boolean mouseDragged(double mouseX, double mouseY, int button, double deltaX, double deltaY) {
        if (draggingPanel != null && draggingPanel.mouseDragged(mouseX, mouseY, button, deltaX, deltaY)) return true;
        return super.mouseDragged(mouseX, mouseY, button, deltaX, deltaY);
    }

    @Override
    public boolean mouseReleased(double mouseX, double mouseY, int button) {
        if (draggingPanel != null) draggingPanel.mouseReleased(mouseX, mouseY, button);
        draggingPanel = null;
        for (CategoryPanel p : panels) p.mouseReleased(mouseX, mouseY, button);
        return super.mouseReleased(mouseX, mouseY, button);
    }

    @Override
    public boolean shouldPause() { return false; }

    @Override
    public boolean keyPressed(int keyCode, int scanCode, int modifiers) {
        if (keyCode == 256) { // ESC
            close();
            return true;
        }
        return super.keyPressed(keyCode, scanCode, modifiers);
    }
}
