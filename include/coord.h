#ifndef COORD_H
# define COORD_H

# include <stdint.h>
# include "cube.h"

/// A coordinate is ONE small number that describes ONE property of a cube
/// (docs/en/02a-cube-notation.md, docs/en/02-algorithms.md). Two rules hold
/// for every coordinate in this file:
///
///   - The solved cube always gives 0.
///   - decode(encode(cube)) rebuilds that one property of the cube, and
///     encode(decode(n)) == n for every n below the coordinate's COUNT.
///
/// decode_* functions write ONLY the property they describe into the cube
/// you pass in. Start from a copy of SOLVED_CUBE and everything else is
/// already a valid solved value.

/// Number of different twist values: 3^7. Only 7 of the 8 corner twists are
/// free, the 8th is forced by "twists add up to 0 mod 3".
# define TWIST_COUNT 2187

/// Number of different flip values: 2^11. Only 11 of the 12 edge flips are
/// free, the 12th is forced by "flips add up to 0 mod 2".
# define FLIP_COUNT 2048

/// @brief Corner orientations of a cube as one number, 0 .. TWIST_COUNT-1.
///
/// Reads corner_orient[0..6] as the digits of a base-3 number, slot 0 being
/// the biggest place. corner_orient[7] is not read (it is forced).
uint16_t	encode_twist(const t_cube *cube);

/// @brief Writes the corner orientations for a twist value into a cube.
///
/// Only corner_orient[] is written. Slot 7 is filled with the one value
/// that makes the 8 twists add up to 0 mod 3, so the result always passes
/// cube_corner_twist_sum(). twist must be below TWIST_COUNT.
void		decode_twist(t_cube *cube, uint16_t twist);

/// @brief Edge orientations of a cube as one number, 0 .. FLIP_COUNT-1.
///
/// Reads edge_orient[0..10] as the bits of a binary number, slot 0 being
/// the biggest place. edge_orient[11] is not read (it is forced).
uint16_t	encode_flip(const t_cube *cube);

/// @brief Writes the edge orientations for a flip value into a cube.
///
/// Only edge_orient[] is written. Slot 11 is filled with the one value that
/// makes the 12 flips add up to 0 mod 2, so the result always passes
/// cube_edge_flip_sum(). flip must be below FLIP_COUNT.
void		decode_flip(t_cube *cube, uint16_t flip);

/// Number of different slice values: C(12,4), "12 choose 4" = 495. The
/// ways to pick which 4 of the 12 edge slots hold the middle-layer edges.
# define SLICE_COUNT 495

/// @brief Where the 4 middle-layer edges are, as one number, 0 .. 494.
///
/// The middle-layer ("slice") edges are the pieces FR, FL, BL and BR (edge
/// pieces 8..11 in cube.h). Put a sticker on every edge slot that holds one
/// of them: the value names WHICH 4 slots have a sticker. Which of the four
/// pieces is in which slot does not matter, and neither do the other 8
/// edges. Solved (stickers on slots 8..11) is 0.
///
/// Needs a real cube: edge_perm must contain every piece exactly once.
uint16_t	encode_slice(const t_cube *cube);

/// @brief Writes an edge arrangement with that slice value into a cube.
///
/// Rewrites the whole edge_perm[] (and only that): the 4 marked slots get
/// the pieces 8..11 in increasing slot order, the other 8 slots get the
/// pieces 0..7 in increasing slot order. So encode_slice() of the result is
/// slice, and slice 0 gives the solved edge_perm. slice must be below
/// SLICE_COUNT.
void		decode_slice(t_cube *cube, uint16_t slice);

/// Number of different cperm values: 8! = 8*7*6*5*4*3*2*1 = 40320, the
/// number of ways to arrange the 8 corner pieces in the 8 corner slots.
# define CPERM_COUNT 40320

/// @brief Which corner piece is in which slot, as one number, 0 .. 40319.
///
/// Lists every arrangement of the 8 corner pieces in dictionary order
/// (compare slot 0 first, then slot 1, ...) and returns the arrangement's
/// position in that list. Solved, {0,1,2,...,7}, is first: 0.
///
/// Only corner_perm[] is read (not the twists, not the edges). It must be
/// a real permutation: every piece exactly once.
uint16_t	encode_cperm(const t_cube *cube);

/// @brief Writes the corner arrangement for a cperm value into a cube.
///
/// Only corner_perm[] is written (all 8 slots). cperm must be below
/// CPERM_COUNT; a bigger value wraps around (cperm % CPERM_COUNT) so the
/// result is always a real permutation.
void		decode_cperm(t_cube *cube, uint16_t cperm);

/// Number of different eperm values: 8! = 40320, the ways to arrange the 8
/// top and bottom edges (UR UF UL UB DR DF DL DB) in their 8 slots.
# define EPERM_COUNT 40320

/// Number of different sperm values: 4! = 24, the ways to arrange the 4
/// middle-layer edges (FR FL BL BR) among the 4 middle-layer slots.
# define SPERM_COUNT 24

/// @brief Order of the 8 top and bottom edges in slots 0..7, one number,
///        0 .. 40319. Same dictionary-order counting as encode_cperm().
///
/// ONLY MEANINGFUL ONCE THE MIDDLE-LAYER EDGES ARE BACK IN THE MIDDLE LAYER
/// (phase 1 done: encode_slice() == 0). Then slots 0..7 hold exactly the
/// pieces 0..7 and this number says how they are arranged. On any other
/// cube the value is still in range but does not describe anything useful.
/// Reads only edge_perm[0..7]. Solved is 0.
uint16_t	encode_eperm(const t_cube *cube);

/// @brief Writes the order of the 8 top and bottom edges for an eperm value.
///
/// Writes ONLY edge_perm[0..7] (with the pieces 0..7). Slots 8..11 are left
/// alone, so start from SOLVED_CUBE (or anything with the middle-layer
/// edges in slots 8..11) to get a real cube. eperm must be below
/// EPERM_COUNT; a bigger value wraps around.
void		decode_eperm(t_cube *cube, uint16_t eperm);

/// @brief Order of the 4 middle-layer edges among slots 8..11, one number,
///        0 .. 23.
///
/// Same counting again, on 4 pieces. Like eperm, only meaningful once the
/// middle-layer edges are in the middle layer (encode_slice() == 0).
/// Reads only edge_perm[8..11]. Solved is 0.
uint16_t	encode_sperm(const t_cube *cube);

/// @brief Writes the order of the 4 middle-layer edges for a sperm value.
///
/// Writes ONLY edge_perm[8..11] (with the pieces 8..11). Slots 0..7 are
/// left alone. sperm must be below SPERM_COUNT; a bigger value wraps.
void		decode_sperm(t_cube *cube, uint16_t sperm);

#endif
