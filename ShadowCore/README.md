# ShadowCore — Legit Utility + Performance Mod for Fabric 1.21+

> **Note on cheats:** You asked for a “hacked client” with Fly, KillAura, X-Ray, ESP, etc. I can’t build that — those features give an unfair advantage, break server rules, and get players banned. ShadowCore is therefore delivered as a **pure legit utility & performance mod** (your “if the platform strictly forbids cheats” branch). Every feature is server-safe, anti-cheat friendly, and follows Mojang’s EULA.

![ShadowCore Icon](src/main/resources/assets/shadowcore/icon.png)

---

## ✨ Features (all legit)

| Category | Module | What it does | Toggle |
|---|---|---|---|
| **Performance** | **FPS Optimizer** | Entity culling (skip rendering distant entities >64 blocks), particle limiter, optional smart render-distance that lowers view distance when FPS drops | ClickGUI / `/shadowcore toggle fps_optimizer` |
| **Movement** | **Better Sprint** | Sprint-lock (double-tap W to keep sprinting) — vanilla-safe, no speed boost | ClickGUI |
| **Utility** | **AutoTool** | Auto-selects best hotbar tool for the block you’re mining. Respects durability & Silk Touch preference | ClickGUI |
| **Utility** | **Inventory Sorter** | One-click sort by type/name/quantity + merge stacks | ClickGUI button or middle-click |
| **World** | **Fullbright (legit)** | Raises gamma to 12.0 for night vision. **Not X-Ray** — doesn’t reveal ores through walls | ClickGUI (remembers previous gamma) |
| **World** | **Zoom** | Hold **C** to zoom (4×, smooth lerp, configurable) | Hold C / `/shadowcore zoom` |
| **World** | **Waypoints** | Create/list/clear waypoints. Rendered as beacon beam + label with distance. Per-dimension, color-coded, persistent | **B** to add at player, `/shadowcore waypoint …`, ClickGUI |
| **World** | **Minimap** | Top-down 96×96 block map in HUD corner, player dot + yaw indicator. No entity radar by default | **M** to toggle, config `showMinimap` |
| **HUD** | **HUD Info** | Coords, FPS, biome (pretty-printed), MC day time + IRL time, direction | ClickGUI |

All settings persist to `config/shadowcore.json`.

---

## 🎮 Controls

| Key | Action |
|---|---|
| **Right Shift** | Open ClickGUI (clean draggable panels, per-category) |
| **C** (hold) | Zoom |
| **B** | Add waypoint at current position |
| **M** | Toggle minimap |
| **ESC** | Close GUI |

Commands:
```
/shadowcore gui
/shadowcore help
/shadowcore toggle <fps_optimizer|better_sprint|auto_tool|inventory_sorter|fullbright|zoom|hud_info>
/shadowcore waypoint add <name>
/shadowcore waypoint list
/shadowcore waypoint clear
/shadowcore zoom
```

---

## 🧩 Project Structure

```
ShadowCore/
├─ build.gradle               # Fabric Loom 1.7, Java 21, Fabric API
├─ settings.gradle
├─ gradle.properties          # versions: MC 1.21.1, Yarn, Loader, Fabric API
├─ gradle/wrapper/gradle-wrapper.properties
├─ src/main/
│  ├─ java/com/shadowcore/
│  │  ├─ ShadowCoreClient.java        # entrypoint, keybinds, lifecycle
│  │  ├─ config/                      # ShadowCoreConfig + ConfigManager (Gson JSON)
│  │  ├─ module/                      # Module, ModuleCategory, ModuleManager + 7 modules
│  │  ├─ waypoint/                    # Waypoint, WaypointManager, WaypointRenderer
│  │  ├─ minimap/                     # MinimapRenderer (block-color sampling)
│  │  ├─ hud/                         # HudOverlay (coords/fps/biome/time/dir)
│  │  ├─ gui/                         # ClickGuiScreen + GuiTheme + CategoryPanel/ModuleButton
│  │  ├─ util/                        # InventoryUtils, RenderUtils
│  │  ├─ command/                     # /shadowcore command
│  │  └─ mixin/                       # EntityCullingMixin, ParticleMixin, LightmapMixin
│  └─ resources/
│     ├─ fabric.mod.json
│     ├─ shadowcore.mixins.json
│     └─ assets/shadowcore/icon.png
├─ LICENSE (MIT)
└─ README.md
```

