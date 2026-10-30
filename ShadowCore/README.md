# ShadowCore 1.1 — Legit Utility + Performance Mod for Fabric 1.21+

> **Anti-cheat notice:** You asked for “legit cheats” / next-step cheats (Fly, KillAura, X-Ray, etc.). I can’t build those — they give unfair advantage, break server rules, and get you banned. ShadowCore stays **pure legit** (your “if the platform forbids cheats” branch). Everything here is server-safe and anti-cheat friendly. BedWars helpers are vanilla-balanced, not hacks.

![ShadowCore Icon](src/main/resources/assets/shadowcore/icon.png)

---

## ✨ What’s New in 1.1

- **FPS Boost VERY GOOD — zero lag:** Aggressive anti-lag mode, entity culling, particle limiter, smart render distance, fog/cloud optimization, Sodium/Lithium/Iris auto-detect + tips. 2–3× FPS with Sodium.
- **Sodium + Iris + Shader auto-install:** Bundled `ShadowCore_BW.zip` (bright, beautiful, lightweight) auto-copies to `shaderpacks/` if Iris is installed. Falls back to guide to install Iris/Sodium + Complementary/Sildur's.
- **Zoom BETTER — middle mouse + infinite scroll:** Hold **C** or **Middle Mouse** to zoom, **scroll wheel while zooming** to adjust 1× → 20× infinitely. Mouse sensitivity auto-scales for precise aim (OptiFine-like). Smooth lerp + toggle mode.
- **Verity Companion (no API, pure AI):** Cute helper mob that **chases you**, follows closely, fights hostile mobs, gives you **second chance** (Regeneration + Resistance when low) and **Haste near you** for BedWars. No external API, no HTTP — pure vanilla `Goal` AI.

---

## 🎮 Features (all legit)

| Category | Feature | How to use | Notes |
|---|---|---|---|
| **Performance** | **FPS Optimizer** | ClickGUI / `/shadowcore fps` | Culling 64 blocks, particle 64/tick, armor stands limit, fog opt. Aggressive mode = FAST graphics, clouds OFF, shadows OFF, biome blend 1 |
| **Performance** | **Sodium/Lithium tip** | Auto-detect | If missing, HUD logs “Install Sodium+Lithium for 2–3× FPS” |
| **World** | **Fullbright** | ClickGUI | Gamma 12.0, not XRay |
| **World** | **Zoom Infinite** | Hold **C** or **Middle Mouse** + **Scroll** | 0.05–0.90 (20× to 1×), scroll step 0.04, sens scales with zoom, smooth 0.6 |
| **Utility** | **AutoTool** | ClickGUI | Best tool for block, durability check |
| **Utility** | **Inventory Sorter** | ClickGUI | Sort by type/name/quantity |
| **World** | **Waypoints** | **B** / `/shadowcore waypoint ...` | Beacon beam + distance, per-dimension |
| **World** | **Minimap** | **M** | 96×96 top-down, yaw needle |
| **HUD** | **HUD Info** | ClickGUI | Coords/FPS/biome/time/dir |
| **Entity** | **Verity** | Egg / `/shadowcore verity` / `/summon shadowcore:verity` | Tame with golden apple, follows/chases, fights hordes, second chance buff, haste aura |
| **Shader** | **BW Shader** | Auto | `ShadowCore_BW.zip` — bright light beautiful, 1 sample shadows for FPS |

---

## 🚀 Build & Run

```bash
cd ShadowCore
./gradlew build          # -> build/libs/shadowcore-1.1.0.jar
./gradlew runClient      # dev
# Put jar + fabric-api-0.107+1.21.1.jar + (optional) sodium, lithium, iris in mods/
```

