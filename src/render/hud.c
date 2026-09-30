#include <stdio.h>
#include "raylib.h"
#include "parse.h"
#include "render/hud.h"

# define HUD_MARGIN 16
# define HUD_FONT_SIZE 20
# define HUD_BAR_WIDTH 280
# define HUD_BAR_HEIGHT 14
# define HUD_PANEL_PAD 10
# define HUD_PANEL_ROUNDNESS 0.25f
# define HUD_PANEL_SEGMENTS 8
# define HUD_HINT_LINE_GAP 4
# define HUD_HINT_MARGIN (HUD_MARGIN * 2)
# define HUD_HINT_BLOCK_GAP (HUD_MARGIN * 2)

static const Color	HUD_TEXT = {225, 225, 230, 255};
static const Color	HUD_BAR_BG = {200, 200, 200, 255};
static const Color	HUD_BAR_FG = {0, 158, 96, 255};
static const Color	HUD_PANEL_BG = {10, 10, 14, 160};
static const Color	HUD_SCRAMBLE_FG = {235, 170, 60, 255};

static const char	*AUTOPLAY_HINT =
	"Space: pause  |  Right: step  |  Up/Down: speed  |  Esc: stop";
static const char	*AUTO_LOOP_TEXT = "AUTO-LOOP demo running  |  A: stop";

/// Cube state/turning hints (scramble/undo/redo/solve plus both turn
/// methods), drawn bottom-left, bottom-up (index 0 sits on the
/// window's very bottom row).
static const char	*const MOVE_HINTS[] = {
	"Left-drag a sticker: turn  |  circle a centre: turn face",
	"U R F D L B to turn  |  hold Shift: ccw  |  hold 2: double turn",
	"S: scramble  |  Z / Y: undo / redo  |  Enter: solve for me",
};

/// Everything else (camera, app settings), drawn bottom-right,
/// bottom-up, in its own block so it can never collide with
/// MOVE_HINTS above.
static const char	*const OTHER_HINTS[] = {
	"A: auto-loop  |  T: algorithm  |  K: puzzle",
	"5-8: views  |  P: palette  |  C: corners",
	"Right-drag/Arrows: orbit  |  wheel: zoom",
};

# define MOVE_HINT_COUNT (int)(sizeof(MOVE_HINTS) / sizeof(MOVE_HINTS[0]))
# define OTHER_HINT_COUNT (int)(sizeof(OTHER_HINTS) / sizeof(OTHER_HINTS[0]))

/// @brief Name shown in the HUD for the currently-selected solver —
///        same three choices as main.c's "-a" flag and render/app.c's T
///        key, always lowercase to match "-a"'s own argument spelling.
static const char	*algo_name(t_algo algo)
{
	if (algo == ALGO_THISTLETHWAITE)
		return ("thistlethwaite");
	if (algo == ALGO_LAYER)
		return ("layer");
	return ("kociemba");
}

/// @brief Name shown in the HUD for the currently-selected puzzle — same
///        spelling as main.c's "-p 2x2x2" argument.
static const char	*puzzle_name(t_puzzle puzzle)
{
	if (puzzle == PUZZLE_2X2X2)
		return ("2x2x2");
	return ("3x3x3");
}

/// @brief Draws a semi-transparent dark rounded panel behind a HUD text
///        block so light text stays legible over the 3D scene.
static void	draw_panel(int x, int y, int width, int height)
{
	DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)width,
		(float)height}, HUD_PANEL_ROUNDNESS, HUD_PANEL_SEGMENTS,
		HUD_PANEL_BG);
}

