# 3D Experience — Playback Modes, Interaction & Polish

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions. This doc assumes the stack decision in `03-graphics.md` is already made — raylib, vendored under `lib/raylib` — and builds on top of it. It doesn't reopen that decision; if the raylib build spike from `06-roadmap-bonus.md` Sprint 5 fails and you fall back to MLX, everything below still applies, just implemented on top of a hand-rolled rasterizer instead of raylib's calls.)*

## 0. What this doc adds

`03-graphics.md` answers "what do we render with, and how does a move animate." `06-roadmap-bonus.md` schedules that work. Neither one designs what it actually *feels* like to use the finished thing beyond "plays back a solve." This doc is that layer: the different ways someone can run the 3D binary, how good it can look, and the full menu of extras — so you can pick from a considered list instead of bolting things on ad hoc as you go.

Everything here is additive to the existing `render/` module boundary in `04-architecture.md` — nothing here proposes touching `solve/`, `parse/`, or the mandatory binary.

## 1. Three ways to run the bonus binary

Think of these as three "modes" the same renderer supports, not three different programs. All three share the same 26-cubie scene, the same `anim_*` engine, and the same logical cube — they differ only in *who is driving the moves*.

| Mode | Who drives moves | Purpose |
|---|---|---|
| **A — Autoplay** | The solver. Scramble in -> solve -> play back the solution automatically. | This is the graded bonus baseline: "graphically show the cube spin in real time." Ships first. |
| **B — Controlled playback** | The solver's move list, but a human controls pace/direction. | Turns the demo into something you can actually study — step through a solve one move at a time, slow it down, scrub to a specific point. |
| **C — Practice / manual** | The human, directly, one move at a time, exactly like holding a physical cube. | The "use the model like a real cube" ask: free scrambling, free solving-by-hand, a timer, and — on request — hand the current state to the solver instead of a human. |

These are not mutually exclusive states in the code — B is really "A with a pause button and a speed dial," and C is "no move list, moves come from input events instead." Same animation engine underneath for all three, per the `anim_push` / `anim_update` / `anim_current_transform` interface already agreed in `06-roadmap-bonus.md`.

## 2. Mode B — the realtime/slow-motion spectrum

This is what you asked for as "realtime, slower modes." It's a single speed parameter plus a handful of transport controls, not several separate rendering paths.

| Control | What it does | Notes |
|---|---|---|
| **Speed slider / hotkeys** | Milliseconds-per-move, from near-instant (~60 ms, "realtime-cool") to deliberately slow (~1.5 s, "watch exactly what's turning") | Already anticipated in `06-roadmap-bonus.md` Sprint 6 ("play / pause / step / speed controls") — this table just spells out what's on the panel |
| **Pause / resume** | Freezes `anim_update(dt)` at the current interpolation fraction | Trivial — just stop feeding it `dt` |
| **Step (single move)** | Advance exactly one queued move, then re-pause | Good for teaching / debugging a specific stage of the solve |
| **Scrub / timeline** | Jump straight to move N of the solution | Needs the *cumulative* logical state at move N, not just the move list — cheapest way is to replay moves 0..N instantly on a scratch copy of the logical cube (no animation) to get the state, then resume animating from there. Don't try to interpolate backwards through committed moves — see the reverse-playback trade-off below. |
| **Reverse playback** | Play the solution backwards, turn by turn | Two implementations: (1) *cheap* — apply the inverse of each move in reverse order as ordinary forward turns (an `R` undone is just `R'`); (2) *fancy* — actually animate the undo motion. (1) is enough; it looks identical on screen. |
| **Auto-loop demo** | On finish, re-scramble and re-solve automatically | Good for an unattended defence-room demo / a booth left running |

**Trade-off worth naming:** true scrub-by-dragging-a-timeline-handle (like a video editor) is a nice-to-have, not a need-to-have — it requires recomputing state on every drag frame. A simpler "jump to move N" text box or +10/-10 buttons gets 90% of the value for a fraction of the UI work. Recommend the simple version; only build the draggable timeline if time is genuinely spare.

## 3. Mode C — "use it like a real cube" (the new part)

