# Verity Companion — BedWars Helper (No API)

Verity is your chase companion, inspired by BW “Varity” — but legit.

## Summon
- Creative: Spawn Eggs tab → Verity Spawn Egg
- Command: `/shadowcore verity` (singleplayer) or `/summon shadowcore:verity`
- Tame: Right-click with Golden Apple (100%)

## Behavior (pure vanilla Goal AI, no HTTP/ML)
- **Chase you:** `FollowOwnerGoal(speed 0.9, min 2.0, max 6.0, teleport if >12)` — sprints to keep up, teleports if you pearl/bridge far.
- **Fight:** `AttackWithOwnerGoal` + `RevengeGoal` + `TrackOwnerAttackerGoal` — if you hit a zombie/skeleton or it hits you, Verity helps. Targets `MobEntity` only (no player KillAura).
- **Wander:** Near owner when idle, looks at you.

## Second Chance (Extra Life)
When your HP ≤4 hearts and Verity is within 8 blocks, cooldown ready:
- Regeneration II 6s + Resistance I 6s + Speed I 5s
- Cooldown 45s, toast message, hearts particle
- Singleplayer/modded server only — not a hack, balanced buff.

## Haste Aura
Within 3 blocks: Haste I pulse every 2s — breaks bed defense faster. Only if Verity alive near you.

## Damage Reduction
30% less damage to Verity when within 4 blocks of owner — loyal protector.

## No API
No OpenAI, no external pub API. All AI via `GoalSelector`. You can extend with your own goals without internet.

## Multiplayer
Works in singleplayer and Fabric modded servers (Paper+Fabric with mod). On vanilla Hypixel, cannot spawn server-side — use as singleplayer BW practice or your own server.
