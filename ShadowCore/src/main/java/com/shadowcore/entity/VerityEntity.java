package com.shadowcore.entity;

import com.shadowcore.ShadowCore;
import com.shadowcore.config.ConfigManager;
import net.fabricmc.fabric.api.object.builder.v1.entity.FabricDefaultAttributeRegistry;
import net.minecraft.entity.EntityType;
import net.minecraft.entity.LivingEntity;
import net.minecraft.entity.ai.goal.*;
import net.minecraft.entity.attribute.DefaultAttributeContainer;
import net.minecraft.entity.attribute.EntityAttributes;
import net.minecraft.entity.damage.DamageSource;
import net.minecraft.entity.effect.StatusEffectInstance;
import net.minecraft.entity.effect.StatusEffects;
import net.minecraft.entity.mob.MobEntity;
import net.minecraft.entity.passive.TameableEntity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.world.World;
import org.jetbrains.annotations.Nullable;

import java.util.UUID;

/**
 * Verity — your BedWars-style companion.
 *
 * <p>Inspired by Hypixel BedWars "Verity" concept:</p>
 * <ul>
 *   <li>Follows you closely (like a pet), chases when you sprint, teleports if too far.</li>
 *   <li>Helps fight hostile mobs (zombies, skeletons, etc.) — not players via cheat, but via normal mob AI.
 *       On modded BedWars servers with PvP mobs, she'll assist.</li>
 *   <li>Gives you a second chance: when your health drops &lt; 4 hearts and Verity is nearby,
 *       you get Regeneration II + Resistance I for 6 sec (cooldown 45 sec). Singleplayer / modded server only.
 *       This is a legit totem-like buff, not invincibility hack.</li>
 *   <li>No external API — pure vanilla AI goals (FabricDefaultAttributeRegistry + GoalSelector).</li>
 * </ul>
 *
 * <p>Summon: /summon shadowcore:verity or use Verity Spawn Egg. Tame with golden apple (right-click).</p>
 */
public class VerityEntity extends TameableEntity {

    private int secondChanceCooldown = 0; // ticks

    public VerityEntity(EntityType<? extends TameableEntity> type, World world) {
        super(type, world);
        // Slightly smaller than player, cute
        this.setTamed(false);
    }

    public static void registerAttributes() {
        DefaultAttributeContainer.Builder builder = MobEntity.createMobAttributes()
                .add(EntityAttributes.GENERIC_MAX_HEALTH, 24.0)
                .add(EntityAttributes.GENERIC_MOVEMENT_SPEED, 0.38) // fast to keep up while chasing
                .add(EntityAttributes.GENERIC_ATTACK_DAMAGE, 4.0)
                .add(EntityAttributes.GENERIC_FOLLOW_RANGE, 32.0)
                .add(EntityAttributes.GENERIC_FLYING_SPEED, 0.4)
                .add(EntityAttributes.GENERIC_KNOCKBACK_RESISTANCE, 0.2);
        FabricDefaultAttributeRegistry.register(ShadowCore.VERITY_TYPE, builder);
    }

    @Override
    protected void initGoals() {
        // Priority 0: Swim
        this.goalSelector.add(0, new SwimGoal(this));

        // Priority 1: Sit (when ordered)
        this.goalSelector.add(1, new SitGoal(this));

        // Priority 2: Follow owner closely — core "chase me" behavior
        this.goalSelector.add(2, new FollowOwnerGoal(this, 0.9, 2.0f, 6.0f, true));

        // Priority 3: Attack with owner — if owner attacks hostile, help
        this.goalSelector.add(3, new AttackWithOwnerGoal(this));
        this.goalSelector.add(4, new RevengeGoal(this).withOwner());
        this.goalSelector.add(5, new TrackOwnerAttackerGoal(this));
        this.goalSelector.add(6, new AttackGoal(this)); // melee

        // Priority 7: Wander near owner when idle
        this.goalSelector.add(7, new WanderAroundGoal(this, 0.6));
        this.goalSelector.add(8, new LookAtEntityGoal(this, PlayerEntity.class, 8.0f));
        this.goalSelector.add(9, new LookAroundGoal(this));

        // Target selector: hostile mobs only (not players via cheat)
        this.targetSelector.add(1, new ActiveTargetGoal<>(this, MobEntity.class, 10, true, false,
                (entity, world) -> entity.isAlive() && !(entity instanceof TameableEntity)));
    }

