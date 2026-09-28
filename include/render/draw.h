#ifndef RENDER_DRAW_H
# define RENDER_DRAW_H

# include <stdbool.h>
# include "raylib.h"
# include "render/geometry.h"

/// Basic-lighting shader state, loaded once and threaded through to
/// draw_scene() every frame — no mutable module-level state, same as
/// every other render/*.c file. ->loaded is false whenever the shader
/// files for the running GL context are missing or fail to compile;
/// draw_scene() then falls back to flat, unlit cubes (Phase 0-4's look)
/// instead of the build or the run breaking.
typedef struct s_render_lighting
{
	Shader	shader;
	bool	loaded;
}	t_render_lighting;

/// @brief Loads the basic-lighting shader and sets up two fixed point
///        lights plus a soft ambient term. Call once, after InitWindow()
///        (rlGetVersion() only reports a real answer once a GL context
///        exists), before the render loop starts.
///
///        The GLSL variant is picked at RUNTIME from rlGetVersion(), not
///        hardcoded: this project's Makefile builds raylib two different
///        ways depending on the machine (see the GRAPHICS_API_OPENGL_21
///        comment there) — the vendored Docker/XQuartz build pins GL 2.1
///        (GLSL 120), while a native macOS build via pkg-config/Homebrew
///        raylib gets a GL 3.3+ core context (GLSL 330) — and asking the
///        actual running context beats assuming either one, since the
///        same binary source has to work under both.
///
///        Paths are relative to the working directory the binary is run
///        from (repo root, same as every other relative path this
///        project uses): assets/shaders/glsl120/lighting.{vs,fs} or
///        assets/shaders/glsl330/lighting.{vs,fs}.
t_render_lighting	draw_lighting_load(void);

/// @brief Refreshes the shader's view-position uniform from the current
///        camera (needed for its specular term). Call once per frame,
///        before draw_scene(). No-op if ->loaded is false.
void	draw_lighting_update_camera(t_render_lighting *lighting,
			Camera3D camera);

/// @brief Unloads the shader. Call once after the render loop ends,
///        before CloseWindow(). No-op if ->loaded is false.
void	draw_lighting_unload(t_render_lighting *lighting);

/// @brief Draws every one of the 26 cubies at its fixed lattice position,
///        plastic body plus one coloured sticker quad per populated
///        face[] direction, lit by `lighting` when it is loaded. Must be
///        called between BeginMode3D()/EndMode3D() (it draws 3D
///        geometry, not a HUD overlay).
///
/// @param scene    The 26 cubies to draw.
/// @param turn     When turn->active is true, every cubie whose fixed
///                 slot sits in turn->axis/turn->layer is drawn rotated
///                 by an extra turn->angle_deg around that world axis
///                 before its normal translation — the live, uncommitted
///                 spin of a move in progress. Pass NULL (or
///                 ->active == false) to draw everything at rest.
/// @param lighting Current lighting state from draw_lighting_load().
void	draw_scene(const t_render_scene *scene, const t_active_turn *turn,
			const t_render_lighting *lighting);

#endif
