# Pathfinder

Pathfinder is a predictive **autoplay bot** for Geometry Dash, built as a [Geode](https://geode-sdk.org) mod.
Instead of reacting to hazards when they are already under you, it **simulates your trajectory** every frame
(both "do nothing" and "jump now" branches), picks the safe branch, and taps jump for you — with the predicted
path drawn right on screen.

## Controls

- **Geode menu → Pathfinder → "Bot Enabled"** — master switch, works everywhere with no extra UI
- **Corner button** — "PF: ON/OFF" in the bottom-right of the level (can be hidden with "Show Corner Button")
- **B** — toggle on PC

## Settings

| Setting | Default | Notes |
| --- | --- | --- |
| Lookahead Distance | 260 px | How far ahead the simulator scans |
| Simulation Time | 1.5 s | How far into the future it simulates |
| Gravity | 1185 px/s² | Community estimate (39.5 blocks × 30 px). Tune if needed |
| Jump Velocity | 460 px/s | Community estimate. Tune if needed |
| Move Speed | 311 px/s | Community estimate (~10.4 blocks). Tune if needed |
| Show Predicted Path | on | Green = chosen path, red = predicted to hit |

## Limitations (honest)

- **Cube mode only**, normal (non-inverted) gravity
- Hazard avoidance only — solid blocks, portals, triggers, other gamemodes and slopes are not solved yet
- GD's exact per-version physics constants are not publicly exposed, so gravity / jump / speed are
  community estimates. If the bot jumps too early or too late, tune them in the mod settings
- Hazards are approximated as rectangles (spikes are triangular in-game)
- Experimental — it will not clear arbitrary levels. It is a solid, tunable foundation, not a solved-problem machine

## Changelog

v1.0.0 — initial release: predictive trajectory simulation, hazard detection, pause-menu toggle, B hotkey,
path visualization, Windows + Android builds.
