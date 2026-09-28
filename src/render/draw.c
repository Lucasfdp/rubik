#include "raylib.h"
#include "rlgl.h"
#define RLIGHTS_IMPLEMENTATION
#include "render/rlights.h"
#include "render/draw.h"

# define BODY_SIZE CUBIE_BODY_SIZE
# define STICKER_SIZE 0.78f
# define STICKER_DEPTH 0.02f
# define STICKER_OFFSET (BODY_SIZE / 2.0f + 0.006f)
# define LIGHTING_VS_120 "assets/shaders/glsl120/lighting.vs"
# define LIGHTING_FS_120 "assets/shaders/glsl120/lighting.fs"
# define LIGHTING_VS_330 "assets/shaders/glsl330/lighting.vs"
# define LIGHTING_FS_330 "assets/shaders/glsl330/lighting.fs"
# define CORNER_FILLET_RADIUS 0.10f
/// docs/en/11-drag-review.md §3.2/P2: rings/slices for the corner-fillet
/// spheres, down from DrawSphere()'s default 16x16 (1,536 verts each,
/// ~320k/frame across 208 of them). 4x8 (192 verts each, ~8x fewer
/// total) reads identically at this radius.
# define CORNER_FILLET_RINGS 4
# define CORNER_FILLET_SLICES 8

static const Color	BODY_COLOR = {30, 30, 30, 255};

/// Phase 7 §9.4's fillet-sphere offsets: the 8 sign combinations of a
/// cubie's corners, in unit-cube coordinates.
static const Vector3	CORNER_SIGNS[8] = {
	{-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f},
	{-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f},
	{1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f},
	{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f},
};

/// @brief The outward unit vector for one of the 6 sticker directions,
///        under this project's axis convention (+X=R, +Y=U, +Z=F).
static Vector3	face_normal(t_render_face face)
{
	if (face == FACE_UP)
		return ((Vector3){0.0f, 1.0f, 0.0f});
	if (face == FACE_DOWN)
		return ((Vector3){0.0f, -1.0f, 0.0f});
	if (face == FACE_RIGHT)
		return ((Vector3){1.0f, 0.0f, 0.0f});
	if (face == FACE_LEFT)
		return ((Vector3){-1.0f, 0.0f, 0.0f});
	if (face == FACE_FRONT)
		return ((Vector3){0.0f, 0.0f, 1.0f});
	return ((Vector3){0.0f, 0.0f, -1.0f});
}

/// @brief The sticker quad's own size: thin along whichever axis its
///        normal points along, full-size on the other two.
static Vector3	sticker_size(Vector3 normal)
{
	if (normal.x != 0.0f)
		return ((Vector3){STICKER_DEPTH, STICKER_SIZE, STICKER_SIZE});
	if (normal.y != 0.0f)
		return ((Vector3){STICKER_SIZE, STICKER_DEPTH, STICKER_SIZE});
	return ((Vector3){STICKER_SIZE, STICKER_SIZE, STICKER_DEPTH});
}

/// @brief True for a GL context whose shaders need #version 120 syntax
///        (attribute/varying, no gl_FragColor-replacement) rather than
///        330's (in/out). RL_OPENGL_11 has no shader support at all —
///        draw_lighting_load() below just fails IsShaderValid() for it
///        either way, so it does not need its own branch here.
static bool	needs_glsl_120(void)
{
	int	version;

	version = rlGetVersion();
	return (version == RL_OPENGL_21 || version == RL_OPENGL_11);
}

t_render_lighting	draw_lighting_load(void)
{
	t_render_lighting	lighting;
	float				ambient[4];

	if (needs_glsl_120())
		lighting.shader = LoadShader(LIGHTING_VS_120, LIGHTING_FS_120);
	else
		lighting.shader = LoadShader(LIGHTING_VS_330, LIGHTING_FS_330);
	lighting.loaded = IsShaderValid(lighting.shader);
	if (!lighting.loaded)
		return (lighting);
	lighting.shader.locs[SHADER_LOC_VECTOR_VIEW]
		= GetShaderLocation(lighting.shader, "viewPos");
	ambient[0] = 0.35f;
	ambient[1] = 0.35f;
	ambient[2] = 0.38f;
	ambient[3] = 1.0f;
	SetShaderValue(lighting.shader, GetShaderLocation(lighting.shader,
			"ambient"), ambient, SHADER_UNIFORM_VEC4);
	CreateLight(LIGHT_POINT, (Vector3){6.0f, 8.0f, 6.0f},
		(Vector3){0.0f, 0.0f, 0.0f}, WHITE, lighting.shader);
	CreateLight(LIGHT_POINT, (Vector3){-6.0f, 4.0f, -4.0f},
		(Vector3){0.0f, 0.0f, 0.0f}, (Color){150, 170, 255, 255},
		lighting.shader);
	/// docs/en/12-visual-polish-review.md V4: a rim/back light, placed
	/// behind and above the cube relative to the default camera view,
	/// to separate the cube's silhouette from the (now dark) background.
	CreateLight(LIGHT_POINT, (Vector3){-2.0f, 10.0f, -8.0f},
		(Vector3){0.0f, 0.0f, 0.0f}, (Color){170, 185, 210, 255},
		lighting.shader);
	/// Low, dim fill light from below: the two original lights and the
	/// rim light all sit at positive Y, so the D face only ever got flat
	/// ambient. Kept deliberately faint (low RGB, not WHITE) so it lifts
	/// the D face slightly without looking lit-from-below.
	CreateLight(LIGHT_POINT, (Vector3){0.0f, -9.0f, 3.0f},
		(Vector3){0.0f, 0.0f, 0.0f}, (Color){40, 40, 44, 255},
		lighting.shader);
	return (lighting);
}

