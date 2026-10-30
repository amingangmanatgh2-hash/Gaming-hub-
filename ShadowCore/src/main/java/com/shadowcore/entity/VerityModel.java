package com.shadowcore.entity;

import net.minecraft.client.model.*;
import net.minecraft.client.render.entity.model.EntityModel;
import net.minecraft.client.render.entity.state.LivingEntityRenderState;

/**
 * Placeholder for custom Verity model — currently VerityRenderer uses Wolf model for simplicity.
 * This class is a stub if you want to design a bespoke "cute helper" model via Blockbench.
 * Export from Blockbench as Modded Entity > Fabric, paste here, and wire in VerityRenderer.
 *
 * Steps:
 * 1. Blockbench > New > Modded Entity
 * 2. Design 16x16 cute companion (big head, small body, cyan eyes)
 * 3. File > Export > Java Code > Fabric
 * 4. Replace contents of this file
 */
public class VerityModel extends EntityModel<LivingEntityRenderState> {

    public VerityModel(ModelPart root) {
        super(root);
    }

    public static TexturedModelData getTexturedModelData() {
        ModelData data = new ModelData();
        ModelPartData root = data.getRoot();
        // Minimal placeholder — single cube for head
        root.addChild("head", ModelPartBuilder.create().uv(0, 0).cuboid(-4, -8, -4, 8, 8, 8), ModelTransform.pivot(0, 0, 0));
        root.addChild("body", ModelPartBuilder.create().uv(16, 16).cuboid(-3, 0, -2, 6, 8, 4), ModelTransform.pivot(0, 0, 0));
        return TexturedModelData.of(data, 32, 32);
    }

    @Override
    public void setAngles(LivingEntityRenderState state) {
        super.setAngles(state);
    }
}
