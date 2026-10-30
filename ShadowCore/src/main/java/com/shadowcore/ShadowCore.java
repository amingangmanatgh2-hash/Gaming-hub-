package com.shadowcore;

import com.shadowcore.entity.VerityEntity;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.itemgroup.v1.ItemGroupEvents;
import net.fabricmc.fabric.api.object.builder.v1.entity.FabricEntityTypeBuilder;
import net.minecraft.entity.EntityDimensions;
import net.minecraft.entity.EntityType;
import net.minecraft.entity.SpawnGroup;
import net.minecraft.item.Item;
import net.minecraft.item.ItemGroups;
import net.minecraft.item.SpawnEggItem;
import net.minecraft.registry.Registries;
import net.minecraft.registry.Registry;
import net.minecraft.util.Identifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Common initializer — runs on both client and server.
 * Registers Verity entity + spawn egg (so companion works in singleplayer & modded servers).
 *
 * <p>Client-only features (HUD, ClickGUI, Zoom) are in {@link ShadowCoreClient}.</p>
 */
public class ShadowCore implements ModInitializer {

    public static final String MOD_ID = "shadowcore";

    // Entity type — registered here so server knows it
    public static EntityType<VerityEntity> VERITY_TYPE;
    public static Item VERITY_SPAWN_EGG;

    public static final Logger LOGGER = LoggerFactory.getLogger("ShadowCore");

    @Override
    public void onInitialize() {
        LOGGER.info("[ShadowCore] Common init — registering Verity companion");

        VERITY_TYPE = Registry.register(Registries.ENTITY_TYPE,
                Identifier.of(MOD_ID, "verity"),
                FabricEntityTypeBuilder.<VerityEntity>create(SpawnGroup.CREATURE, VerityEntity::new)
                        .dimensions(EntityDimensions.fixed(0.6f, 1.0f))
                        .trackRangeChunks(8)
                        .trackedUpdateRate(2)
                        .build()
        );

        VERITY_SPAWN_EGG = Registry.register(Registries.ITEM,
                Identifier.of(MOD_ID, "verity_spawn_egg"),
                new SpawnEggItem(VERITY_TYPE, 0x0A1F0A, 0x7ED957, new Item.Settings())
        );

        // Add to spawn egg creative tab
        ItemGroupEvents.modifyEntriesEvent(ItemGroups.SPAWN_EGGS).register(content -> content.add(VERITY_SPAWN_EGG));

        // Attributes are registered via Fabric's DefaultAttributeRegistry in newer MC — handled in VerityEntity
        VerityEntity.registerAttributes();
    }
}
