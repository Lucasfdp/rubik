# 12 — Visual polish review: background, lighting, HUD

**Date:** 2026-09-28 · **Scope:** `src/render/app.c` (clear/background), `src/render/hud.c` (2D overlay), `src/render/draw.c` + `src/render/geometry.c` (cubie/sticker geometry & palette), `include/render/rlights.h` + `assets/shaders/*/lighting.*` (Phong lighting), `src/render/fx.c` (particles).
**Reviewer lens:** raylib rendering / visual design. Trigger: current build looks flat and low-contrast — starting point was "I don't like the white background."
**Stack confirmed:** raylib + rlgl + raymath, custom Phong shader via raylib's official `rlights.h` helper (GLSL 120/330 variants in `assets/shaders/`). No raw GL/GLFW code, no existing skybox/shadow-map infra.

---

## 0. TL;DR

| ID | Sev | Area | Root cause / gap (one line) |
|----|-----|------|------------------------------|
| V1 | BLOCKER | Background | `ClearBackground(RAYWHITE)` (`app.c` ~line 406) is a single flat `{245,245,245,255}` — white/light stickers nearly vanish into it, cube reads as a flat cutout, not a 3D object in space. |
| V2 | BLOCKER | Grounding | No ground plane, contact shadow, or reflection anywhere in `draw.c` — the cube has nothing anchoring it in space; gets worse once V1 changes (dark cube on dark void). |
| V3 | SHOULD-FIX | HUD | `hud.c` draws everything as flat black `DrawText` on raylib's default bitmap font, directly over the 3D scene, no background panel — illegible over light stickers, no grouping/hierarchy. |
| V4 | SHOULD-FIX | Lighting | Two static point lights only (`rlights`: ambient `{0.35,0.35,0.38,1.0}`, white key + cool-blue fill) — flat, even shading, no rim/back light to separate cube silhouette from background. |
| V5 | NITPICK | Palette | Vivid palette (`P` toggle: pink/purple/lime/cyan/orange-red/teal, defined in `geometry.c`) was tuned against the current white bg — recheck contrast once V1 ships. |
| V6 | NITPICK | Cube style | `BODY_COLOR {30,30,30,255}` + `DrawCubeWires(DARKGRAY)` outline + 8 corner-fillet spheres per cubie currently reads as "wireframe toy" more than polished plastic — likely self-resolves once V1/V4 land; revisit only if it still looks off. |
| V7 | NITPICK | Idle state | Only motion at rest is the hardcoded 6s-idle auto-orbit (6°/s, `input.c`) — no subtle showcase motion as a deliberate default state. |

**Recommended order:** V1 → V2 → V3 → V4, then reassess V5–V7 as a polish pass. V1 is a one-line change with the single highest perceived-quality-per-effort ratio in this list — ship it first before judging anything else.

---

## 1. Blockers

### V1 — Flat near-white background kills contrast & depth

**Where:** `src/render/app.c`, `ClearBackground(RAYWHITE)` (~line 406). `RAYWHITE = {245,245,245,255}`, no other background color exists anywhere in the codebase.

**Why it matters:** this is the first thing anyone notices. The U-face (white stickers) and the light gray plastic between stickers sit at nearly the same luminance as the clear color — the cube's own silhouette does most of the "is this even 3D" work, and it's losing that job to the background right now.

**Options:**

| Option | Description | Pros | Cons |
|---|---|---|---|
| (a) Flat dark clear color | Swap `RAYWHITE` → e.g. `{18,18,22,255}` (near-black) or `{22,24,32,255}` (deep slate) | One-line change; instant contrast boost for all 6 sticker colors on both palettes; makes the existing Phong lighting pop immediately | Can look harsh/empty without a shadow (→ V2); loses the "clean studio" look some cube-timer apps go for |
| (b) Vertical gradient | Full-screen gradient quad drawn in 2D mode *before* `BeginMode3D()` each frame (`DrawRectangleGradientV`), e.g. dark slate top → near-black bottom | Near-zero perf cost, visibly more polished than flat color, common in cube-timer/showcase apps | Slightly more code than (a); still needs V2 to avoid the "floating" look |
| (c) Procedural skybox + baked/blob shadow | Gradient sphere or skydome behind camera, plus a real ground shadow | Best depth/grounding result, cube stops looking like it's in a void at all | Real implementation cost: new render pass or baked shadow texture, no existing infra to build on |

