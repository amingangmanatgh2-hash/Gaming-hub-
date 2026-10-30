package com.shadowcore.mixin;

import com.shadowcore.config.ConfigManager;
import net.minecraft.client.particle.ParticleManager;
import net.minecraft.client.particle.Particle;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Throttles particle spawning when FPS Optimizer's particle limiter is on.
 *
 * <p>Legit performance: prevents particle spam from lagging client. Does not
 * modify gameplay, only visual count.</p>
 */
@Mixin(ParticleManager.class)
public class ParticleMixin {

    // Simple per-tick counter
    private static int particlesThisTick = 0;
    private static long lastTick = -1;

    @Inject(method = "addParticle(Lnet/minecraft/client/particle/Particle;)V", at = @At("HEAD"), cancellable = true)
    private void shadowcore$limitParticles(Particle particle, CallbackInfo ci) {
        if (!ConfigManager.get().particleLimiter) return;
        if (!ConfigManager.get().fpsOptimizerEnabled) return;

        long now = System.nanoTime() / 50_000_000L; // ~tick
        if (now != lastTick) {
            lastTick = now;
            particlesThisTick = 0;
        }
        particlesThisTick++;
        if (particlesThisTick > ConfigManager.get().maxParticlesPerTick) {
            ci.cancel(); // drop excess particle
        }
    }
}
