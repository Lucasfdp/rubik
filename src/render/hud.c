#include <stdio.h>
#include "raylib.h"
#include "parse.h"
#include "render/hud.h"

# define HUD_MARGIN 16
# define HUD_FONT_SIZE 20
# define HUD_BAR_WIDTH 280
# define HUD_BAR_HEIGHT 14

static const Color	HUD_TEXT = {20, 20, 20, 255};
static const Color	HUD_BAR_BG = {200, 200, 200, 255};
static const Color	HUD_BAR_FG = {0, 158, 96, 255};

/// @brief Draws the autoplay-only part: play/pause, speed, current move,
///        and a "done/total" progress bar.
static void	draw_autoplay(const t_anim_state *anim, int total_moves)
{
	char	line[96];
	char	move_text[4];
	int		done;
	int		y;

	y = HUD_MARGIN;
	snprintf(line, sizeof(line), "%s  |  speed %.0f deg/s",
		anim->paused ? "PAUSED" : "PLAYING", (double)anim->speed_deg_per_sec);
	DrawText(line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
	y += HUD_FONT_SIZE + 6;
	if (anim->active)
	{
		format_moves(&anim->current, 1, move_text);
		snprintf(line, sizeof(line), "move: %s", move_text);
		DrawText(line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
		y += HUD_FONT_SIZE + 6;
	}
	if (total_moves > 0)
	{
		done = total_moves - (int)anim_pending_count(anim);
		snprintf(line, sizeof(line), "%d / %d", done, total_moves);
		DrawText(line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
		DrawRectangle(HUD_MARGIN, y + HUD_FONT_SIZE + 4, HUD_BAR_WIDTH,
			HUD_BAR_HEIGHT, HUD_BAR_BG);
		DrawRectangle(HUD_MARGIN, y + HUD_FONT_SIZE + 4,
			HUD_BAR_WIDTH * done / total_moves, HUD_BAR_HEIGHT, HUD_BAR_FG);
	}
	DrawText("Space: pause  |  Right: step  |  Up/Down: speed  |  Esc: stop",
		HUD_MARGIN, GetScreenHeight() - HUD_MARGIN - HUD_FONT_SIZE,
		HUD_FONT_SIZE, HUD_TEXT);
}

/// @brief Draws the manual-only part: mode label, the practice-session
///        timer/move-counter/TPS line, and three lines of keybinding
///        hints (turning, extras, camera), bottom-up.
static void	draw_manual(double elapsed_sec, int move_count)
{
	char	line[96];
	double	tps;
	int		bottom;

	DrawText("MANUAL", HUD_MARGIN, HUD_MARGIN, HUD_FONT_SIZE, HUD_TEXT);
	tps = 0.0;
	if (elapsed_sec > 0.0)
		tps = (double)move_count / elapsed_sec;
	snprintf(line, sizeof(line), "time %.1fs  |  moves %d  |  tps %.2f",
		elapsed_sec, move_count, tps);
	DrawText(line, HUD_MARGIN, HUD_MARGIN + HUD_FONT_SIZE + 6,
		HUD_FONT_SIZE, HUD_TEXT);
	bottom = GetScreenHeight() - HUD_MARGIN - HUD_FONT_SIZE;
	DrawText("Right-drag: orbit camera  |  wheel: zoom",
		HUD_MARGIN, bottom, HUD_FONT_SIZE, HUD_TEXT);
	bottom -= HUD_FONT_SIZE + 4;
	DrawText(
		"U R F D L B to turn  |  hold Shift: ccw  |  hold 2: double turn",
		HUD_MARGIN, bottom, HUD_FONT_SIZE, HUD_TEXT);
	bottom -= HUD_FONT_SIZE + 4;
	DrawText("S: scramble  |  Z / Y: undo / redo  |  Enter: solve for me",
		HUD_MARGIN, bottom, HUD_FONT_SIZE, HUD_TEXT);
}

void	hud_draw(const t_anim_state *anim, t_render_mode mode,
	int total_moves, double elapsed_sec, int move_count)
{
	if (mode == MODE_AUTOPLAY)
		draw_autoplay(anim, total_moves);
	else
		draw_manual(elapsed_sec, move_count);
}
