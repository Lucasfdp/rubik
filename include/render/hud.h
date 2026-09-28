#ifndef RENDER_HUD_H
# define RENDER_HUD_H

# include <stdbool.h>
# include "render/anim.h"
# include "render/input.h"

/// @brief Draws the 2D HUD overlay: current mode, play/pause state,
///        speed, and (during autoplay) a "move done/total" progress
///        bar; in manual mode, the practice-session timer/move-counter
///        and the full keybinding hints instead. Must be called
///        OUTSIDE BeginMode3D()/EndMode3D() — this is screen-space 2D,
///        not scene geometry.
///
/// @param anim        Current playback state.
/// @param mode        Current top-level mode (autoplay vs manual).
/// @param total_moves Length of the queued solution/scramble (0 outside
///                    autoplay, or when Mode C hasn't queued one).
/// @param elapsed_sec Manual-mode practice timer, in seconds (0 until
///                    the first manual move; frozen once solved).
/// @param move_count  Manual-mode move counter since the last scramble.
/// @param auto_loop   True while Phase 7 §9.6's auto-loop demo is on —
///                    shown so it's obvious the app is driving itself
///                    and L (not just Esc) turns it back off.
void	hud_draw(const t_anim_state *anim, t_render_mode mode,
			int total_moves, double elapsed_sec, int move_count,
			bool auto_loop);

#endif
