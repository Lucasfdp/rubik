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

# include "render.h"
# include "scramble.h"
# include <time.h>

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

/// @brief Solves a valid, unsolved cube with the beginner (layer-by-
///        layer) method and prints the solution.
///
/// Mirrors solve_and_print() / solve_and_print_thistle() above.
/// layer_solve() (include/layer.h) needs no tables of its own -- there
/// is nothing to build or free.
///
/// @return 0 on success, 2 if no solution was found (should not happen
///         for a valid cube).
static int	solve_and_print_layer(const t_cube *cube)
{
	t_move	solution[LAYER_MAX_MOVES];
	char	text[LAYER_MAX_MOVES * 3 + 1];
	int		count;

	count = layer_solve(cube, solution);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("%s\n", text);
	return (0);
}

/// @brief Solves the CORNERS of a valid, unscrambled-elsewhere cube as a
///        2x2x2 (the "ways to work with other puzzles" bonus item, "-p
///        2x2x2") and prints the solution.
///
/// Mirrors solve_and_print() / solve_and_print_layer() above, with
/// two_solver_init()/two_solve() (include/twobytwo.h) in their place.
/// Edges are read by nothing on this path: the cube can come from any
/// scramble, its edge state is simply never looked at.
///
/// @return 0 on success, 2 if the solver could not be built, the corner
///         invariant failed, or no solution was found (the last two
///         should not happen for a cube built from a real scramble).
static int	solve_and_print_two(const t_cube *cube)
{
	t_two_solver	solver;
	t_move			solution[TWOBYTWO_MAX_MOVES];
	char			text[TWOBYTWO_MAX_MOVES * 3 + 1];
	int				count;

	if (!two_solver_init(&solver))
	{
		fprintf(stderr, "rubik: out of memory\n");
		return (2);
	}
	count = two_solve(&solver, cube, solution);
	two_solver_free(&solver);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("%s\n", text);
	return (0);
}

