package com.shadowcore.mixin;

import com.shadowcore.config.ConfigManager;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.render.Frustum;
import net.minecraft.client.render.entity.EntityRenderer;
import net.minecraft.entity.Entity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Entity culling — skips rendering entities beyond configured distance.
 *
 * <p>Massive FPS win on servers with many entities / armor stands. Fully legit,
 * only affects client rendering.</p>
 */
@Mixin(EntityRenderer.class)
public class EntityCullingMixin {

    @Inject(method = "shouldRender", at = @At("HEAD"), cancellable = true)
    private void shadowcore$cullDistant(Entity entity, Frustum frustum, double x, double y, double z, CallbackInfoReturnable<Boolean> cir) {
        if (!ConfigManager.get().fpsOptimizerEnabled) return;
        if (!ConfigManager.get().entityCulling) return;

        MinecraftClient client = MinecraftClient.getInstance();
        if (client.player == null) return;
        // Never cull the player itself or vehicle
        if (entity == client.player || entity == client.player.getVehicle()) return;

        double dist = client.player.squaredDistanceTo(entity);
        double max = ConfigManager.get().entityCullingDistance;
        if (dist > max * max) {
            // Still respect frustum is cheap; we cancel only if truly far
            cir.setReturnValue(false);
        }
    }
}
