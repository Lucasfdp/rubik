#include "ida.h"

void	ida_phase1_setup(t_ida_phase *phase, const t_move_tables *moves,
	const t_prune_tables *prune)
{
	phase->move_table[0] = moves->twist;
	phase->move_table[1] = moves->flip;
	phase->move_table[2] = moves->slice;
	phase->prune_xz = &prune->twist_slice;
	phase->prune_yz = &prune->flip_slice;
	phase->moves = MOVES_ALL;
	phase->max_depth = IDA_PHASE1_CAP;
}

void	ida_phase2_setup(t_ida_phase *phase, const t_move_tables *moves,
	const t_prune_tables *prune)
{
	phase->move_table[0] = moves->cperm;
	phase->move_table[1] = moves->eperm;
	phase->move_table[2] = moves->sperm;
	phase->prune_xz = &prune->cperm_sperm;
	phase->prune_yz = &prune->eperm_sperm;
	phase->moves = MOVES_PHASE2;
	phase->max_depth = IDA_PHASE2_CAP;
}

void	ida_phase1_start(const t_cube *cube, uint16_t start[3])
{
	start[0] = encode_twist(cube);
	start[1] = encode_flip(cube);
	start[2] = encode_slice(cube);
}

void	ida_phase2_start(const t_cube *cube, uint16_t start[3])
{
	start[0] = encode_cperm(cube);
	start[1] = encode_eperm(cube);
	start[2] = encode_sperm(cube);
}

int	ida_estimate(const t_ida_phase *phase, const uint16_t coord[3])
{
	int	a;
	int	b;

	a = prune_dist(phase->prune_xz, coord[0], coord[2]);
	b = prune_dist(phase->prune_yz, coord[1], coord[2]);
	if (a > b)
		return (a);
	return (b);
}

/// Faces are ordered U R F D L B, so the opposite of face f is (f + 3) % 6.
bool	ida_move_is_redundant(int last, t_move move)
{
	int	face;
	int	last_face;

	if (last < 0)
		return (false);
	face = (int)move / 3;
	last_face = last / 3;
	if (face == last_face)
		return (true);
	return (face == (last_face + 3) % 6 && face < last_face);
}

/// @brief One branch of the search: is the goal reachable from `coord`
///        within the current budget?
///
/// First the estimate: 0 means this state is the goal (done); depth +
/// estimate over the budget means give up on this branch. Otherwise try
/// every allowed, non-redundant move, looking up the next value of each
/// coordinate. path[depth] is only written when depth < bound (estimate
/// is at least 1 there and depth + estimate fits in the bound), so the
/// buffer of IDA_MAX_DEPTH moves can never overflow.
///
/// @param depth Moves used so far on this branch.
/// @param last  The move just made, or -1 at the start.
static bool	search(t_ida_ctx *ctx, const uint16_t coord[3], int depth,
	int last)
{
	uint16_t	next[3];
	int			left;
	int			move;

	left = ida_estimate(ctx->phase, coord);
	if (left == 0)
	{
		ctx->length = depth;
		return (true);
	}
	if (depth + left > ctx->bound)
		return (false);
	move = -1;
	while (++move < MOVE_COUNT)
	{
		if (!move_in_mask(ctx->phase->moves, (t_move)move)
			|| ida_move_is_redundant(last, (t_move)move))
			continue ;
		next[0] = move_table_next(ctx->phase->move_table[0], coord[0],
				(t_move)move);
		next[1] = move_table_next(ctx->phase->move_table[1], coord[1],
				(t_move)move);
		next[2] = move_table_next(ctx->phase->move_table[2], coord[2],
				(t_move)move);
		ctx->path[depth] = (t_move)move;
		if (search(ctx, next, depth + 1, move))
			return (true);
	}
	return (false);
}

int	ida_search(const t_ida_phase *phase, const uint16_t start[3],
	t_move *path)
{
	t_ida_ctx	ctx;
	int			cap;

	ctx.phase = phase;
	ctx.path = path;
	ctx.length = -1;
	cap = phase->max_depth;
	if (cap > IDA_MAX_DEPTH)
		cap = IDA_MAX_DEPTH;
	ctx.bound = ida_estimate(phase, start);
	while (ctx.bound <= cap)
	{
		if (search(&ctx, start, 0, -1))
			return (ctx.length);
		ctx.bound++;
	}
	return (-1);
}
