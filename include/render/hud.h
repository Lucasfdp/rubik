#ifndef RENDER_HUD_H
# define RENDER_HUD_H

# include <stdbool.h>
# include "algo.h"
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
/// @param scrambling  True while the queued playback is a scramble,
///                    false while it's a solve — used to label the
///                    autoplay HUD "SCRAMBLING" vs "SOLVING".
/// @param algo        Which solver is currently selected — shown in both
///                    HUD modes so it's always visible which one T will
///                    affect next and which one just produced the
///                    on-screen solve/scramble. Only meaningful for
///                    puzzle == PUZZLE_3X3X3 — a 2x2x2 has exactly one
///                    solver, so hud.c shows the puzzle name in its
///                    place instead of a solver name that would never
///                    change.
/// @param puzzle      Which puzzle (algo.h's t_puzzle) is currently
///                    shown/solved — shown in both HUD modes, same as
///                    algo, so it's always visible which view K will
///                    switch away from.
void	hud_draw(const t_anim_state *anim, t_render_mode mode,
			int total_moves, double elapsed_sec, int move_count,
			bool auto_loop, bool scrambling, t_algo algo, t_puzzle puzzle);

/// @brief The narrowest window width at which the manual-mode HUD's two
///        bottom hint blocks (cube-turning, bottom-left; everything
///        else, bottom-right — see hud.c's draw_manual()) can sit side
///        by side without colliding, including a comfortable gap
///        between them. Must be called after InitWindow() (raylib's
///        default font, which MeasureText() uses, only exists once the
///        window is up). render/app.c's render_run() uses this both to
///        set SetWindowMinSize() and, if the compiled-in default is
///        already too narrow, to widen the window up front.
int	hud_min_window_width(void);

#endif