void	draw_lighting_update_camera(t_render_lighting *lighting,
	Camera3D camera)
{
	float	pos[3];

	if (!lighting->loaded)
		return ;
	pos[0] = camera.position.x;
	pos[1] = camera.position.y;
	pos[2] = camera.position.z;
	SetShaderValue(lighting->shader,
		lighting->shader.locs[SHADER_LOC_VECTOR_VIEW], pos,
		SHADER_UNIFORM_VEC3);
}

void	draw_lighting_unload(t_render_lighting *lighting)
{
	if (lighting->loaded)
		UnloadShader(lighting->shader);
	lighting->loaded = false;
}

/// @brief True if `sign` (one of CORNER_SIGNS) is a corner of `cubie`
///        that no exposed face touches, and is therefore always fully
///        enclosed by neighbouring cubies — docs/en/11-drag-review.md
///        §3.2/P2's second cut. A face along axis i is exposed only
///        when the cubie sits on THAT axis's outer layer (cubie_pos[i]
///        != 0) and `sign`'s component matches its side (a middle-slice
///        cubie, cubie_pos[i] == 0, has a neighbour on BOTH sides of
///        that axis, so it never contributes an exposed face at all).
///        Hidden requires ALL THREE axes to fail that test — a corner
///        touching even one exposed face still bounds that face's own
///        rounded-corner grid and stays visible. Verified exhaustively
///        (all 26 cubies x 8 corners, see the doc's own worked example):
///        1 of 8 hidden per corner cubie (only the one pointing straight
///        at the puzzle's centre), 2 of 8 per edge, 4 of 8 per centre —
///        56 of 208 total (~27%), short of the doc's "roughly halves it
///        again" but still a real cut on top of the resolution drop, and
///        never hides a corner that's actually reachable by an exposed
///        face.
static bool	corner_hidden(const t_render_cubie *cubie, Vector3 sign)
{
	bool	exposed_x;
	bool	exposed_y;
	bool	exposed_z;

	exposed_x = (cubie->x != 0) && ((sign.x > 0.0f) == (cubie->x > 0));
	exposed_y = (cubie->y != 0) && ((sign.y > 0.0f) == (cubie->y > 0));
	exposed_z = (cubie->z != 0) && ((sign.z > 0.0f) == (cubie->z > 0));
	return (!(exposed_x || exposed_y || exposed_z));
}

/// @brief Phase 7 §9.4's cheapest rounding trick: a small body-coloured
///        sphere over each of the cube's 8 corners, drawn on top of the
///        sharp wireframe outline so the silhouette reads as rounded
///        with no custom beveled mesh to build or load. Low-poly
///        (CORNER_FILLET_RINGS/SLICES) and culled (corner_hidden())
///        per §3.2/P2 — `skip_culling` turns the cull off for a cubie
///        whose own layer is mid-turn, since a spinning layer opens
///        gaps that can expose a normally-enclosed corner.
static void	draw_corner_fillets(const t_render_cubie *cubie, Vector3 center,
	bool skip_culling)
{
	float	half;
	int		i;
	Vector3	pos;

	half = BODY_SIZE / 2.0f - CORNER_FILLET_RADIUS * 0.6f;
	i = 0;
	while (i < 8)
	{
		if (skip_culling || !corner_hidden(cubie, CORNER_SIGNS[i]))
		{
			pos = (Vector3){center.x + CORNER_SIGNS[i].x * half,
				center.y + CORNER_SIGNS[i].y * half,
				center.z + CORNER_SIGNS[i].z * half};
			DrawSphereEx(pos, CORNER_FILLET_RADIUS, CORNER_FILLET_RINGS,
				CORNER_FILLET_SLICES, BODY_COLOR);
		}
		i++;
	}
}