This is the piece none of the existing docs design in depth — `06-roadmap-bonus.md` only mentions "scramble the cube via the renderer's manual-turn UI" once, as a *test gate*, not as a feature. Here's the actual design.

### 3.1 What it needs to feel right

A real cube has exactly one input vocabulary: you grab a layer and twist it. Two ways to offer that at the keyboard/mouse, not mutually exclusive — ship the first, add the second if time allows:

1. **Keyboard notation input (do this first — cheap, zero ambiguity).** Map `U R F D L B` (and shift for `'`, a digit for `2`) directly to `anim_push`. This alone delivers "free scrambling and free solving by hand" with an afternoon of work, and it can never be ambiguous the way mouse picking can. Ship this even if mouse-drag turning (below) never gets finished — it's the fallback that guarantees Mode C exists at all.
2. **Mouse click-and-drag turning (the impressive version).** Click a sticker, drag, the layer it belongs to turns to follow the drag, snapping to the nearest 90° on release. This is the one that actually *looks* like manipulating a physical cube and is worth the extra time if the schedule allows it.

### 3.2 How click-and-drag picking actually works

This is the one genuinely new technical piece, so it gets full detail:

1. **Ray from the mouse.** raylib's `GetMouseRay(mousePos, camera)` turns the 2D cursor position into a 3D ray from the camera through that pixel. This is **raycasting**: firing an imaginary line into the scene and asking what it hits first.
2. **Hit test against cubies.** Test that ray against each of the 26 cubies' **bounding box** (a simple box, `BoundingBox { min, max }`, standing in for the cubie's true shape because it's far cheaper to test a ray against a box than against the cubie's actual quads) using raylib's `GetRayCollisionBox`. Take the closest hit.
3. **Which face got clicked?** The collision result includes a hit point and a surface normal (which of the 6 directions the clicked face points in). That normal, together with the cubie's lattice position, tells you which of the 12 turnable layers the click *could* belong to (a click on a cubie's `+x` face could belong to the R-layer turn, or to a rotation around a different axis if you then drag sideways along that face — see next step).
4. **Drag direction decides the axis.** As the mouse moves while the button is held, project the mouse delta onto the clicked face's plane (a simple 2D vector in that face's local up/right directions). Whichever of the two in-plane directions the drag is closer to determines the rotation axis; the sign of the drag determines clockwise vs. counter-clockwise. This is the same trick every mouse-driven cube simulator (e.g. the well-known browser ones) uses — pick face, then let a same-plane drag choose the turn, rather than trying to infer intent from the initial click alone.
5. **Snap on release.** While dragging, rotate the selected layer's cubies to visually follow the mouse (partial rotation, not committed). On mouse-up, snap to the nearest legal quarter/half turn, animate the last bit with the normal `anim_*` easing, then commit to the logical state exactly like an autoplay move — **reuse the identical commit path Mode A uses.** This is important: Mode C must never have a separate "apply this turn" code path from Mode A/B. One `apply_move()` function, called from three different input sources (solver list, keyboard, drag gesture). That's what keeps the anti-cheat spirit from `01-requirements.md` intact — a cube scrambled by hand in Mode C and then handed to the solver is going through the exact same permutation code the mandatory binary uses, so there's no way for the two to quietly disagree.
6. **Reject a new drag mid-animation.** If a turn (from any source) is still animating, ignore new pick attempts until it commits. A physical cube doesn't let you grab a second layer mid-twist either.

### 3.3 Camera vs. turning — the input conflict to design around up front

Orbit-camera control (rotate your *view* of the whole cube) and layer-turning (rotate *part* of the cube) both want to consume left-mouse-drag. Pick one convention before writing input code, and put it in `DECISIONS.md` next to the cubie-indexing decision from Sprint 0:

| Convention | How it works | Trade-off |
|---|---|---|
| **Button split (recommended)** | Left-drag on a sticker = turn; right-drag (or middle-drag) anywhere = orbit camera | One button per intent, zero ambiguity, but trackpad users without a real right-click may find it awkward |
| **Modifier key** | Left-drag = orbit by default; hold Shift/Ctrl + left-drag = turn | Works fine on a trackpad; costs a moment of "which mode am I in" |
| **Empty-space rule** | Left-drag starting on a cubie = turn; left-drag starting on empty background = orbit | No modifier needed, matches how you'd naturally interact, but needs a hit-test on drag-start not just drag-position |

