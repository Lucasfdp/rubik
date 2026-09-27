#include "rubik.h"

/// @brief Solves a valid, unsolved cube with Kociemba's two-phase method
///        and prints the solution.
///
/// The solver gets ONLY the cube (anti-cheat rule). Tables are built here,
/// after every check that can fail cheaply, so bad input never pays the
/// ~0.4 s startup.
///
/// @return 0 on success, 2 if the solver could not be built or found no
///         solution (neither should happen for a valid cube).
static int	solve_and_print(const t_cube *cube)
{
	t_solver	solver;
	t_move		solution[SOLVE_MAX_MOVES];
	char		text[SOLVE_MAX_MOVES * 3 + 1];
	int			count;

	if (!solver_init(&solver))
	{
		fprintf(stderr, "rubik: out of memory\n");
		return (2);
	}
	count = solve(&solver, cube, solution);
	solver_free(&solver);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("%s\n", text);
	return (0);
}

#ifdef BONUS_ALGO

/// Which solver "-a" picked. Kociemba is the default (no flag needed),
/// matching the mandatory binary's only behaviour.
typedef enum e_algo
{
	ALGO_KOCIEMBA,
	ALGO_THISTLETHWAITE,
}	t_algo;

/// @brief Solves a valid, unsolved cube with Thistlethwaite's four-phase
///        method and prints the solution.
///
/// Mirrors solve_and_print() above: t_thistle_solver does not own a
/// t_move_tables (include/thistlethwaite.h), so one is built and freed
/// here around it, same anti-cheat rule (the solver never sees the
/// scramble, only the resulting cube).
///
/// @return 0 on success, 2 if the tables could not be built or no
///         solution was found (neither should happen for a valid cube).
static int	solve_and_print_thistle(const t_cube *cube)
{
	t_move_tables		tables;
	t_thistle_solver	solver;
	t_move				solution[THISTLE_MAX_MOVES];
	char				text[THISTLE_MAX_MOVES * 3 + 1];
	int					count;

	if (!move_tables_build(&tables))
	{
		fprintf(stderr, "rubik: out of memory\n");
		return (2);
	}
	if (!thistle_init(&solver, &tables))
	{
		move_tables_free(&tables);
		fprintf(stderr, "rubik: out of memory\n");
		return (2);
	}
	count = thistle_solve(&solver, cube, solution);
	thistle_free(&solver);
	move_tables_free(&tables);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("%s\n", text);
	return (0);
}

/// @brief Pulls the optional "-a kociemba|thistlethwaite" flag out of argv,
///        leaving exactly one non-flag argument: the scramble.
///
/// Every option flag in this project is `-`-prefixed (docs/en/06-roadmap-
/// bonus.md), and the scramble must stay valid regardless of which flags
/// are combined with it — so the flag and the scramble can appear in
/// either order, and anything else is a usage error.
///
/// @param ac       Argument count, as passed to main().
/// @param av       Argument vector, as passed to main().
/// @param algo     Set to the requested algorithm; ALGO_KOCIEMBA if "-a"
///                 is absent.
/// @param scramble Set to point at the scramble argument (into av).
/// @return true on a well-formed command line; false on a usage error, in
///         which case a message was already printed to stderr.
static bool	parse_args(int ac, char const *av[], t_algo *algo,
				char const **scramble)
{
	int	i;

	*algo = ALGO_KOCIEMBA;
	*scramble = NULL;
	i = 1;
	while (i < ac)
	{
		if (strcmp(av[i], "-a") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "kociemba") == 0)
			*algo = ALGO_KOCIEMBA;
		else if (strcmp(av[i], "-a") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "thistlethwaite") == 0)
			*algo = ALGO_THISTLETHWAITE;
		else if (strcmp(av[i], "-a") == 0 || *scramble != NULL)
			break ;
		else
		{
			*scramble = av[i];
			i++;
			continue ;
		}
		i += 2;
	}
	if (i != ac || *scramble == NULL)
	{
		fprintf(stderr,
			"usage: %s \"<scramble>\" [-a kociemba|thistlethwaite]\n",
			av[0]);
		return (false);
	}
	return (true);
}

/// @brief Entry point (bonus): reads a scramble and an optional "-a" flag,
///        then prints a solution from whichever solver was picked.
///
/// Same pipeline as the mandatory build below: parse -> apply to
/// SOLVED_CUBE -> validate -> hand ONLY the resulting cube to a solver
/// (docs/en/01-requirements.md's anti-cheat rule). "-a" only chooses which
/// solver runs at the very end.
///
/// @return 0 on success, 1 on a usage/parse/validation error, 2 on an
///         internal failure (out of memory, no solution).
int	main(int ac, char const *av[])
{
	t_move			moves[MAX_MOVES];
	size_t			count;
	t_cube			cube;
	t_parse_status	status;
	t_algo			algo;
	char const		*scramble;

	if (!parse_args(ac, av, &algo, &scramble))
		return (1);
	status = parse_notation(scramble, moves, &count);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, moves, count);
	status = validate_cube(&cube);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	if (cube_is_solved(&cube))
	{
		printf("\n");
		return (0);
	}
	if (algo == ALGO_THISTLETHWAITE)
		return (solve_and_print_thistle(&cube));
	return (solve_and_print(&cube));
}

#else

/// @brief Entry point (mandatory): reads a scramble from argv[1] and
///        prints a solution. Kociemba only — no "-a" flag exists here.
///
/// Usage: rubik "<scramble>", for example: rubik "R2 D' B'"
///
/// Pipeline: parse the string into moves, apply them to a copy of
/// SOLVED_CUBE, validate the resulting state, then hand ONLY that state to
/// the solver. The move list and the string never reach the solver: that
/// is the anti-cheat rule (docs/en/01-requirements.md), so the solver
/// physically cannot return the inverse of the scramble.
///
/// Prints the solution on one line, moves separated by single spaces. An
/// already-solved cube prints an empty line (zero moves).
///
/// @return 0 on success, 1 on wrong argument count or a parse/validation
///         error, 2 on an internal failure (out of memory, no solution).
int	main(int ac, char const *av[])
{
	t_move			moves[MAX_MOVES];
	size_t			count;
	t_cube			cube;
	t_parse_status	status;

	if (ac != 2)
	{
		fprintf(stderr, "usage: %s \"<scramble>\"\n", av[0]);
		return (1);
	}
	status = parse_notation(av[1], moves, &count);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, moves, count);
	status = validate_cube(&cube);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	if (cube_is_solved(&cube))
	{
		printf("\n");
		return (0);
	}
	return (solve_and_print(&cube));
}

#endif