**Recommendation:** ship (a) or (b) first — dark neutral in the `{18,18,22}`–`{28,28,34}` range. It's the best ratio of effort to visible improvement and unblocks judging everything else (V4's rim light, V6's material read) against the right backdrop. Layer in (c)'s shadow via V2 next; treat a full skybox as a later nice-to-have, not part of this pass.

**Open question for Lucas:** flat dark vs. gradient — pick one before an agent implements, since it changes whether `app.c`'s draw loop needs a 2D pre-pass.

---

### V2 — No ground plane or shadow — cube looks like it's floating

**Where:** no shadow/ground code found in `draw.c` or elsewhere; confirmed absent, not just unused.

**Why it matters:** combined with V1, the cube has literally nothing underneath it — no shadow, no floor, no reflection. Once the background goes dark this gets *more* noticeable, not less, because there's no gradient-into-white to hide the lack of grounding.

**Options:**

| Option | Description | Pros | Cons |
|---|---|---|---|
| (a) Blob/contact shadow | Semi-transparent dark ellipse under the cube (`DrawCircle3D` or a soft-edged quad with an alpha-faded circular texture) | Cheap, big visual payoff, matches the codebase's current scope (no shadow-map infra to build) | Not a "real" shadow — doesn't respond to light direction or occluders |
| (b) Real-time shadow map | Second render pass from the light's POV into a depth texture, sampled in the main shader | Physically correct, matches the moving point lights | Meaningfully heavier: new FBO/depth-texture setup, shader changes on top of the existing `rlights` Phong pipeline |
| (c) Reflection-plane trick | Draw a mirrored copy of the cube below itself, faded with alpha/distance | "Studio product shot" look | Extra draw call per frame (cube is already drawn twice per frame for shaded+wireframe passes, per V6 — a third pass adds up) |

**Recommendation:** (a), the blob shadow — matches the project's current rendering complexity, ships fast, solves the "floating" problem without new render-pass infrastructure.

---

## 2. Should-fix

### V3 — HUD has no visual hierarchy, will be illegible on a dark background too

**Where:** `src/render/hud.c` — all text via `DrawText` with raylib's default font, flat colors: body text `{20,20,20,255}` (near-black), progress-bar bg `{200,200,200,255}`, accent `{0,158,96,255}` (green, same hue as the Classic F-face sticker).

**Why it matters:** near-black text with no panel behind it currently works only because the background is near-white. It will invert to being invisible once V1 ships (dark text on dark bg) unless it's fixed as part of the same pass — treat V1 and V3 as coupled, not independent.

**Options:**

| Option | Description | Pros | Cons |
|---|---|---|---|
| (a) Semi-transparent panel | `DrawRectangleRounded` (alpha ~160-200, dark or the new bg color) behind each text block, text switches to a light color | Cheap, immediately fixes the legibility coupling with V1 | Adds a bit of visual weight; needs the panel color chosen relative to whatever V1 picks |
| (b) Load a real font | `LoadFontEx` with a monospace/UI font instead of raylib's default bitmap font | Meaningfully more "designed" feel for very little code | One more asset to ship (`assets/fonts/...`) and load/unload lifecycle |
| (c) Collapse always-on hints | Keep only status + "press ? for keys" visible by default, full keybind list behind a toggle | Cleans up screen real estate, currently 4 stacked hint lines is a lot of permanent text | Changes interaction model — confirm this is wanted before doing it, lower priority than (a)/(b) |

**Recommendation:** do (a) as part of the same change as V1 (they must ship together or the HUD becomes unreadable), (b) as a quick follow-up quality bump. Leave (c) as optional — it's a UX call, not a rendering fix.

---

### V4 — Two static point lights, no rim light, flat shading

**Where:** lighting setup via `rlights.h`, ambient `{0.35,0.35,0.38,1.0}`, light 1 white at `{6,8,6}`, light 2 cool-blue-tinted `{150,170,255,255}` at `{-6,4,-4}`. Shaded pass and wireframe/fillet pass are drawn separately (wireframe/fillets are unlit) — see `draw.c`.

**Why it matters:** two front-ish lights give even, flat coverage but nothing separates the cube's silhouette from whatever is behind it — that job currently falls entirely to the outline (`DARKGRAY` `DrawCubeWires`), which is doing double duty as both "sticker border" and "the only thing making the cube read as an object."

**Options:**

