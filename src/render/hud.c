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

static const Color	HUD_TEXT = {225, 225, 230, 255};
static const Color	HUD_BAR_BG = {200, 200, 200, 255};
static const Color	HUD_BAR_FG = {0, 158, 96, 255};
static const Color	HUD_PANEL_BG = {10, 10, 14, 160};
static const Color	HUD_SCRAMBLE_FG = {235, 170, 60, 255};

static const char	*AUTOPLAY_HINT =
	"Space: pause  |  Right: step  |  Up/Down: speed  |  Esc: stop";
static const char	*AUTO_LOOP_TEXT = "AUTO-LOOP demo running  |  A: stop";
static const char	*MANUAL_HINT_1 =
	"S: scramble  |  Z / Y: undo / redo  |  Enter: solve for me";
static const char	*MANUAL_HINT_2 =
	"U R F D L B to turn  |  hold Shift: ccw  |  hold 2: double turn";
static const char	*MANUAL_HINT_3 =
	"Left-drag a sticker: turn  |  circle a centre: turn face  |  "
	"[ ]: scrub  |  \\: reverse solve";
static const char	*MANUAL_HINT_4 =
	"Right-drag/Arrows: orbit  |  wheel: zoom  |  5-8: views  |  "
	"P: palette  |  C: corners  |  A: auto-loop  |  T: algorithm  |  "
	"K: puzzle";

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

/// @brief Draws the manual-only part: mode label, the practice-session
///        timer/move-counter/TPS line, and keybinding hints, bottom-up.
static void	draw_manual(double elapsed_sec, int move_count, t_algo algo,
	t_puzzle puzzle)
{
	char	line[96];
	char	label[48];
	double	tps;
	int		top_w;
	int		w;
	int		hint_w;
	int		line1_y;
	int		line2_y;
	int		line3_y;
	int		line4_y;

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
	line4_y = GetScreenHeight() - HUD_MARGIN - HUD_FONT_SIZE;
	line3_y = line4_y - (HUD_FONT_SIZE + 4);
	line2_y = line3_y - (HUD_FONT_SIZE + 4);
	line1_y = line2_y - (HUD_FONT_SIZE + 4);
	hint_w = MeasureText(MANUAL_HINT_4, HUD_FONT_SIZE);
	w = MeasureText(MANUAL_HINT_3, HUD_FONT_SIZE);
	if (w > hint_w)
		hint_w = w;
	w = MeasureText(MANUAL_HINT_2, HUD_FONT_SIZE);
	if (w > hint_w)
		hint_w = w;
	w = MeasureText(MANUAL_HINT_1, HUD_FONT_SIZE);
	if (w > hint_w)
		hint_w = w;
	draw_panel(HUD_MARGIN - HUD_PANEL_PAD, line1_y - HUD_PANEL_PAD,
		hint_w + HUD_PANEL_PAD * 2,
		(line4_y + HUD_FONT_SIZE) - line1_y + HUD_PANEL_PAD * 2);
	DrawText(MANUAL_HINT_4, HUD_MARGIN, line4_y, HUD_FONT_SIZE, HUD_TEXT);
	DrawText(MANUAL_HINT_3, HUD_MARGIN, line3_y, HUD_FONT_SIZE, HUD_TEXT);
	DrawText(MANUAL_HINT_2, HUD_MARGIN, line2_y, HUD_FONT_SIZE, HUD_TEXT);
	DrawText(MANUAL_HINT_1, HUD_MARGIN, line1_y, HUD_FONT_SIZE, HUD_TEXT);
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