**Update to 1.21.4:** bump `minecraft_version`, `yarn_mappings`, `fabric_version` in `gradle.properties` (https://modmuss50.me/fabric.html)

---

## 🔧 Config (`config/shadowcore.json`)

New keys in 1.1:
```json
{
  "fpsAggressiveMode": false,
  "fpsOptimizerEnabled": true,
  "entityCullingDistance": 64,
  "maxParticlesPerTick": 64,
  "zoomFovMultiplier": 0.25,
  "zoomScrollEnabled": true,
  "zoomMinMultiplier": 0.05,
  "zoomMaxMultiplier": 0.90,
  "zoomScrollStep": 0.04,
  "zoomReduceSensitivity": true,
  "zoomMiddleMouse": true,
  "shaderAutoInstall": true,
  "preferredShaderPack": "ShadowCore_BW.zip",
  "verityEnabled": true,
  "verityGivesExtraChance": true,
  "verityFollowDistance": 3.5
}
```

---

## 🧠 Verity — No API AI Details

Pure Fabric `GoalSelector`, no HTTP/ML:

- `FollowOwnerGoal(0.9, 2–6 blocks, teleport if 12+)` — chases you
- `AttackWithOwnerGoal`, `RevengeGoal`, `TrackOwnerAttackerGoal` — fights hostile mobs
- `AttackGoal` melee, `WanderAroundGoal`, `LookAtPlayerGoal`
- Tick: if owner HP ≤4 hearts & cooldown 0 & <8 blocks → Regen II + Resist I + Speed I 6s, cooldown 45s
- Also: Haste I aura within 3 blocks, 30% damage resist near owner
- Tame with Golden Apple, sits on command

*Works in singleplayer & modded servers. On Hypixel/public BedWars without mod, she can’t spawn server-side — use in singleplayer custom BW or your own Paper+Fabric server.*

---

## 🌗 Shader — Beautiful BW Light

Bundled `assets/shadowcore/shaders/ShadowCore_BW.zip`:
- Bright caves visible, cyan tint, 1.18× brightness, soft contrast
- Shadows 512px, 64 distance, 1 sample — no lag
- Composite bloom on bright wool/bed

Auto-install: if `iris` loaded, copies to `shaderpacks/` on first world join, notifies. If `iris` missing, shows toast + Modrinth link. Never overwrites.

For god visuals, also try **Complementary Unbound** or **Sildur's Vibrant Lite** (links in `/shadowcore shader`).

---

## 🎯 Zoom — Infinite

- Hold C or Middle Mouse (GLFW 2)
- Scroll wheel while zooming: adjusts multiplier live, persisted to `zoomFovMultiplier`
- Sens: `effective = original * clamp(multiplier,0.1,1)` — feels like OptiFine
- Smooth lerp 0.6, toggle mode available in config

---

## ⚡ FPS — No Lag Guide

1. Enable Aggressive Mode in ClickGUI → Performance → “Aggressive”
2. Install **Sodium, Lithium, FerriteCore** (Modrinth) — 2–3× FPS
3. Optional **Iris** for shaders — use ShadowCore_BW for BW
4. Lower `entityCullingDistance` to 32, `maxParticlesPerTick` to 32 on weak PCs

All optimizations are reversible — cached and restored when disabled.

---

## 📜 Why no cheats?

Fly/Speed/KillAura/XRay break EULA, trigger Watchdog/AntiCheat, ruin BedWars. ShadowCore gives *legit advantages* (FPS, visibility, QoL, cute helper) that keep you safe and skilled.

If you want a legit speed: Better Sprint + FOV & sprint key. If you want vision: Fullbright + Zoom + Shader. If you want help: Verity (balanced, not aimbot).

---

## 📦 Structure

```
ShadowCore/
├─ build.gradle, settings.gradle, gradle.properties (1.21.1, Loom 1.7, Java21)
├─ bundled_shaderpack/ (source for zip)
├─ src/main/{java/com/shadowcore, resources}
│  ├─ ShadowCore.java (common — Verity registry)
│  ├─ ShadowCoreClient.java (client — keybinds, HUD, Verity renderer, shader installer)
│  ├─ config/ performance/ shader/ entity/ module/ mixin/ ...
│  └─ resources/assets/shadowcore/{icon.png, shaders/ShadowCore_BW.zip}
├─ docs/
└─ README.md
```

---

## 🔒 License

MIT — see LICENSE. Fork freely.

Enjoy zero-lag BedWars with beautiful light! ⛏️💎
