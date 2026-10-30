package com.shadowcore.command;

import com.mojang.brigadier.arguments.StringArgumentType;
import com.shadowcore.ShadowCore;
import com.shadowcore.ShadowCoreClient;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.gui.ClickGuiScreen;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleManager;
import com.shadowcore.waypoint.Waypoint;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandManager;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandRegistrationCallback;
import net.minecraft.client.MinecraftClient;
import net.minecraft.entity.SpawnReason;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.text.ClickEvent;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.minecraft.util.math.BlockPos;

/**
 * Client command: /shadowcore — v1.1 with Verity + FPS + Shader helpers
 */
public final class ShadowCoreCommand {

    private ShadowCoreCommand() {}

    public static void register() {
        ClientCommandRegistrationCallback.EVENT.register((dispatcher, registryAccess) -> {
            dispatcher.register(ClientCommandManager.literal("shadowcore")
                    .executes(ctx -> {
                        ctx.getSource().sendFeedback(Text.literal("ShadowCore v1.1 — type /shadowcore help").formatted(Formatting.AQUA));
                        return 1;
                    })
                    .then(ClientCommandManager.literal("help").executes(ctx -> {
                        var src = ctx.getSource();
                        src.sendFeedback(Text.literal("— ShadowCore 1.1 Help —").formatted(Formatting.AQUA));
                        src.sendFeedback(Text.literal("/shadowcore gui — Open ClickGUI (Right Shift)")));
                        src.sendFeedback(Text.literal("/shadowcore toggle <module> — Toggle module")));
                        src.sendFeedback(Text.literal("/shadowcore waypoint add <name> / list / clear")));
                        src.sendFeedback(Text.literal("/shadowcore zoom — Toggle zoom (hold C + scroll)")));
                        src.sendFeedback(Text.literal("/shadowcore fps — Show FPS boost status + Sodium tip")));
                        src.sendFeedback(Text.literal("/shadowcore verity — Summon Verity companion")));
                        src.sendFeedback(Text.literal("/shadowcore shader — Shader install help")));
                        return 1;
                    }))
                    .then(ClientCommandManager.literal("gui").executes(ctx -> {
                        MinecraftClient client = MinecraftClient.getInstance();
                        client.send(() -> client.setScreen(new ClickGuiScreen()));
                        return 1;
                    }))
                    .then(ClientCommandManager.literal("toggle")
                            .then(ClientCommandManager.argument("module", StringArgumentType.word())
                                    .suggests((c, b) -> {
                                        for (Module m : ModuleManager.getInstance().getAll()) b.suggest(m.getId());
                                        return b.buildFuture();
                                    })
                                    .executes(ctx -> {
                                        String id = StringArgumentType.getString(ctx, "module");
                                        var opt = ModuleManager.getInstance().get(id);
                                        if (opt.isEmpty()) {
                                            ctx.getSource().sendError(Text.literal("Unknown module: " + id));
                                            return 0;
                                        }
                                        Module m = opt.get();
                                        m.toggle();
                                        ctx.getSource().sendFeedback(Text.literal(m.getName() + " " + (m.isEnabled() ? "ON" : "OFF"))
                                                .formatted(m.isEnabled() ? Formatting.GREEN : Formatting.RED));
                                        return 1;
                                    })))
                    .then(ClientCommandManager.literal("waypoint")
                            .then(ClientCommandManager.literal("add")
                                    .then(ClientCommandManager.argument("name", StringArgumentType.greedyString())
                                            .executes(ctx -> {
                                                String name = StringArgumentType.getString(ctx, "name");
                                                var wp = ShadowCoreClient.getInstance().getWaypointManager().createAtPlayer(name);
                                                ctx.getSource().sendFeedback(Text.literal("Waypoint added: " + wp.getName()).formatted(Formatting.GREEN));
                                                return 1;
                                            })))
                            .then(ClientCommandManager.literal("list").executes(ctx -> {
                                var mgr = ShadowCoreClient.getInstance().getWaypointManager();
                                if (mgr.size() == 0) {
                                    ctx.getSource().sendFeedback(Text.literal("No waypoints").formatted(Formatting.GRAY));
                                    return 1;
                                }
                                for (Waypoint w : mgr.getAll()) {
                                    var p = w.getPos();
                                    ctx.getSource().sendFeedback(Text.literal("- " + w.getName() + " @ " + p.getX() + ", " + p.getY() + ", " + p.getZ() + " [" + w.getDimension() + "]"));
                                }
                                return 1;
                            }))
                            .then(ClientCommandManager.literal("clear").executes(ctx -> {
                                ShadowCoreClient.getInstance().getWaypointManager().clear();
                                ctx.getSource().sendFeedback(Text.literal("All waypoints cleared").formatted(Formatting.YELLOW));
                                return 1;
                            })))
                    .then(ClientCommandManager.literal("zoom").executes(ctx -> {
                        var mod = ModuleManager.getInstance().get("zoom");
                        mod.ifPresent(Module::toggle);
                        boolean on = mod.map(Module::isEnabled).orElse(false);
                        ctx.getSource().sendFeedback(Text.literal("Zoom " + (on ? "ON (hold C or MiddleMouse + scroll to adjust)" : "OFF")).formatted(Formatting.GRAY));
                        return 1;
                    }))
                    .then(ClientCommandManager.literal("fps").executes(ctx -> {
                        var cfg = ConfigManager.get();
                        ctx.getSource().sendFeedback(Text.literal("— FPS Boost —").formatted(Formatting.GREEN));
                        ctx.getSource().sendFeedback(Text.literal("Optimizer: " + (cfg.fpsOptimizerEnabled ? "ON" : "OFF") + " | Aggressive: " + (cfg.fpsAggressiveMode ? "ON" : "OFF")));
                        ctx.getSource().sendFeedback(Text.literal("Culling: " + cfg.entityCullingDistance + " blocks | Particles: " + cfg.maxParticlesPerTick)));
                        boolean sodium = net.fabricmc.loader.api.FabricLoader.getInstance().isModLoaded("sodium");
                        boolean lithium = net.fabricmc.loader.api.FabricLoader.getInstance().isModLoaded("lithium");
                        if (!sodium) ctx.getSource().sendFeedback(Text.literal("Tip: Install Sodium + Lithium for 2-3x FPS — Modrinth > Search Sodium").formatted(Formatting.YELLOW));
                        else ctx.getSource().sendFeedback(Text.literal("Sodium ✓  Lithium " + (lithium ? "✓" : "✗ — install for more FPS")).formatted(Formatting.AQUA));
                        return 1;
                    }))
                    .then(ClientCommandManager.literal("verity").executes(ctx -> {
                        var client = MinecraftClient.getInstance();
                        if (client.player == null) return 0;
                        // In singleplayer with integrated server, we can summon directly
                        if (client.getServer() != null && client.player.getWorld() instanceof ServerWorld sw) {
                            BlockPos pos = client.player.getBlockPos().up();
                            var e = ShadowCore.VERITY_TYPE.spawn(sw, pos, SpawnReason.COMMAND);
                            if (e != null) {
                                e.setOwner(client.player);
                                e.setTamed(true);
                                ctx.getSource().sendFeedback(Text.literal("Verity summoned! Tame with golden apple if needed. She chases you and grants second chance.").formatted(Formatting.LIGHT_PURPLE));
                            }
                        } else {
                            // Multiplayer — server must have mod; give egg tip
                            ctx.getSource().sendFeedback(Text.literal("Verity needs modded server or singleplayer. In singleplayer use /summon shadowcore:verity").formatted(Formatting.GRAY));
                            ctx.getSource().sendFeedback(Text.literal("In creative, Verity Spawn Egg is in Spawn Eggs tab.").formatted(Formatting.GRAY));
                        }
                        return 1;
                    }))
                    .then(ClientCommandManager.literal("shader").executes(ctx -> {
                        ctx.getSource().sendFeedback(Text.literal("— ShadowCore BW Shader —").formatted(Formatting.AQUA));
                        ctx.getSource().sendFeedback(Text.literal("Bundled: ShadowCore_BW.zip (bright, light, BW-optimized) auto-copies to shaderpacks/ if Iris present")));
                        ctx.getSource().sendFeedback(Text.literal("Enable: Options > Video Settings > Shader Packs")));
                        Text link = Text.literal("[Get Complementary Shaders (recommended)]").formatted(Formatting.YELLOW, Formatting.UNDERLINE)
                                .styled(s -> s.withClickEvent(new ClickEvent(ClickEvent.Action.OPEN_URL, "https://modrinth.com/shader/complementary")));
                        ctx.getSource().sendFeedback(link);
                        return 1;
                    }))
            );
        });
    }
}