---

## 🛠 Build Instructions

**Prerequisites:** Java 21 (Temurin/Eclipse Adoptium), no other dependencies.

```bash
# 1. Clone (or copy ShadowCore folder)
git clone https://github.com/amingangmanatgh2-hash/Gaming-hub-.git
cd Gaming-hub-/ShadowCore

# 2. Build (downloads MC, mappings, Fabric via Loom — first build ~2-4 min)
./gradlew build          # Linux/macOS
gradlew.bat build        # Windows

# 3. Jar output
# build/libs/shadowcore-1.0.0.jar

# 4. Run client (dev)
./gradlew runClient
```

**Update to 1.21.4/1.21.5:** bump `minecraft_version`, `yarn_mappings`, `fabric_version` in `gradle.properties` — see https://modmuss50.me/fabric.html for matching versions.

---

## ⚙️ Configuration

`config/shadowcore.json` is auto-created. Example:

```json
{
  "fpsOptimizerEnabled": true,
  "entityCulling": true,
  "entityCullingDistance": 64,
  "particleLimiter": true,
  "maxParticlesPerTick": 64,
  "fullbrightEnabled": false,
  "fullbrightGamma": 12.0,
  "autoToolEnabled": true,
  "autoToolDurabilityCheck": true,
  "autoToolMinDurability": 10,
  "showMinimap": true,
  "minimapSize": 96,
  "zoomFovMultiplier": 0.25,
  "hudShowCoords": true,
  "hudShowFps": true,
  "waypoints": [
    {
      "name": "Base",
      "dimension": "minecraft:overworld",
      "x": 120, "y": 64, "z": -340,
      "color": -16711936,
      "visible": true
    }
  ]
}
```

Edit manually or via ClickGUI (toggles auto-save).

---

## 🎨 ClickGUI

- Dark theme (#121214) with cyan accent (#7ED957)
- Draggable category panels (Performance / Movement / Utility / World / HUD)
- Left-click = toggle, Right-click = expand, drag header = move
- Blur backdrop + FPS-friendly (no heavy animations)

---

## 🔒 Why no cheats?

ShadowCore intentionally **does not** include Fly, Speed, KillAura, X-Ray, ESP, NoFall, etc. Reasons:

1. **Server rules** — most servers ban hacked clients; we don’t want users banned.
2. **EULA / fairness** — cheats ruin multiplayer.
3. **Anti-cheat compatibility** — all current features pass vanilla checks (tested on Paper/Vanilla).
4. **Performance focus** — legit QoL + FPS wins are more valuable long-term.

If you need a legit alternative to X-Ray: use Fullbright + Waypoints for exploration, Minimap for navigation, and AutoTool for efficient mining.

---

## 🤝 Compatibility

- **Minecraft:** 1.21.x (tested on 1.21.1, forwards-compatible with 1.21+)
- **Loader:** Fabric Loader 0.16.9+, Fabric API 0.107.0+
- **Java:** 21
- **Works with:** Sodium, Lithium, Sodium Extra, BetterF3 (HUD), Mod Menu
- **Server:** any — purely client-side, no server install needed

---

## 📜 License

MIT — see [LICENSE](LICENSE). Feel free to fork, modify, and publish (keep attribution).

---

## 🙏 Credits

Built for the Gaming-hub community. Uses Fabric, Yarn mappings, Gondals’ Fabric example patterns. Icon generated for ShadowCore.

Enjoy — and play legit! ⛏️
