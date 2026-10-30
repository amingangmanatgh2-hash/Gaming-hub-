package com.shadowcore.command;

import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import com.shadowcore.ShadowCoreClient;
import com.shadowcore.config.ConfigManager;
import com.shadowcore.gui.ClickGuiScreen;
import com.shadowcore.module.Module;
import com.shadowcore.module.ModuleManager;
import com.shadowcore.waypoint.Waypoint;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandManager;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandRegistrationCallback;
import net.minecraft.client.MinecraftClient;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;

/**
 * Client command: /shadowcore
 * <ul>
 *   <li>/shadowcore gui — open ClickGUI</li>
 *   <li>/shadowcore toggle &lt;module&gt; — toggle module</li>
 *   <li>/shadowcore waypoint add &lt;name&gt; — add waypoint at player</li>
 *   <li>/shadowcore waypoint list — list waypoints</li>
 *   <li>/shadowcore waypoint clear — remove all</li>
 * </ul>
 */
public final class ShadowCoreCommand {

    private ShadowCoreCommand() {}

    public static void register() {
        ClientCommandRegistrationCallback.EVENT.register((dispatcher, registryAccess) -> {
            dispatcher.register(ClientCommandManager.literal("shadowcore")
                    .executes(ctx -> {
                        ctx.getSource().sendFeedback(Text.literal("ShadowCore v1.0.0 — type /shadowcore help").formatted(Formatting.GRAY));
                        return 1;
                    })
                    .then(ClientCommandManager.literal("help").executes(ctx -> {
                        var src = ctx.getSource();
                        src.sendFeedback(Text.literal("— ShadowCore Help —").formatted(Formatting.AQUA));
                        src.sendFeedback(Text.literal("/shadowcore gui — Open ClickGUI (Right Shift)")));
                        src.sendFeedback(Text.literal("/shadowcore toggle <module> — Toggle module")));
                        src.sendFeedback(Text.literal("/shadowcore waypoint add <name> — Add waypoint")));
                        src.sendFeedback(Text.literal("/shadowcore waypoint list — List waypoints")));
                        src.sendFeedback(Text.literal("/shadowcore waypoint clear — Clear waypoints")));
                        src.sendFeedback(Text.literal("/shadowcore zoom — Toggle zoom module")));
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
                                    ctx.getSource().sendFeedback(Text.literal(
                                            "- " + w.getName() + " @ " + p.getX() + ", " + p.getY() + ", " + p.getZ() + " [" + w.getDimension() + "]"));
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
                        ctx.getSource().sendFeedback(Text.literal("Zoom " + (on ? "ON (hold C)" : "OFF")).formatted(Formatting.GRAY));
                        return 1;
                    }))
            );
        });
    }
}