/// @brief Draws the autoplay-only part: play/pause, speed, a
///        scrambling/solving phase label, current move, and a
///        "done/total" progress bar scoped to whichever phase is
///        currently queued (never a stale total from the other phase).
static void	draw_autoplay(const t_anim_state *anim, int total_moves,
	bool auto_loop, bool scrambling, t_algo algo, t_puzzle puzzle)
{
	char		status_line[96];
	char		move_line[32];
	char		count_line[32];
	char		move_text[4];
	const char	*phase_label;
	Color		phase_color;
	int			done;
	int			y;
	int			top_w;
	int			top_bottom;
	int			w;
	int			bottom_y;
	int			loop_w;
	int			loop_x;

	y = HUD_MARGIN;
	if (puzzle == PUZZLE_2X2X2)
		snprintf(status_line, sizeof(status_line),
			"%s  |  speed %.0f deg/s  |  puzzle: %s",
			anim->paused ? "PAUSED" : "PLAYING",
			(double)anim->speed_deg_per_sec, puzzle_name(puzzle));
	else
		snprintf(status_line, sizeof(status_line),
			"%s  |  speed %.0f deg/s  |  algo: %s",
			anim->paused ? "PAUSED" : "PLAYING",
			(double)anim->speed_deg_per_sec, algo_name(algo));
	top_w = MeasureText(status_line, HUD_FONT_SIZE);
	phase_label = scrambling ? "SCRAMBLING" : "SOLVING";
	phase_color = scrambling ? HUD_SCRAMBLE_FG : HUD_BAR_FG;
	w = MeasureText(phase_label, HUD_FONT_SIZE);
	if (w > top_w)
		top_w = w;
	top_bottom = y + HUD_FONT_SIZE + 6 + HUD_FONT_SIZE;
	move_line[0] = '\0';
	done = 0;
	if (anim->active)
	{
		format_moves(&anim->current, 1, move_text);
		snprintf(move_line, sizeof(move_line), "move: %s", move_text);
		w = MeasureText(move_line, HUD_FONT_SIZE);
		if (w > top_w)
			top_w = w;
		top_bottom += 6 + HUD_FONT_SIZE;
	}
	count_line[0] = '\0';
	if (total_moves > 0)
	{
		done = total_moves - (int)anim_pending_count(anim);
		snprintf(count_line, sizeof(count_line), "%d / %d", done,
			total_moves);
		w = MeasureText(count_line, HUD_FONT_SIZE);
		if (w > top_w)
			top_w = w;
		if (HUD_BAR_WIDTH > top_w)
			top_w = HUD_BAR_WIDTH;
		top_bottom += 6 + HUD_FONT_SIZE + 4 + HUD_BAR_HEIGHT;
	}
	draw_panel(HUD_MARGIN - HUD_PANEL_PAD, HUD_MARGIN - HUD_PANEL_PAD,
		top_w + HUD_PANEL_PAD * 2,
		top_bottom - HUD_MARGIN + HUD_PANEL_PAD * 2);
	DrawText(status_line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
	y += HUD_FONT_SIZE + 6;
	DrawText(phase_label, HUD_MARGIN, y, HUD_FONT_SIZE, phase_color);
	y += HUD_FONT_SIZE + 6;
	if (anim->active)
	{
		DrawText(move_line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
		y += HUD_FONT_SIZE + 6;
	}
	if (total_moves > 0)
	{
		DrawText(count_line, HUD_MARGIN, y, HUD_FONT_SIZE, HUD_TEXT);
		DrawRectangle(HUD_MARGIN, y + HUD_FONT_SIZE + 4, HUD_BAR_WIDTH,
			HUD_BAR_HEIGHT, HUD_BAR_BG);
		DrawRectangle(HUD_MARGIN, y + HUD_FONT_SIZE + 4,
			HUD_BAR_WIDTH * done / total_moves, HUD_BAR_HEIGHT, HUD_BAR_FG);
	}
	if (auto_loop)
	{
		loop_w = MeasureText(AUTO_LOOP_TEXT, HUD_FONT_SIZE);
		loop_x = GetScreenWidth() - HUD_MARGIN - loop_w;
		draw_panel(loop_x - HUD_PANEL_PAD, HUD_MARGIN - HUD_PANEL_PAD,
			loop_w + HUD_PANEL_PAD * 2, HUD_FONT_SIZE + HUD_PANEL_PAD * 2);
		DrawText(AUTO_LOOP_TEXT, loop_x, HUD_MARGIN, HUD_FONT_SIZE,
			HUD_BAR_FG);
	}
	bottom_y = GetScreenHeight() - HUD_MARGIN - HUD_FONT_SIZE;
	draw_panel(HUD_MARGIN - HUD_PANEL_PAD, bottom_y - HUD_PANEL_PAD,
		MeasureText(AUTOPLAY_HINT, HUD_FONT_SIZE) + HUD_PANEL_PAD * 2,
		HUD_FONT_SIZE + HUD_PANEL_PAD * 2);
	DrawText(AUTOPLAY_HINT, HUD_MARGIN, bottom_y, HUD_FONT_SIZE, HUD_TEXT);
}

/// @brief Widest of `count` lines, at the HUD's own font size.
static int	hint_block_width(const char *const *lines, int count)
{
	int	block_w;
	int	w;
	int	i;

	block_w = 0;
	i = 0;
	while (i < count)
	{
		w = MeasureText(lines[i], HUD_FONT_SIZE);
		if (w > block_w)
			block_w = w;
		i++;
	}
	return (block_w);
}

/// @brief Draws one hint block, bottom-up, inset from the window's
///        bottom-left corner (right_align false) or bottom-right corner
///        (right_align true) by HUD_HINT_MARGIN: lines[0] sits on the
///        very bottom row, each following line one row above it. Used
///        to keep the cube-turning hints and the camera/session hints
///        in separate corners so neither list overlaps or runs off the
///        window -- see hud_min_window_width(), which sizes the window
///        so the two blocks always have room to sit side by side.
static void	draw_hint_block(const char *const *lines, int count,
	bool right_align)
{
	int	block_w;
	int	x;
	int	bottom_y;
	int	y;
	int	i;

	block_w = hint_block_width(lines, count);
	bottom_y = GetScreenHeight() - HUD_HINT_MARGIN - HUD_FONT_SIZE;
	if (right_align)
		x = GetScreenWidth() - HUD_HINT_MARGIN - block_w;
	else
		x = HUD_HINT_MARGIN;
	draw_panel(x - HUD_PANEL_PAD,
		bottom_y - (count - 1) * (HUD_FONT_SIZE + HUD_HINT_LINE_GAP)
			- HUD_PANEL_PAD, block_w + HUD_PANEL_PAD * 2,
		count * HUD_FONT_SIZE + (count - 1) * HUD_HINT_LINE_GAP
			+ HUD_PANEL_PAD * 2);
	i = 0;
	while (i < count)
	{
		y = bottom_y - i * (HUD_FONT_SIZE + HUD_HINT_LINE_GAP);
		if (right_align)
			DrawText(lines[i], x + block_w
				- MeasureText(lines[i], HUD_FONT_SIZE), y, HUD_FONT_SIZE,
				HUD_TEXT);
		else
			DrawText(lines[i], x, y, HUD_FONT_SIZE, HUD_TEXT);
		i++;
	}
}

/// @brief Draws the manual-only part: mode label and the practice-
///        session timer/move-counter/TPS line top-left, cube-turning
///        hints stacked bottom-left, and the remaining (camera/session)
///        hints stacked bottom-right.
static void	draw_manual(double elapsed_sec, int move_count, t_algo algo,
	t_puzzle puzzle)
{
	char	line[96];
	char	label[48];
	double	tps;
	int		top_w;
	int		w;

	tps = 0.0;
	if (elapsed_sec > 0.0)
		tps = (double)move_count / elapsed_sec;
	snprintf(line, sizeof(line), "time %.1fs  |  moves %d  |  tps %.2f",
		elapsed_sec, move_count, tps);
	if (puzzle == PUZZLE_2X2X2)
		snprintf(label, sizeof(label), "MANUAL -- %s", puzzle_name(puzzle));
	else
		snprintf(label, sizeof(label), "MANUAL -- %s -- %s",
			puzzle_name(puzzle), algo_name(algo));
	top_w = MeasureText(label, HUD_FONT_SIZE);
	w = MeasureText(line, HUD_FONT_SIZE);
	if (w > top_w)
		top_w = w;
	draw_panel(HUD_MARGIN - HUD_PANEL_PAD, HUD_MARGIN - HUD_PANEL_PAD,
		top_w + HUD_PANEL_PAD * 2,
		HUD_FONT_SIZE + 6 + HUD_FONT_SIZE + HUD_PANEL_PAD * 2);
	DrawText(label, HUD_MARGIN, HUD_MARGIN, HUD_FONT_SIZE, HUD_TEXT);
	DrawText(line, HUD_MARGIN, HUD_MARGIN + HUD_FONT_SIZE + 6,
		HUD_FONT_SIZE, HUD_TEXT);
	draw_hint_block(MOVE_HINTS, MOVE_HINT_COUNT, false);
	draw_hint_block(OTHER_HINTS, OTHER_HINT_COUNT, true);
}

void	hud_draw(const t_anim_state *anim, t_render_mode mode,
	int total_moves, double elapsed_sec, int move_count, bool auto_loop,
	bool scrambling, t_algo algo, t_puzzle puzzle)
{
	if (mode == MODE_AUTOPLAY)
		draw_autoplay(anim, total_moves, auto_loop, scrambling, algo,
			puzzle);
	else
		draw_manual(elapsed_sec, move_count, algo, puzzle);
}

int	hud_min_window_width(void)
{
	int	left_w;
	int	right_w;

	left_w = hint_block_width(MOVE_HINTS, MOVE_HINT_COUNT);
	right_w = hint_block_width(OTHER_HINTS, OTHER_HINT_COUNT);
	return (HUD_HINT_MARGIN * 2 + HUD_PANEL_PAD * 4 + HUD_HINT_BLOCK_GAP
		+ left_w + right_w);
}