| Option | Description | Pros | Cons |
|---|---|---|---|
| (a) Add a rim/back light | Third light behind/above the cube, only lighting silhouette edges | Highest single-change value once bg is dark (V1) — separates cube from background without touching sticker colors or geometry | One more light in the `rlights` array + shader already supports multiple lights, so mostly a data change |
| (b) Re-balance existing lights | Lower ambient slightly, push key light warmer, fill cooler, for more lit/shadow contrast per face | No new lights needed | Smaller visual delta than (a) |
| (c) Tone mapping / vignette | Post-process pass for a more "rendered" look | Biggest polish ceiling | Real scope: needs a render-to-texture + post shader pass, out of proportion to the rest of this list |

**Recommendation:** (a) rim light, after V1 ships (judging silhouette separation against the current white bg is close to meaningless — everything separates from white). Treat (c) as out of scope for this pass.

---

## 3. Nitpicks (one-liners, revisit after V1–V4)

- **V5 — Palette contrast:** Vivid palette (`geometry.c`: U pink `{255,20,147}`, D purple `{155,0,255}`, F lime `{160,255,0}`, B cyan `{0,191,255}`, R orange-red `{255,105,0}`, L teal `{0,200,180}`) was eyeballed against white; recheck saturation/contrast once the background is dark — may need no change, may need slight desaturation.
- **V6 — Wireframe/toy read:** `BODY_COLOR {30,30,30,255}` + `DARKGRAY` `DrawCubeWires` outline + 8 corner-fillet spheres per cubie currently reads as a wireframe sketch. Likely a side effect of V1+V4, not its own bug — don't touch geometry until those ship, then re-evaluate.
- **V7 — Idle motion:** only existing motion at rest is the 6s-idle auto-orbit (6°/s) in `input.c`. Fine as-is; a slower default idle bob/rotation is a "nice to have," not a fix for anything broken.

---

## 4. Next actions

1. Pick a direction for V1 (flat dark vs. gradient) — this gates V2–V4.
2. Implement V1 + V3 together (`app.c` clear color + `hud.c` panel/text-color swap) — they're coupled, shipping one without the other breaks HUD legibility.
3. Implement V2 (blob/contact shadow).
4. Implement V4 (rim light via `rlights`).
5. Re-render and screenshot; revisit V5–V7 only if they still look off against the new scene.

---

## 5. Implementation status (2026-09-28)

- **V1 (background)** — done. `app.c`: `ClearBackground(BLACK)` + `DrawRectangleGradientV` (top `{28,30,38,255}` → bottom `{12,12,15,255}`), drawn each frame before `BeginMode3D`.
- **V2 (ground shadow)** — skipped by request. Previously caused issues with the D-face render and risk to the camera; not implemented.
- **V3 (HUD panel)** — done. `hud.c`: `draw_panel()` (`DrawRectangleRounded`, dark semi-transparent fill) behind each status/hint block, body text switched to light `{225,225,230,255}`. Layout/positions/font unchanged.
- **V4 (rim light)** — done. `draw.c`: third `rlights` light at `{-2,10,-8}`, cool-white `{170,185,210,255}`. Existing two lights untouched; `MAX_LIGHTS` already supported 4 in both the header and the GLSL shaders, no cap change needed.
- **V5–V7** — untouched, as scoped.

### V8 (new) — Camera pitch had no clamp, gimbal-locked at the poles [BLOCKER, fixed]

Reported by Lucas: orbiting up/down never reaches the back of the cube — the camera does "a 180 and goes back 180 on itself."

**Root cause:** `pitch_deg` in `input.c` had no clamp at all (right-drag orbit or arrow-key nudge). The camera's `up` vector is fixed to `{0,1,0}` in `app.c`. As pitch approaches ±90° the view direction runs parallel to that fixed `up`, which is a gimbal-lock singularity in the LookAt matrix — the derived right/up vectors degenerate at the pole, so continuing to pitch bounces the camera back instead of carrying it over to the far side. Not a deliberate limit — there was none.

**Fix shipped:** `ORBIT_PITCH_LIMIT_DEG = 89.0f`, clamped after both the drag delta and the arrow-key nudge (`input.c`).

**Known limitation:** this stops the bounce but does not implement true pole-crossing (pitching straight over the top to see the underside in one continuous motion) — that needs an `up`-vector flip at the pole and touches the same math the 4 camera presets lerp through. Revisit only if reaching the exact top/bottom pole view (not just the back, which yaw already reaches) turns out to matter.
