#ifndef RENDER_FX_H
# define RENDER_FX_H

# include <stdbool.h>
# include "raylib.h"

/// Fixed-size celebration burst (Phase 7 §9.3) — sized generously for a
/// short one-off effect, no growth needed.
# define FX_PARTICLE_CAP 64

typedef struct s_fx_particle
{
	Vector3	pos;
	Vector3	vel;
	float	life;
}	t_fx_particle;

/// Turn-sound + solve-complete celebration state ("juice", Phase 7
/// §9.3). Threaded through explicitly (a pointer in t_app, same as every
/// other render/*.c module's state) rather than kept as a hidden static,
/// so audio device lifecycle stays as visible as the lighting shader's.
typedef struct s_fx_state
{
	bool			audio_ready;
	Sound			turn_sound;
	bool			sound_loaded;
	t_fx_particle	particles[FX_PARTICLE_CAP];
	unsigned int	rng_seed;
}	t_fx_state;

/// @brief Opens the audio device and loads the turn sound. Call once
///        after InitWindow(). Safe either way: if the audio device or
///        the sound file (assets/sfx/turn.wav) is unavailable, the
///        corresponding ->*_loaded/->audio_ready flag is just false and
///        every other fx_* call below silently no-ops instead of
///        crashing or blocking the build.
t_fx_state	fx_load(void);

/// @brief Plays the turn sound, if loaded. Call from anim_update()'s
///        commit branch (once per committed move).
void	fx_play_turn(t_fx_state *fx);

/// @brief Spawns the solve-complete celebration burst: every particle
///        reset to the cube's centre with a random outward velocity.
///        Call once, the instant cube_is_solved() flips true.
void	fx_spawn_celebration(t_fx_state *fx);

/// @brief Advances every live particle by dt and draws it as a shrinking
///        sphere. Call once per frame, inside BeginMode3D()/EndMode3D()
///        (same contract as draw_scene()).
void	fx_update_and_draw(t_fx_state *fx, float dt);

/// @brief Unloads the sound and closes the audio device (no-ops if
///        either was never opened). Call once after the render loop
///        ends, before CloseWindow().
void	fx_unload(t_fx_state *fx);

#endif
