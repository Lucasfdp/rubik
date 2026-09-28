#include <stdlib.h>
#include "raylib.h"
#include "render/fx.h"

# define TURN_SOUND_PATH "assets/sfx/turn.wav"
# define PARTICLE_LIFE_SEC 0.9f
# define PARTICLE_SPEED 3.5f
# define PARTICLE_UPWARD_BIAS 1.5f
# define PARTICLE_RADIUS 0.08f

static const Color	PARTICLE_COLOR = {255, 213, 0, 255};

/// @brief One pseudo-random float in [-1, 1], seeded from *seed. Kept
///        local rather than reusing scramble.c's rand_r() convention
///        elsewhere: this is cosmetic jitter, not anything that needs to
///        replay the same sequence as any other module.
static float	jitter(unsigned int *seed)
{
	return ((float)(rand_r(seed) % 2001 - 1000) / 1000.0f);
}

t_fx_state	fx_load(void)
{
	t_fx_state	fx;
	int			i;

	InitAudioDevice();
	fx.audio_ready = IsAudioDeviceReady();
	fx.sound_loaded = false;
	if (fx.audio_ready && FileExists(TURN_SOUND_PATH))
	{
		fx.turn_sound = LoadSound(TURN_SOUND_PATH);
		fx.sound_loaded = IsSoundValid(fx.turn_sound);
	}
	i = 0;
	while (i < FX_PARTICLE_CAP)
	{
		fx.particles[i].life = 0.0f;
		i++;
	}
	fx.rng_seed = 1;
	return (fx);
}

void	fx_play_turn(t_fx_state *fx)
{
	if (fx->sound_loaded)
		PlaySound(fx->turn_sound);
}

void	fx_spawn_celebration(t_fx_state *fx)
{
	int	i;

	i = 0;
	while (i < FX_PARTICLE_CAP)
	{
		fx->particles[i].pos = (Vector3){0.0f, 0.0f, 0.0f};
		fx->particles[i].vel = (Vector3){
			jitter(&fx->rng_seed) * PARTICLE_SPEED,
			jitter(&fx->rng_seed) * PARTICLE_SPEED + PARTICLE_UPWARD_BIAS,
			jitter(&fx->rng_seed) * PARTICLE_SPEED};
		fx->particles[i].life = PARTICLE_LIFE_SEC;
		i++;
	}
}

void	fx_update_and_draw(t_fx_state *fx, float dt)
{
	int		i;
	float	shrink;

	i = 0;
	while (i < FX_PARTICLE_CAP)
	{
		if (fx->particles[i].life > 0.0f)
		{
			fx->particles[i].pos.x += fx->particles[i].vel.x * dt;
			fx->particles[i].pos.y += fx->particles[i].vel.y * dt;
			fx->particles[i].pos.z += fx->particles[i].vel.z * dt;
			fx->particles[i].life -= dt;
			shrink = fx->particles[i].life / PARTICLE_LIFE_SEC;
			if (shrink < 0.0f)
				shrink = 0.0f;
			DrawSphere(fx->particles[i].pos, PARTICLE_RADIUS * shrink,
				PARTICLE_COLOR);
		}
		i++;
	}
}

void	fx_unload(t_fx_state *fx)
{
	if (fx->sound_loaded)
		UnloadSound(fx->turn_sound);
	if (fx->audio_ready)
		CloseAudioDevice();
}
