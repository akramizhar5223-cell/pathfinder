# Pathfinder (Geometry Dash mod)

A predictive path-simulation autoplay bot for Geometry Dash, built as a [Geode](https://geode-sdk.org) mod.

Source + build workflow. Windows / Android builds are produced by GitHub Actions (`.github/workflows/build.yml`),
or locally with the [Geode CLI](https://docs.geode-sdk.org).

## Get a build

### Option A — GitHub Actions (no local tools needed)

1. Push this folder to your GitHub repository:
   ```sh
   git init
   git add .
   git commit -m "Pathfinder v1.0.0"
   git branch -M main
   git remote add origin https://github.com/<you>/pathfinder.git
   git push -u origin main
   ```
2. Open the **Actions** tab → latest "Build Geode Mod" run → download the `Pathfinder.geode` artifact.
   (Run it via **Run workflow** if you want to trigger it manually.)

### Option B — local build (Windows + Android)

Prerequisites: CMake, a C++ toolchain, and the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli).

```powershell
# Windows build
geode build

# Android build (needs Android NDK + ANDROID_NDK_ROOT, and once: geode sdk install-binaries -p android64)
geode build -p android64     # arm64 phones
geode build -p android32     # 32-bit phones
```

The combined `.geode` (CI) or per-platform `.geode` (local) goes in the Geode mods folder:

- **Windows**: `<Geometry Dash>\geode\mods\` (e.g. `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods\`)
- **Android**: `/storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/` on the phone

On this machine the mod folder sits at `E:\Pathfinder\` (your USB phone). The `.geode` produced by CI/local build
just needs to be copied alongside this source.

## Notes

See `about.md` for features, controls, settings and honest limitations. v1 targets **cube mode, normal gravity**
and treats hazards as rectangles — it is tunable, experimental, and not a universal level solver.