/// @brief Runs all three algorithms on the same cube and prints each
///        one's move count and solution, fixed order (kociemba,
///        thistlethwaite, layer) so the spread is easy to read without
///        re-sorting. Move count first (that's the thing being
///        compared), solution text after, same format_moves() everyone
///        else uses.
///
/// @return 0 on success, 2 if any solver could not be built or found no
///         solution (neither should happen for a valid cube).
static int	compare_algorithms(const t_cube *cube)
{
	t_solver			kociemba;
	t_move_tables		tables;
	t_thistle_solver	thistle;
	/* Shared buffer: sized for LAYER_MAX_MOVES, the largest bound of
	 * the three algorithms compared here. Every solver below writes
	 * into this same buffer, so it must fit the biggest one, not
	 * whichever one is called first. */
	t_move				solution[LAYER_MAX_MOVES];
	char				text[LAYER_MAX_MOVES * 3 + 1];
	int					count;

	if (!solver_init(&kociemba) || !move_tables_build(&tables)
		|| !thistle_init(&thistle, &tables))
	{
		fprintf(stderr, "rubik: out of memory\n");
		return (2);
	}
	count = solve(&kociemba, cube, solution);
	solver_free(&kociemba);
	if (count < 0)
	{
		thistle_free(&thistle);
		move_tables_free(&tables);
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("kociemba:       %3d moves   %s\n", count, text);
	count = thistle_solve(&thistle, cube, solution);
	thistle_free(&thistle);
	move_tables_free(&tables);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("thistlethwaite: %3d moves   %s\n", count, text);
	count = layer_solve(cube, solution);
	if (count < 0)
	{
		fprintf(stderr, "rubik: no solution found\n");
		return (2);
	}
	format_moves(solution, (size_t)count, text);
	printf("layer:          %3d moves   %s\n", count, text);
	return (0);
}

/// @brief Parses a positive base-10 integer argument for "-g"/"-n":
///        rejects anything that is not purely digits, empty, a leading
///        '-', zero, or over `max`. Shared by both flags so they can't
///        drift on what counts as a valid number.
///
/// @param s   The argv token right after "-g" or "-n".
/// @param max Upper bound (inclusive) -- MAX_MOVES for "-g", unbounded
///            (SIZE_MAX) for "-n".
/// @param out Set to the parsed value on success; untouched on failure.
/// @return true on a valid 1..max integer, false otherwise.
static bool	parse_uint_arg(const char *s, size_t max, size_t *out)
{
	unsigned long	value;
	char			*end;

	if (s[0] == '\0' || s[0] == '-')
		return (false);
	value = strtoul(s, &end, 10);
	if (*end != '\0' || value < 1 || value > (unsigned long)max)
		return (false);
	*out = (size_t)value;
	return (true);
}

/// @brief Final well-formedness check for parse_args(): unconsumed
///        argv, a missing scramble, "-r"/"-p 2x2x2" each against "-c",
///        a literal scramble alongside "-g"/"-n" (contradictory -- pick
///        one source), and a batch of more than one generated mix
///        (explicit "-n" > 1) combined with "-r"/"-a"/"-p 2x2x2"/"-c" --
///        rendering or solving N results at once is undefined, so batch
///        generation stays print-only.
static bool	args_well_formed(int i, int ac, char const *scramble,
				bool want_render, bool want_compare, bool want_two,
				bool want_generate, bool algo_given, size_t gen_count)
{
	if (i != ac)
		return (false);
	if (scramble == NULL && !want_render && !want_generate)
		return (false);
	if (want_render && want_compare)
		return (false);
	if (want_two && want_compare)
		return (false);
	if (want_generate && scramble != NULL)
		return (false);
	if (want_generate && gen_count > 1
		&& (want_render || want_compare || want_two || algo_given))
		return (false);
	return (true);
}

/// @brief Pulls the optional "-a kociemba|thistlethwaite|layer", "-r",
///        "-c", "-p 2x2x2", "-g LENGTH" and "-n COUNT" flags out of
///        argv, leaving exactly one non-flag argument (the scramble) --
///        except with "-r" alone, or with "-g"/"-n" (either generates
///        its own scramble), where the scramble argument is skipped
///        entirely.
///
/// "-c" (compare all three 3x3x3 algorithms) is mutually exclusive with
/// "-r": the comparison is print-only, and rendering one of three
/// solutions at once is a separate feature nobody asked for. "-c" also
/// ignores "-a" (it runs every algorithm, so picking one makes no sense)
/// -- but the two may still be given together without erroring, "-a" is
/// simply unused in that case, same as any option a person forgot
/// they'd typed. "-p 2x2x2" is likewise mutually exclusive with "-c" --
/// the three algorithms it compares are all 3x3x3 solvers -- but, unlike
/// "-c", NOT with "-r" any more: "-p 2x2x2 -r" is exactly how the 3D
/// bonus's K key (render/app.c) gets an initial puzzle to open on
/// instead of always starting on a 3x3x3 and waiting for a keypress.
///
/// "-g"/"-n" generate a mix instead of reading one from argv (the
/// integrated mixing generator bonus item): with neither "-r", "-a" nor
/// "-c"/"-p 2x2x2" given, the generated mix is printed and the program
/// exits -- only an explicit "-a"/"-c"/"-r"/"-p 2x2x2" opts into
/// solving, comparing or rendering it, same as a typed scramble would
/// be. See docs/en/06-roadmap-bonus.md item 2.
///
/// Every option flag in this project is `-`-prefixed, and the scramble
/// must stay valid regardless of which flags are combined with it -- so
/// the flags and the scramble can appear in any order, and anything else
/// is a usage error.
///
/// @param ac            Argument count, as passed to main().
/// @param av            Argument vector, as passed to main().
/// @param algo          Set to the requested algorithm; ALGO_KOCIEMBA if
///                      "-a" is absent.
/// @param algo_given    Set to true if "-a" was given (distinguishes a
///                      real choice from the ALGO_KOCIEMBA default, for
///                      the mixing generator's print-vs-solve rule).
/// @param scramble      Set to point at the scramble argument (into av),
///                      or NULL when "-r"/"-g"/"-n" supplies the cube
///                      instead.
/// @param want_render   Set to true if "-r" was given.
/// @param want_compare  Set to true if "-c" was given.
/// @param want_two      Set to true if "-p 2x2x2" was given.
/// @param want_generate Set to true if "-g" or "-n" was given.
/// @param gen_len       Set from "-g"'s argument; 0 if "-g" was absent
///                      (the caller then applies SCRAMBLE_DEFAULT_LEN).
/// @param gen_count     Set from "-n"'s argument; 0 if "-n" was absent
///                      (the caller then applies a default of 1).
/// @return true on a well-formed command line; false on a usage error, in
///         which case a message was already printed to stderr.
static bool	parse_args(int ac, char const *av[], t_algo *algo,
				bool *algo_given, char const **scramble, bool *want_render,
				bool *want_compare, bool *want_two, bool *want_generate,
				size_t *gen_len, size_t *gen_count)
{
	int	i;

	*algo = ALGO_KOCIEMBA;
	*algo_given = false;
	*scramble = NULL;
	*want_render = false;
	*want_compare = false;
	*want_two = false;
	*want_generate = false;
	*gen_len = 0;
	*gen_count = 0;
	i = 1;
	while (i < ac)
	{
		if (strcmp(av[i], "-a") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "kociemba") == 0)
		{
			*algo = ALGO_KOCIEMBA;
			*algo_given = true;
			i += 2;
		}
		else if (strcmp(av[i], "-a") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "thistlethwaite") == 0)
		{
			*algo = ALGO_THISTLETHWAITE;
			*algo_given = true;
			i += 2;
		}
		else if (strcmp(av[i], "-a") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "layer") == 0)
		{
			*algo = ALGO_LAYER;
			*algo_given = true;
			i += 2;
		}
		else if (strcmp(av[i], "-r") == 0)
		{
			*want_render = true;
			i++;
		}
		else if (strcmp(av[i], "-c") == 0)
		{
			*want_compare = true;
			i++;
		}
		else if (strcmp(av[i], "-p") == 0 && i + 1 < ac
			&& strcmp(av[i + 1], "2x2x2") == 0)
		{
			*want_two = true;
			i += 2;
		}
		else if (strcmp(av[i], "-g") == 0 && i + 1 < ac
			&& parse_uint_arg(av[i + 1], MAX_MOVES, gen_len))
		{
			*want_generate = true;
			i += 2;
		}
		else if (strcmp(av[i], "-n") == 0 && i + 1 < ac
			&& parse_uint_arg(av[i + 1], SIZE_MAX, gen_count))
		{
			*want_generate = true;
			i += 2;
		}
		else if (strcmp(av[i], "-a") == 0 || strcmp(av[i], "-p") == 0
			|| strcmp(av[i], "-g") == 0 || strcmp(av[i], "-n") == 0
			|| *scramble != NULL)
			break ;
		else
		{
			*scramble = av[i];
			i++;
		}
	}
	if (!args_well_formed(i, ac, *scramble, *want_render, *want_compare,
			*want_two, *want_generate, *algo_given, *gen_count))
	{
		fprintf(stderr, "usage: %s [\"<scramble>\"] "
			"[-a kociemba|thistlethwaite|layer] [-p 2x2x2] [-r] [-c] "
			"[-g LENGTH] [-n COUNT]\n", av[0]);
		return (false);
	}
	return (true);
}