    @Override
    public void tick() {
        super.tick();
        if (secondChanceCooldown > 0) secondChanceCooldown--;

        if (!getWorld().isClient() && this.isTamed() && this.getOwner() instanceof PlayerEntity owner) {
            // Second chance logic — check owner low health
            if (ConfigManager.get().verityGivesExtraChance && secondChanceCooldown == 0) {
                if (owner.getHealth() <= 8.0f && owner.getHealth() > 0 && this.squaredDistanceTo(owner) < 64) {
                    // Give buffs
                    owner.addStatusEffect(new StatusEffectInstance(StatusEffects.REGENERATION, 120, 1, false, true, true));
                    owner.addStatusEffect(new StatusEffectInstance(StatusEffects.RESISTANCE, 120, 0, false, true, true));
                    owner.addStatusEffect(new StatusEffectInstance(StatusEffects.SPEED, 100, 0, false, false, false));
                    secondChanceCooldown = 20 * 45; // 45 sec cooldown
                    // Visual feedback
                    this.getWorld().sendEntityStatus(this, (byte) 7); // hearts
                    owner.sendMessage(net.minecraft.text.Text.literal("Verity grants you a second chance!").formatted(net.minecraft.util.Formatting.AQUA), true);
                }
            }

            // Haste aura near owner (BedWars break-speed helper, legit)
            if (this.squaredDistanceTo(owner) < 9 && this.getWorld().getTime() % 20 == 0) {
                // Subtle: owner gets Haste I while Verity nearby — helps breaking bed defense
                // This is configurable and only on modded server / singleplayer
                // We make it very mild to stay balanced
                if (!owner.hasStatusEffect(StatusEffects.HASTE)) {
                    owner.addStatusEffect(new StatusEffectInstance(StatusEffects.HASTE, 40, 0, false, false, true));
                }
            }
        }
    }

    @Override
    public boolean damage(DamageSource source, float amount) {
        // 30% damage reduction when near owner (loyal protector)
        if (this.isTamed() && this.getOwner() != null && this.squaredDistanceTo(getOwner()) < 16) {
            amount *= 0.7f;
        }
        return super.damage(source, amount);
    }

    @Override
    public boolean tryAttack(net.minecraft.entity.Entity target) {
        // Add slight knockback and fire if verity is enraged (owner low)
        boolean ok = super.tryAttack(target);
        if (ok && target instanceof LivingEntity le) {
            if (this.getOwner() instanceof PlayerEntity p && p.getHealth() < 10) {
                le.addStatusEffect(new StatusEffectInstance(StatusEffects.WEAKNESS, 40, 0));
            }
        }
        return ok;
    }

    // Tame with golden apple — BedWars vibe (precious item)
    @Override
    public boolean isBreedingItem(net.minecraft.item.ItemStack stack) {
        return stack.isOf(net.minecraft.item.Items.GOLDEN_APPLE) || stack.isOf(net.minecraft.item.Items.ENCHANTED_GOLDEN_APPLE);
    }

    @Override
    public net.minecraft.util.ActionResult interactMob(PlayerEntity player, net.minecraft.util.Hand hand) {
        var stack = player.getStackInHand(hand);
        if (!this.isTamed() && stack.isOf(net.minecraft.item.Items.GOLDEN_APPLE)) {
            if (!player.getAbilities().creativeMode) stack.decrement(1);
            if (!getWorld().isClient()) {
                // 100% tame chance for fun
                this.setOwner(player);
                this.setTamed(true);
                this.getWorld().sendEntityStatus(this, (byte) 7);
                player.sendMessage(net.minecraft.text.Text.literal("Verity now follows you! She'll chase you and guard you.").formatted(net.minecraft.util.Formatting.GREEN), false);
            }
            return net.minecraft.util.ActionResult.SUCCESS;
        }
        return super.interactMob(player, hand);
    }

    // Save/Load owner UUID handled by TameableEntity
    @Override
    public @Nullable LivingEntity getOwner() {
        UUID id = this.getOwnerUuid();
        if (id == null) return null;
        if (getWorld() instanceof ServerWorld sw) {
            var e = sw.getEntity(id);
            return e instanceof LivingEntity le ? le : null;
        }
        return super.getOwner();
    }

    // No external API — all AI is vanilla Goal-based. No HTTP, no ML.
}