Recommend the **button split**: it needs no extra state, it's unambiguous, and school-machine mice all have a right button (only the trackpad-only case is awkward, and 42 clusters are desktop mice).

### 3.4 Extra Practice-mode features (once picking works, or even with keyboard-only)

- **Scramble button** — calls the exact same scramble generator built for the bonus list's "integrated scramble generator" item (`06-roadmap-bonus.md` Sprint 7 #2); Mode C is a second consumer of that same function, not a reason to build a separate one.
- **Timer** — start on first move after a scramble, stop on a solved-state check, display running. A speedcuber's inspection/timer convention if you want to lean into it (space-bar hold-to-ready is the standard one), but a plain start/stop is enough.
- **Move counter + TPS (turns per second)** — free once you're counting moves anyway; TPS is just moves divided by elapsed time.
- **Undo / redo** — every manual turn already goes through `apply_move()`; keep a small stack of the inverse moves for undo, and a redo stack that clears on any new manual move (standard undo/redo semantics).
- **"Solve for me" handoff** — one button that takes the current live logical state, feeds it to the solver exactly as the mandatory CLI path does, and switches the view into Mode A/B to play the returned solution back. This is the single most satisfying feature to demo at defence: scramble it by hand, badly, on purpose, in front of the evaluator, then hit solve.
- **Solved-state detection** — needed for the timer and for a "nice!" celebration cue; trivial, it's the same check used to terminate the solver's own search space at distance 0.

## 4. Making it *look* good — the visual-polish layer

Everything in this section is genuinely optional relative to sections 1-3 (which is the actual bonus requirement); treat it as a backlog to pull from as time allows, not a checklist to complete.

### 4.1 Lighting & shading

raylib ships a basic lighting shader example (diffuse + a bit of specular — **diffuse** is the flat, direction-dependent shading that makes a face look brighter when it faces the light; **specular** is the small bright highlight that makes a surface look glossy/plastic rather than chalky) that's a drop-in upgrade over raylib's default flat/unlit `DrawCube`. This is the single highest-value-per-hour visual change available: a plastic-looking, gently lit cube instead of a flat-shaded toy immediately reads as "someone cared about this."

| Option | Effort | Payoff |
|---|---|---|
| Default unlit `DrawCube` | Free (already the baseline) | Looks like a debug view |
| raylib's built-in lighting shader (1-2 lights) | ~half a day | Big visual jump for very little work — **do this one** |
| Custom shader (rim light, per-sticker gloss, soft shadows) | 1-2+ days | Diminishing returns unless someone on the team specifically wants the shader-writing experience |

### 4.2 Geometry & materials

- **Rounded-corner cubies** instead of sharp boxes — either a slightly beveled mesh or a cheap trick (a subtle rounded-corner texture/normal map on each sticker quad). Purely cosmetic, but it's the single detail that most makes a render "read" as a real cube photo rather than a coloured-boxes demo.
- **Sticker gaps / plastic frame** — leave a thin black or dark-grey border between stickers instead of coloured faces touching edge-to-edge. Cheap (just shrink each sticker quad slightly within its face) and very high perceived-realism payoff.
- **Colour themes / skins** — the classic 6-colour scheme by default, plus one or two alternate palettes (e.g. a colourblind-safe palette — swapping red/green for something more distinguishable — and maybe a fun palette purely for demo variety). Reads from the same logical-colour-per-facelet mapping, just through a different lookup table, so it's cheap once the base render works.

### 4.3 Camera & environment

- **Orbit camera with sane limits** — already covered by raylib's `Camera3D` + `UpdateCamera` (per `03-graphics.md`); just remember to clamp zoom/pitch so the user can't flip the camera upside-down or clip through the cube.
- **Camera presets** — a hotkey each for a straight-on 3-face iso view, a "watch this face turn" close-up that auto-follows the currently-turning layer, and a slow automatic orbit for an idle/demo screensaver mode.
- **Background** — a plain gradient or a simple skybox (a large background sphere/cube textured to look like a distant environment, giving the scene a sense of place) beats a flat colour for almost no extra cost.
- **Ground shadow / contact shadow** — a single soft blob shadow under the cube (fake, not a real shadow map) makes it look like it's sitting on a surface instead of floating. Cheap, worth it.