/// @brief Prints COUNT freshly generated LENGTH-move mixes, one per
///        line, and nothing else -- the batch path of the mixing
///        generator ("-n" > 1), which per docs/en/06-roadmap-bonus.md
///        stays print-only rather than trying to render or solve N
///        results at once. Also the engine a future benchmark harness
///        would drive.
///
/// @return Always 0 -- generation itself cannot fail.
static int	generate_and_print(size_t length, size_t count,
				unsigned int *seed)
{
	t_move	moves[MAX_MOVES];
	char	text[MAX_MOVES * 3 + 1];
	size_t	i;

	i = 0;
	while (i < count)
	{
		scramble_generate(moves, length, seed);
		format_moves(moves, length, text);
		printf("%s\n", text);
		i++;
	}
	return (0);
}

/// @brief Fills `moves`/`count` with the cube's move list, from whichever
///        source parse_args() selected: a freshly generated single mix
///        ("-g"/"-n" with a count of exactly 1 -- a count above 1 is
///        handled earlier in main() by generate_and_print() and never
///        reaches here), or the parsed scramble argument.
static t_parse_status	resolve_scramble_moves(char const *scramble,
				bool want_generate, size_t gen_len, unsigned int *seed,
				t_move *moves, size_t *count)
{
	if (want_generate)
	{
		scramble_generate(moves, gen_len, seed);
		*count = gen_len;
		return (PARSE_OK);
	}
	return (parse_notation(scramble, moves, count));
}

