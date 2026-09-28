#include "render/history.h"

void	history_init(t_history *history)
{
	history->undo_top = 0;
	history->redo_top = 0;
}

t_move	move_inverse(t_move move)
{
	return ((t_move)((move / 3) * 3 + (2 - move % 3)));
}

void	history_record(t_history *history, t_move applied)
{
	if (history->undo_top < HISTORY_CAP)
	{
		history->undo_stack[history->undo_top] = applied;
		history->undo_top++;
	}
	history->redo_top = 0;
}

bool	history_undo(t_history *history, t_move *out)
{
	t_move	applied;

	if (history->undo_top == 0)
		return (false);
	history->undo_top--;
	applied = history->undo_stack[history->undo_top];
	if (history->redo_top < HISTORY_CAP)
	{
		history->redo_stack[history->redo_top] = applied;
		history->redo_top++;
	}
	*out = move_inverse(applied);
	return (true);
}

bool	history_redo(t_history *history, t_move *out)
{
	t_move	applied;

	if (history->redo_top == 0)
		return (false);
	history->redo_top--;
	applied = history->redo_stack[history->redo_top];
	if (history->undo_top < HISTORY_CAP)
	{
		history->undo_stack[history->undo_top] = applied;
		history->undo_top++;
	}
	*out = applied;
	return (true);
}