### 4.4 "Juice" — feedback effects

("Juice" is a common game-dev term for small feedback effects that make interactions feel satisfying even though they don't change the underlying logic — screen shake, particle bursts, sound, etc.)

- **Turn click/whoosh sound** on each committed move (raylib has trivial audio support: `InitAudioDevice` + `PlaySound`).
- **Solve-complete celebration** — a short particle burst, a color flash, or a camera flourish when solved-state is detected. Purely cosmetic, high demo value, an afternoon of work.
- **Move highlight** — briefly outline or glow the layer currently turning, so a viewer watching a fast autoplay can still track which face just moved.

### 4.5 HUD

Already scoped in `06-roadmap-bonus.md` Sprint 6 ("move counter, current move shown in notation, solution progress bar"). Extras worth adding to that baseline:

- **Phase label** — free once Sprint 7 item #4 ("humanly understandable substeps") exists; display the current Kociemba phase alongside the move.
- **Mode indicator** — which of A/B/C is currently active, plus the current speed setting.
- **Stats panel** — total move count, elapsed solve time, moves-per-second, all things you're already tracking for other features.
- **On-screen key hints** — a small always-visible legend ("drag a sticker to turn - right-drag to orbit - space to scramble") so Mode C is discoverable without a README.

## 5. Master extras backlog (MoSCoW)

**MoSCoW** is a simple prioritization label set — **M**ust have, **S**hould have, **C**ould have, **W**on't have (this time) — useful here because the list below is much longer than the time budget allows, and the point is to decide *now*, on paper, what gets cut first if Sprint 7 runs long, rather than deciding under deadline pressure.

| Item | Priority | Est. effort | Depends on |
|---|---|---|---|
| Autoplay of solver output (Mode A) | **Must** | done by Sprint 6 | -- |
| Play/pause/step/speed controls (Mode B) | **Must** | ~1 day | Mode A |
| Keyboard-driven manual turns (Mode C, keyboard) | **Must** | ~1 day | shared `apply_move()` path |
| Scramble button + timer + move counter | **Should** | ~1 day | Mode C keyboard |
| Basic lighting shader | **Should** | ~half day | none |
| Sticker gaps / plastic frame | **Should** | ~half day | none |
| Undo/redo in Mode C | **Should** | ~half day | Mode C keyboard |
| "Solve for me" handoff button | **Should** | ~half day | solver already callable in-process |
| Mouse click-and-drag turning (Mode C, mouse) | **Could** | ~2-3 days | keyboard version shipped first |
| Camera presets + idle auto-orbit | **Could** | ~half day | none |
| Colour themes / skins | **Could** | ~half day | none |
| Turn sound + solve-complete celebration | **Could** | ~half day | none |
| Rounded-corner cubie geometry | **Could** | ~1 day | none |
| Move/timeline scrubbing | **Could** | ~1 day | Mode B |
| Ground shadow / skybox | **Could** | ~half day | none |
| Custom shader work (rim light, soft shadows) | **Won't** (unless way ahead of schedule) | 1-2+ days | basic lighting shipped |
| Draggable timeline scrub handle | **Won't** | ~1-2 days | simple scrub shipped |
| VR/AR viewing | **Won't** | large, unscoped | out of scope entirely -- no defence value proportional to the cost |

Read down the "Should" row before touching anything in "Could" — those five items are the ones that make Mode C actually feel complete and make the render look considered, for about 3.5 days combined. Everything below that line is genuine bonus-on-the-bonus.

## 6. Architecture additions

One new module, one interface extension, one state machine — everything else in `04-architecture.md` stands as written.

```
render/
  ...
  input.c         NEW -- raycasting/picking, drag-to-turn, camera vs. turn
                  button-split, keyboard notation mapping. Depends only on
                  cube/ (for apply_move) and geometry.c's cubie lattice
                  positions -- same rule as everything else in render/:
                  never includes anything from parse/ or solve/, except a
                  single, explicit call into solve/ for the "solve for me"
                  handoff.
```

- **Mode state machine** — three states, `MODE_AUTOPLAY`, `MODE_PAUSED`, `MODE_MANUAL`. Legal transitions: autoplay <-> paused freely; manual -> autoplay only via the explicit "solve for me" action; autoplay -> manual is disallowed while a solve is queued (finish or cancel it first). Keep this as an explicit enum and switch statement, not a scatter of booleans — it's small enough that the state diagram fits in `DECISIONS.md` as a paragraph.
- **`anim_*` interface extension** — the existing signatures (`anim_push(move)`, `anim_update(dt)`, `anim_current_transform(cubie_id)`) don't need to change. What's new is *who calls `anim_push`*: the solver's move list (Mode A/B) or `input.c`'s picking/keyboard code (Mode C) — both funnel into the same call. Add one more: `anim_push_partial(cubie_group, axis, angle)` for the live, not-yet-committed rotation while a drag is in progress, which is visual-only and never touches logical state until release triggers the ordinary `anim_push`.
- **One `apply_move()` entry point.** All three input sources call the same function that both the mandatory binary's playback and the solver already use. This is the load-bearing design rule in this whole doc — it's what makes Mode C's manual scrambles trustworthy input to the solver, and it's a direct extension of the anti-cheat principle already stated in `01-requirements.md` and `04-architecture.md`.

## 7. Risks specific to this layer

| Risk | Severity | Mitigation |
|---|---|---|
| Mouse drag-picking (§3.2) turns out fiddly/buggy under time pressure | Medium | Keyboard notation input (§3.1.1) ships first and independently -- Mode C exists and is demoable even if drag-turning never lands |
| Camera-orbit and layer-turn input fight each other (both want left-drag) | Medium | Decide the button-split convention (§3.3) in Sprint 5, write it into `DECISIONS.md`, before any input code is written |
| Visual-polish backlog (§4-5) eats time that belongs to the mandatory/bonus core | Medium-High | The MoSCoW table in §5 is the pre-committed cut line -- if Sprint 7 runs long, stop at the bottom of "Should," not partway through "Could" |
| A second, divergent code path accidentally applies manual turns differently from solver turns | High (defeats the anti-cheat argument) | The single-`apply_move()` rule in §6; test it directly -- scramble via Mode C, feed the resulting state to the solver, verify it solves (this is already exactly the Sprint 6 gate in `06-roadmap-bonus.md`, just also exercised from the mouse/keyboard path, not only a fixed test scramble) |

## 8. Where this slots into the existing roadmap

No new sprints needed — this refines what's already inside Sprint 7 of `06-roadmap-bonus.md`, and reorders it now that "usable like a real cube" is an explicit goal rather than an implicit one:

- **Sprint 6** (unchanged) ships Mode A, which is `apply_move()` plus the solver-driven queue — build it so `input.c` can call the same function later, and this whole doc costs nothing extra later.
- **Sprint 7**, reordered: keyboard-driven Mode C and the play/pause/step/speed controls move to the *front* of Sprint 7 (they're cheap and they're the actual "real cube" ask), ahead of the originally-listed multi-algorithm/optimal-solver items, which stay valuable but are independent of this doc. Visual-polish "Should" items slot in alongside them since they're small and don't block anything. Mouse drag-turning and everything marked "Could" above only get attempted once the "Must"/"Should" rows are done and there's still runway before Sprint 8.

## 9. Open decisions to carry into `08-risks-and-open-decisions.md`

1. **Button-split vs. modifier-key vs. empty-space** for camera-orbit vs. layer-turn input (§3.3) — recommend button-split.
2. **Keyboard-only Mode C, or also mouse drag-turning?** — recommend building keyboard first unconditionally, treating mouse-drag as a stretch goal.
3. **Does Practice/Manual mode get demoed at defence, or kept as a private "we also built this" extra?** — it's a strong demo moment ("watch me mess it up, then watch it get solved"), but only worth rehearsing if it's reliable; decide once it exists.
