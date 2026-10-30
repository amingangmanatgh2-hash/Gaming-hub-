package com.shadowcore.module.modules;

import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleCategory;
import com.shadowcore.util.InventoryUtils;
import net.minecraft.client.MinecraftClient;
import net.minecraft.screen.slot.SlotActionType;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * Inventory Sorter — one-click sort for player inventory & chests.
 *
 * <p>Triggered by middle-click on inventory background or bound key (default: none,
 * use ClickGUI button). Sorting is <b>client-initiated inventory clicks</b> — no
 * server packet abuse, fully vanilla-friendly.</p>
 */
public class InventorySorterModule extends Module {

    public InventorySorterModule(boolean enabled) {
        super("inventory_sorter", "Inventory Sorter", "Sorts inventory by type/name/quantity.", ModuleCategory.UTILITY, enabled);
        // Sorter is passive — actual sorting is triggered via GUI or keybind.
        this.togglable = true;
    }

    /**
     * Sort the player's main inventory (slots 9-35). Called from ClickGUI button.
     */
    public static void sortPlayerInventory(MinecraftClient client) {
        if (client.player == null || client.interactionManager == null) return;
        InventoryUtils.sortInventory(client);
        if (client.player != null) {
            client.player.sendMessage(
                    Text.literal("[ShadowCore] Inventory sorted").formatted(Formatting.GRAY), true);
        }
    }

    @Override
    public Text getHudText() {
        return null; // no persistent HUD for sorter
    }
}