/// @brief Draws one cubie's dark plastic body plus one coloured sticker
///        quad per populated face[] direction, centred at `center` —
///        the shaded (lit) half of draw_scene()'s two passes (§3.1/P1).
static void	draw_cubie_shaded(const t_render_cubie *cubie, Vector3 center)
{
	int		face;
	Vector3	normal;
	Vector3	pos;

	DrawCube(center, BODY_SIZE, BODY_SIZE, BODY_SIZE, BODY_COLOR);
	face = 0;
	while (face < RENDER_FACE_COUNT)
	{
		if (cubie->has_face[face])
		{
			normal = face_normal((t_render_face)face);
			pos.x = center.x + normal.x * STICKER_OFFSET;
			pos.y = center.y + normal.y * STICKER_OFFSET;
			pos.z = center.z + normal.z * STICKER_OFFSET;
			DrawCubeV(pos, sticker_size(normal), cubie->face[face]);
		}
		face++;
	}
}

/// @brief One cubie's flat, unlit wireframe edge plus rounded-corner
///        fillets — the unshaded half of draw_scene()'s two passes
///        (§3.1/P1). A wireframe has no normals for the lighting shader
///        to light, so it (and the fillets on top of it) were always
///        drawn outside BeginShaderMode/EndShaderMode; splitting the two
///        passes this way just stops that switch happening PER CUBIE.
static void	draw_cubie_wire(const t_render_cubie *cubie, Vector3 center,
	bool rounded_corners, bool skip_culling)
{
	DrawCubeWires(center, BODY_SIZE, BODY_SIZE, BODY_SIZE, DARKGRAY);
	if (rounded_corners)
		draw_corner_fillets(cubie, center, skip_culling);
}

/// @brief True if this cubie's fixed slot sits on the layer a live turn
///        is rotating (i.e. it should be drawn spinning, not at rest).
static bool	in_turning_layer(const t_render_cubie *cubie,
	const t_active_turn *turn)
{
	if (turn->axis == AXIS_X)
		return (cubie->x == turn->layer);
	if (turn->axis == AXIS_Y)
		return (cubie->y == turn->layer);
	return (cubie->z == turn->layer);
}

/// @brief The world axis vector rlRotatef() needs for `axis`.
static Vector3	axis_vector(t_axis axis)
{
	if (axis == AXIS_X)
		return ((Vector3){1.0f, 0.0f, 0.0f});
	if (axis == AXIS_Y)
		return ((Vector3){0.0f, 1.0f, 0.0f});
	return ((Vector3){0.0f, 0.0f, 1.0f});
}

void	draw_scene(const t_render_scene *scene, const t_active_turn *turn,
	const t_render_lighting *lighting, bool rounded_corners)
{
	int						i;
	const t_render_cubie	*cubie;
	Vector3					center;
	Vector3					axis;
	bool					spinning;

	if (lighting->loaded)
		BeginShaderMode(lighting->shader);
	i = 0;
	while (i < RENDER_CUBIE_COUNT)
	{
		cubie = &scene->cubies[i];
		center = (Vector3){cubie->x * CUBIE_SPACING, cubie->y * CUBIE_SPACING,
			cubie->z * CUBIE_SPACING};
		spinning = turn != NULL && turn->active
			&& in_turning_layer(cubie, turn);
		if (spinning)
		{
			axis = axis_vector(turn->axis);
			rlPushMatrix();
			rlRotatef(turn->angle_deg, axis.x, axis.y, axis.z);
			draw_cubie_shaded(cubie, center);
			rlPopMatrix();
		}
		else
			draw_cubie_shaded(cubie, center);
		i++;
	}
	if (lighting->loaded)
		EndShaderMode();
	i = 0;
	while (i < RENDER_CUBIE_COUNT)
	{
		cubie = &scene->cubies[i];
		center = (Vector3){cubie->x * CUBIE_SPACING, cubie->y * CUBIE_SPACING,
			cubie->z * CUBIE_SPACING};
		spinning = turn != NULL && turn->active
			&& in_turning_layer(cubie, turn);
		if (spinning)
		{
			axis = axis_vector(turn->axis);
			rlPushMatrix();
			rlRotatef(turn->angle_deg, axis.x, axis.y, axis.z);
			draw_cubie_wire(cubie, center, rounded_corners, true);
			rlPopMatrix();
		}
		else
			draw_cubie_wire(cubie, center, rounded_corners, false);
		i++;
	}
}