/// @brief What to do with a validated cube once its move list is known:
///        render it, compare all three 3x3x3 algorithms, solve it as a
///        2x2x2 ("-p 2x2x2"), print nothing for an already-solved cube,
///        or solve with whichever 3x3x3 algorithm was chosen. Unchanged
///        from before the mixing generator -- the generator's own
///        print-only case is handled earlier in main(), before this is
///        ever reached.
static int	dispatch_cube(const t_cube *cube, bool want_render,
				bool want_compare, bool want_two, t_algo algo,
				t_puzzle puzzle)
{
	if (want_render)
		return (render_run(cube, true, puzzle) ? 0 : 2);
	if (want_compare)
		return (compare_algorithms(cube));
	if (want_two)
	{
		if (two_cube_is_solved(cube))
		{
			printf("\n");
			return (0);
		}
		return (solve_and_print_two(cube));
	}
	if (cube_is_solved(cube))
	{
		printf("\n");
		return (0);
	}
	if (algo == ALGO_THISTLETHWAITE)
		return (solve_and_print_thistle(cube));
	if (algo == ALGO_LAYER)
		return (solve_and_print_layer(cube));
	return (solve_and_print(cube));
}

/// @brief Entry point (bonus): reads a scramble (typed on argv, or
///        generated by "-g"/"-n") and the optional "-a", "-r", "-c" and
///        "-p 2x2x2" flags, then either prints a solution from whichever
///        solver was picked, prints all three 3x3x3 solvers' move counts
///        side by side ("-c"), opens the 3D window instead ("-r", on the
///        2x2x2 view already if "-p 2x2x2" was also given), or -- with
///        no scramble source but "-g"/"-n" and none of the above -- just
///        prints the generated mix(es) and exits.
///
/// Same pipeline as the mandatory build below: parse/generate -> apply to
/// SOLVED_CUBE -> validate -> hand ONLY the resulting cube onward -- to a
/// solver, to the "-c" comparison, or to render_run() (anti-cheat rule).
/// "-a"/"-c"/"-p 2x2x2" only choose what happens at the very end; "-r" is
/// checked first and, with no scramble and no generator flag, skips
/// parsing entirely. A batch of generated mixes ("-n" > 1) is print-only
/// and returns before any of that pipeline runs.
///
/// @return 0 on success, 1 on a usage/parse/validation error, 2 on an
///         internal failure (out of memory, no solution, render error).
int	main(int ac, char const *av[])
{
	t_move			moves[MAX_MOVES];
	char			text[MAX_MOVES * 3 + 1];
	size_t			count;
	t_cube			cube;
	t_parse_status	status;
	t_algo			algo;
	bool			algo_given;
	char const		*scramble;
	bool			want_render;
	bool			want_compare;
	bool			want_two;
	bool			want_generate;
	size_t			gen_len;
	size_t			gen_count;
	t_puzzle		puzzle;
	unsigned int	seed;

	if (!parse_args(ac, av, &algo, &algo_given, &scramble, &want_render,
			&want_compare, &want_two, &want_generate, &gen_len,
			&gen_count))
		return (1);
	puzzle = PUZZLE_3X3X3;
	if (want_two)
		puzzle = PUZZLE_2X2X2;
	seed = (unsigned int)time(NULL);
	if (want_generate)
	{
		if (gen_len == 0)
			gen_len = SCRAMBLE_DEFAULT_LEN;
		if (gen_count == 0)
			gen_count = 1;
		if (gen_count > 1)
			return (generate_and_print(gen_len, gen_count, &seed));
	}
	else if (want_render && scramble == NULL)
		return (render_run(&SOLVED_CUBE, false, puzzle) ? 0 : 2);
	status = resolve_scramble_moves(scramble, want_generate, gen_len, &seed,
			moves, &count);
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
	if (want_generate && !want_render && !want_compare && !want_two
		&& !algo_given)
	{
		format_moves(moves, count, text);
		printf("%s\n", text);
		return (0);
	}
	return (dispatch_cube(&cube, want_render, want_compare, want_two, algo,
			puzzle));
}

#else

/// @brief Entry point (mandatory): reads a scramble from argv[1] and
///        prints a solution. Kociemba only -- no "-a" flag exists here.
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
