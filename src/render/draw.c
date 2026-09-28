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

/// @brief Phase 7 §9.4's cheapest rounding trick: a small body-coloured
///        sphere over each of the cube's 8 corners, drawn on top of the
///        sharp wireframe outline so the silhouette reads as rounded
///        with no custom beveled mesh to build or load.
static void	draw_corner_fillets(Vector3 center)
{
	float	half;
	int		i;
	Vector3	pos;

	half = BODY_SIZE / 2.0f - CORNER_FILLET_RADIUS * 0.6f;
	i = 0;
	while (i < 8)
	{
		pos = (Vector3){center.x + CORNER_SIGNS[i].x * half,
			center.y + CORNER_SIGNS[i].y * half,
			center.z + CORNER_SIGNS[i].z * half};
		DrawSphere(pos, CORNER_FILLET_RADIUS, BODY_COLOR);
		i++;
	}
}

/// @brief Draws one cubie's dark plastic body plus one coloured sticker
///        quad per populated face[] direction, centred at `center`
///        (world space — the caller has already applied any transient
///        layer rotation to the modelview matrix before calling this).
///        Body + stickers draw inside the lighting shader when it's
///        loaded; the wireframe edge stays a flat, unlit outline either
///        way — a wireframe has no normals for the shader to light.
static void	draw_cubie(const t_render_cubie *cubie, Vector3 center,
	const t_render_lighting *lighting, bool rounded_corners)
{
	int		face;
	Vector3	normal;
	Vector3	pos;

	if (lighting->loaded)
		BeginShaderMode(lighting->shader);
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
	if (lighting->loaded)
		EndShaderMode();
	DrawCubeWires(center, BODY_SIZE, BODY_SIZE, BODY_SIZE, DARKGRAY);
	if (rounded_corners)
		draw_corner_fillets(center);
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
			draw_cubie(cubie, center, lighting, rounded_corners);
			rlPopMatrix();
		}
		else
			draw_cubie(cubie, center, lighting, rounded_corners);
		i++;
	}
}
