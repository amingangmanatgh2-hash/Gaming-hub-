package com.shadowcore.entity;

import net.minecraft.client.render.VertexConsumerProvider;
import net.minecraft.client.render.entity.EntityRendererFactory;
import net.minecraft.client.render.entity.MobEntityRenderer;
import net.minecraft.client.render.entity.model.EntityModelLayers;
import net.minecraft.client.render.entity.model.WolfEntityModel;
import net.minecraft.client.util.math.MatrixStack;
import net.minecraft.util.Identifier;

/**
 * Simple renderer for Verity — reuses Wolf model with custom cyan-tinted texture.
 * For full custom model, replace with GeckoLib or custom EntityModel — kept lightweight here.
 */
public class VerityRenderer extends MobEntityRenderer<VerityEntity, WolfEntityModel<VerityEntity>> {

    private static final Identifier TEXTURE = Identifier.of("shadowcore", "textures/entity/verity.png");
    private static final Identifier TEXTURE_ANGRY = Identifier.of("shadowcore", "textures/entity/verity_angry.png");
    // Fallback to vanilla wolf texture if ours missing — no crash
    private static final Identifier FALLBACK = Identifier.ofVanilla("textures/entity/wolf/wolf.png");

    public VerityRenderer(EntityRendererFactory.Context context) {
        super(context, new WolfEntityModel<>(context.getPart(EntityModelLayers.WOLF)), 0.5f);
    }

    @Override
    public Identifier getTexture(VerityEntity entity) {
        try {
            var client = net.minecraft.client.MinecraftClient.getInstance();
            if (client.getResourceManager().getResource(TEXTURE).isPresent()) {
                boolean angry = entity.getTarget() != null;
                return angry ? TEXTURE_ANGRY : TEXTURE;
            }
        } catch (Exception ignored) {}
        return FALLBACK;
    }

    @Override
    public void render(VerityEntity entity, float yaw, float tickDelta, MatrixStack matrices, VertexConsumerProvider vertexConsumers, int light) {
        // Slight scale up when giving second chance (pulse)
        if (entity.getWorld().getTime() % 20 < 2 && entity.getOwner() != null) {
            matrices.push();
            matrices.scale(1.05f, 1.05f, 1.05f);
            super.render(entity, yaw, tickDelta, matrices, vertexConsumers, light);
            matrices.pop();
        } else {
            super.render(entity, yaw, tickDelta, matrices, vertexConsumers, light);
        }
    }

    @Override
    protected float getShadowRadius(VerityEntity entity) {
        return 0.4f;
    }
}
