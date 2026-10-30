# Performance — Zero Lag Guide (1.1)

## Quick Win (2-3x FPS)
1. Install **Sodium + Lithium + FerriteCore** (Modrinth, latest for 1.21.1) alongside ShadowCore.
2. In ShadowCore ClickGUI → Performance → Aggressive ON.
3. Optional **Iris** + ShadowCore_BW shader (bright light, no lag).

ShadowCore auto-detects Sodium/Iris and logs synergy: check latest.log for “[ShadowCore] Perf mods detected”.

## What Aggressive Does
- Graphics FAST, Biome Blend 1, Clouds OFF, Entity Shadows OFF, Particles MINIMAL
- Fog optimization, ArmorStand limit 16, ItemFrame culling far
- Smart render distance: FPS <55 → -1 view, FPS >70 → +1 view (4–12)
- Restores on disable (caches prev values).

## No Lag Checklist
- Entity culling 32–64: hides distant entities (huge on BW with many players)
- Particle 32–64: drop excess (mixins)
- No Fancy Trees/Grass if needed
- Close ClickGUI when not used, minimap size 64 on weak GPU

## Shader FPS
ShadowCore_BW: shadowMap 512, distance 64, noHardwareFiltering, no mipmaps → +15 FPS vs heavy shaders.
For max beauty + speed: Complementary Unbound Low or MakeUp Ultra Fast.

## Verifying
F3: FPS 120+ with Sodium, 60+ without. `/shadowcore fps` shows optimizer state.
