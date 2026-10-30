# Building ShadowCore

## Requirements

- Java 21 (JDK, not JRE) — Fabric 1.21+ mandates 21.
  - Ubuntu: `sudo apt install openjdk-21-jdk`
  - Windows: https://adoptium.net/temurin/releases/?version=21

- Internet on first build (downloads MC 1.21.1 ~ 80 MB, Yarn, Fabric API).

## Build

```bash
cd ShadowCore
./gradlew build
```

Output: `build/libs/shadowcore-1.0.0.jar`

Copy to `mods/` alongside `fabric-api-*.jar`.

## Dev run

```bash
./gradlew runClient        # launches MC with mod
./gradlew runClient --offline  # offline mode
```

## Update mappings

Check https://linkie.shedaniel.me/mappings and https://modmuss50.me/fabric.html

Update `gradle.properties`:

```
minecraft_version=1.21.4
yarn_mappings=1.21.4+build.1
fabric_version=0.110.0+1.21.4
```

Then `./gradlew clean build`.

## IDE

- IntelliJ IDEA: Import as Gradle project, use Java 21 SDK.
- VS Code: Install Gradle extension, set `java.configuration.runtimes` to 21.
- Run `gradlew genSources` for deobfuscated sources.

## Troubleshooting

- `Unsupported class file major version 65` → wrong Java (need 21, not 17).
- `Could not resolve com.mojang:minecraft` → no internet or bad `minecraft_version`.
- Mixins not applied → check `shadowcore.mixins.json` package matches actual package, rebuild.

## Publishing

- Increment `mod_version` in `gradle.properties`
- `./gradlew build remapJar`
- Upload `build/libs/shadowcore-*.jar` to Modrinth/CurseForge
