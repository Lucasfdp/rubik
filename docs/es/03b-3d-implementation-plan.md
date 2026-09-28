# Bonus 3D — Plan de Implementación Completo (Fase por Fase)

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos. Este documento convierte el diseño de `03a-3d-experience-and-interaction.md` en un orden de construcción real: estructuras de datos concretas, firmas de funciones, algoritmos, listas de archivos, y un gate de salida para cada fase. Asume la decisión de stack de `03-graphics.md` (raylib) y el límite de módulos de `04-architecture.md`. Escrito contra el repo tal como está hoy: `apply_move()`, `SOLVED_CUBE`, `t_cube`, `t_move`, y los solvers Kociemba/Thistlethwaite ya existen y nada de lo de abajo los toca; raylib ya está vendorizada y compila (`lib/raylib/src/libraylib.a` existe) — el build spike del Sprint 5 ya está hecho.)*

## 0. Decisiones que este documento fija

Dos de los ítems abiertos en `08-risks-and-open-decisions.md` ahora están decididos, no solo recomendados:

- **#6 — separación por botón.** Arrastre izquierdo sobre una pegatina gira una capa; arrastre derecho en cualquier sitio orbita la cámara. Confirmado, construido en la Fase 3 y la Fase 6 de abajo.
- **#7 — secuenciación.** El giro manual por teclado se entrega y funciona completamente *antes* de intentar el clic-y-arrastre de ratón. La Fase 3 (teclado) debe pasar su gate antes de que empiece la Fase 6 (ratón).

Alcance de este plan: todo lo de la tabla MoSCoW de `03a` salvo los tres ítems **Won't** (trabajo de shader personalizado, un tirador de scrub de línea de tiempo arrastrable, VR/AR) — es decir, todos los Must, todos los Should, y todos los Could.

## 1. Cómo encajan las fases en `06-roadmap-bonus.md`

| Este documento | `06-roadmap-bonus.md` | Nivel MoSCoW |
|---|---|---|
| Fase 0 — Esqueleto de renderizado | Sprint 5 | (fontanería previa, no tiene su propia fila MoSCoW) |
| Fase 1 — Modo A: reproducción automática | Sprint 6 | Must |
| Fase 2 — Modo B: transporte de reproducción | Sprint 6 | Must |
| Fase 3 — Modo C: teclado + separación por botón | Sprint 6 / inicio del 7 | Must |
| Fase 4 — Extras del modo práctica | Sprint 7 | Should |
| Fase 5 — Base de acabado visual | Sprint 7 | Should |
| Fase 6 — Giro por clic-y-arrastre de ratón | Sprint 7 | Could |
| Fase 7 — Resto de acabado y extras | Sprint 7 | Could |

Cada fase enumera: objetivo, dependencia, archivos nuevos/modificados, las estructuras de datos y firmas de funciones reales, el algoritmo, el gate de salida, y una estimación de esfuerzo. Nada aquí es pseudocódigo disfrazado de vaguedad — cada firma está pensada para escribirse casi tal cual; cada paso de algoritmo está pensado para implementarse directamente a partir de esta descripción.

## 2. Una aclaración de diseño que merece señalarse de antemano

`04-architecture.md` reserva `cube/facelet.c` (conversión de 54 pegatinas <-> cubies) como algo "necesario para ... el renderizador." Tras trabajar el algoritmo de renderizado real de abajo (Fase 0), resulta que el renderizador **no** necesita en absoluto la cadena de 54 caracteres de facelets — lee `corner_perm`/`corner_orient`/`edge_perm`/`edge_orient` directamente y los mapea a colores mediante una pequeña tabla de colores de referencia por pieza (§4.2 de abajo), lo cual es más simple y no tiene ninguna tabla de traducción de índices hecha a mano que se pueda equivocar. `facelet.c` sigue mereciendo la pena construirse en algún momento — es genuinamente útil para una futura salida de depuración `-f`/cadena de facelets y para comparar el estado de vuestro solver contra herramientas de referencia online — pero **no está en el camino crítico** de ninguna fase de abajo, y ninguna de estas fases depende de él. Construidlo cuando os venga bien, de forma independiente.

## Fase 0 — Esqueleto de renderizado (ventana, cámara, escena estática)

**Objetivo:** `./rubik_bonus "<scramble>" -r` abre una ventana mostrando un cubo estático con la forma y los colores correctos (todavía sin animación), con control de cámara orbital por ratón. Esta es la base sobre la que se apoya todo lo demás.

**Depende de:** nada — primera fase.

### 2.1 Archivos nuevos

```
include/render.h              NUEVO — paraguas agnóstico de raylib, el
                               ÚNICO header que main.c tiene permitido
                               incluir de toda esta funcionalidad.
include/render/app.h          NUEVO
include/render/geometry.h     NUEVO
include/render/draw.h         NUEVO
src/render/app.c              NUEVO
src/render/geometry.c         NUEVO
src/render/draw.c             NUEVO
```

`include/render.h` deliberadamente no declara nada que huela a raylib, porque la unidad de traducción `BONUS_ALGO` de `main.c` se compila *sin* la ruta de include de raylib (solo los objetos bajo `src/render/` reciben `-I$(RAYLIB_SRC_DIR)`, según la regla `CPPFLAGS` específica por patrón del Makefile). Mantened el límite real, no solo por convención:

```c
#ifndef RENDER_H
# define RENDER_H
# include <stdbool.h>
# include "cube.h"

/// @brief Abre la ventana 3D y ejecuta el bucle de renderizado hasta que
///        el usuario la cierre. Posee todo lo de src/render/
///        internamente; esta es toda la superficie pública que main.c
///        toca jamás.
///
/// @param start_cube  El cubo con el que abrir. Cuando has_scramble es
///                    false, esto es SOLVED_CUBE y la ventana abre
///                    directamente en el Modo C (manual/práctica).
/// @param has_scramble true para resolver start_cube automáticamente y
///                    reproducir la solución al abrir (Modo A); false
///                    para abrir con un cubo resuelto listo para girar
///                    manualmente.
/// @return true si se cierra limpiamente, false si hubo un error interno
///         de renderizado.
bool	render_run(const t_cube *start_cube, bool has_scramble);
# endif
```

### 2.2 Cambio en `main.c`

Dentro de la rama `#ifdef BONUS_ALGO` ya existente, extended `parse_args()` (o añadid un segundo paso de parseo pequeño después de él) para aceptar un flag `-r`, y haced que el argumento de scramble sea opcional cuando `-r` está presente:

```c
/// usage: rubik_bonus ["<scramble>"] [-a kociemba|thistlethwaite] [-r]
```

- `-r` ausente: exactamente el comportamiento de hoy (solución en texto, `-a` seleccionable). Nada del camino de CLI ya probado cambia — `tests/test_cli.sh` y cualquier script de comparación con `-a` siguen funcionando sin tocarlos.
- `-r` presente con un scramble: parsear + validar como hoy, y luego, en vez de `solve_and_print()` / `solve_and_print_thistle()`, llamar a `render_run(&cube, true)`.
- `-r` presente sin scramble: saltarse el parseo por completo, llamar a `render_run(&SOLVED_CUBE, false)`.

Esto mantiene la pipeline anti-trampa existente de `main.c` (parsear -> aplicar -> validar -> pasar solo el cubo hacia adelante) completamente intacta para el caso de reproducción automática, y es un superconjunto estricto de la gramática de flags de hoy — ningún uso existente se rompe.

### 2.3 Estructuras de datos (`include/render/geometry.h`)

```c
typedef enum e_render_face
{
	FACE_UP, FACE_DOWN, FACE_RIGHT, FACE_LEFT, FACE_FRONT, FACE_BACK,
	RENDER_FACE_COUNT
}	t_render_face;

/// Uno de los 26 cubies visibles. `x/y/z` es su posición de SLOT en la
/// rejilla — fija para siempre, una entrada por slot físico, nunca se
/// permuta. Solo `face[]` cambia, y solo en respuesta a geometry_sync().
/// Esto es deliberado: significa que un movimiento nunca mueve una
/// struct cubie a un slot distinto; solo repinta qué colores hay en los
/// 26 slots fijos, que es lo que hace que geometry_sync() sea un resync
/// completo, barato y siempre correcto en vez de una contabilidad
/// incremental que se pueda desincronizar.
typedef struct s_render_cubie
{
	int8_t	x;
	int8_t	y;
	int8_t	z;
	Color	face[RENDER_FACE_COUNT];	// BLANK_COLOR donde este cubie no
										// tiene pegatina mirando hacia
										// esa dirección
}	t_render_cubie;

typedef struct s_render_scene
{
	t_render_cubie	cubies[26];
}	t_render_scene;

/// @brief Configuración de una sola vez: asigna a cada uno de los 26
///        slots su posición fija en la rejilla, y luego llama a
///        geometry_sync() contra SOLVED_CUBE.
void	geometry_init(t_render_scene *scene);

/// @brief Resync completo: recalcula los colores de `face[]` de cada
///        cubie a partir del cubo lógico ACTUAL. Barato (26 cubies,
///        lecturas de array planas), y se llama después de cada
///        movimiento confirmado — nunca se actualiza incrementalmente,
///        así que no existe ninguna ruta de contabilidad que pueda
///        desincronizarse de `cube`.
void	geometry_sync(t_render_scene *scene, const t_cube *cube);
```

Convención de ejes (se elige una vez, se escribe aquí, y no se revisita nunca): **+X = R, +Y = U, +Z = F** (mano derecha). La pegatina de `FACE_RIGHT` mira hacia +X, `FACE_UP` hacia +Y, `FACE_FRONT` hacia +Z, y las otras tres son las direcciones negativas.

### 2.4 El algoritmo de color — no hace falta la cadena de facelets

El nombre de cada esquina y arista ya indica sus colores en estado resuelto, porque los nombres están escritos letra-de-cara por letra-de-cara (`CORNER_URF` toca U, R, F). Construid una tabla estática por tipo de pieza, directamente a partir del orden de enum ya existente en `cube.h`:

```c
/// Indexado por t_corner. Cada entrada son los 3 colores de pegatina de
/// la pieza, siempre listados empezando por su pegatina de tipo U/D
/// (según cómo se define corner_orient en 02a-cube-notation.md §4), y
/// luego las otras dos en un orden horario fijo (visto desde fuera de
/// la esquina).
static const Color	CORNER_COLORS[CORNER_COUNT][3] = {
	[CORNER_URF] = {WHITE_C, RED_C,   GREEN_C},
	[CORNER_UFL] = {WHITE_C, GREEN_C, ORANGE_C},
	[CORNER_ULB] = {WHITE_C, ORANGE_C, BLUE_C},
	[CORNER_UBR] = {WHITE_C, BLUE_C,  RED_C},
	[CORNER_DFR] = {YELLOW_C, GREEN_C, RED_C},
	[CORNER_DLF] = {YELLOW_C, ORANGE_C, GREEN_C},
	[CORNER_DBL] = {YELLOW_C, BLUE_C, ORANGE_C},
	[CORNER_DRB] = {YELLOW_C, RED_C,  BLUE_C},
};

/// Indexado por t_edge. Cada entrada es {pegatina tipo U/D-o-F/B, la otra}.
static const Color	EDGE_COLORS[EDGE_COUNT][2] = {
	[EDGE_UR] = {WHITE_C, RED_C},   [EDGE_UF] = {WHITE_C, GREEN_C},
	[EDGE_UL] = {WHITE_C, ORANGE_C},[EDGE_UB] = {WHITE_C, BLUE_C},
	[EDGE_DR] = {YELLOW_C, RED_C},  [EDGE_DF] = {YELLOW_C, GREEN_C},
	[EDGE_DL] = {YELLOW_C, ORANGE_C},[EDGE_DB] = {YELLOW_C, BLUE_C},
	[EDGE_FR] = {GREEN_C, RED_C},   [EDGE_FL] = {GREEN_C, ORANGE_C},
	[EDGE_BL] = {BLUE_C, ORANGE_C}, [EDGE_BR] = {BLUE_C, RED_C},
};
```

(`WHITE_C`, etc. son vuestras propias constantes `Color`, convención WCA: U=blanco, D=amarillo, F=verde, B=azul, R=rojo, L=naranja — según `02a-cube-notation.md` §1. Con sufijo `_C` aquí solo para no chocar con los propios `WHITE`/`RED`/etc. de raylib — usad el que prefiráis, solo sed consistentes.)

El algoritmo de `geometry_sync()`, por cada slot de esquina `s` (0..7) y calcado para aristas:

1. `piece = cube->corner_perm[s]` — qué pieza ocupa actualmente el slot `s`.
2. `twist = cube->corner_orient[s]` — 0, 1, o 2.
3. Rotar `CORNER_COLORS[piece]` a la izquierda `twist` posiciones para obtener los 3 colores **en orden de slot** (es decir, qué color está ahora en el lado que mira a U/D del slot, cuál en el siguiente, cuál en el último) — una rotación fija de 3 entradas, trivial.
4. Las 3 direcciones-mundo visibles del slot `s` son estáticas y ya se conocen (p. ej., la posición del slot `CORNER_URF` es siempre `(1,1,1)`, así que sus 3 caras visibles son siempre `FACE_RIGHT, FACE_UP, FACE_FRONT` en ese orden fijo). Asignad los 3 colores rotados a esas 3 direcciones fijas en `scene->cubies[piece]`.

Aristas: la misma idea con 2 colores y `edge_orient` (0 o 1: intercambiar o no).

Centros: 6 entradas estáticas, nunca tocadas por `geometry_sync()` tras el `geometry_init()` que las fija una vez — un giro de cara nunca permuta ni reorienta un centro.

**Verificarlo empíricamente, una vez:** el "orden horario" exacto y a qué dirección de rotación física corresponde `twist == 1` es exactamente el tipo de cosa contra la que avisa el propio comentario de cabecera de `cube/moves.c` ("estos valores NO se escribieron de memoria... se comprobaron con tests"). Haced lo mismo aquí: renderizad `SOLVED_CUBE`, aplicad una `R` vía `apply_move()` + `geometry_sync()`, y confirmad visualmente que los colores caen donde los mostraría un cubo real girado. Si la dirección de rotación del twist está al revés, es un arreglo de una línea (rotar a la derecha en vez de a la izquierda) — barato de equivocarse y barato de arreglar, pero solo si de verdad miráis el primer render en vez de asumirlo.

### 2.5 `app.c` — el bucle

```c
bool	render_run(const t_cube *start_cube, bool has_scramble)
{
	t_render_scene	scene;
	t_cube			cube;
	Camera3D		camera;

	InitWindow(1280, 720, "rubik — 3D bonus");
	camera = (Camera3D){ .position = {6, 6, 8}, .target = {0, 0, 0},
		.up = {0, 1, 0}, .fovy = 45, .projection = CAMERA_PERSPECTIVE };
	cube = *start_cube;
	geometry_init(&scene);
	geometry_sync(&scene, &cube);
	while (!WindowShouldClose())
	{
		// La Fase 3 añade aquí órbita de cámara + entrada de teclado.
		// La Fase 1 añade aquí anim_update().
		BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode3D(camera);
		draw_scene(&scene);
		EndMode3D();
		EndDrawing();
	}
	CloseWindow();
	return (true);
}
```

`draw_scene()` de `draw.c` para esta fase: por cada uno de los 26 cubies, dibujar un `DrawCube` en `(x*SPACING, y*SPACING, z*SPACING)` de tamaño ligeramente menor que `SPACING` (para que se vea una línea de rejilla negra entre cubies — gratis, y se ve mucho mejor que cubos que se tocan), luego `DrawCubeWires` en gris oscuro para una línea de borde sutil. El detalle de color a nivel de pegatina (§4.2 en la Fase 5) llega después; para la Fase 0, colorear toda la cara visible del cubie con el color de pegatina dominante basta para demostrar que la pipeline funciona.

### 2.6 Gate de salida

- `make bonus && ./rubik_bonus -r` abre una ventana mostrando un cubo resuelto con los colores WCA correctos en las 6 caras, coincidiendo exactamente con la disposición de un cubo resuelto real (comprobad a ojo unos cuantos slots contra las tablas de §4.2).
- `./rubik_bonus "R U R' U'" -r` abre igual (el Modo A todavía no anima — eso es la Fase 1 — pero nada casca, y la ventana al menos abre sobre la posición *resuelta* de partida en vez de la posición ya revuelta... en realidad para la Fase 0 está bien mostrar siempre `SOLVED_CUBE` sin importar `has_scramble`; conectar la secuencia real de scramble-luego-resolver-luego-reproducir es trabajo de la Fase 1).
- El ratón puede orbitar (el `UpdateCamera(&camera, CAMERA_ORBITAL)` de serie de raylib vale como marcador de posición de la Fase 0 — la Fase 3 lo sustituye por la versión personalizada de solo-arrastre-derecho para la separación por botón).

**Esfuerzo:** ~1 día.

## Fase 1 — Modo A: reproducción automática de la lista de movimientos del solver

**Objetivo:** `./rubik_bonus "<scramble>" -r` resuelve in-process y anima la solución, con cada movimiento girando correctamente y confirmándose sin deriva.

**Depende de:** Fase 0.

### 3.1 Archivos nuevos

```
include/render/anim.h   NUEVO
src/render/anim.c       NUEVO
```

### 3.2 Estructuras de datos

```c
typedef enum e_axis { AXIS_X, AXIS_Y, AXIS_Z }	t_axis;

/// Una entrada por cada t_move (18), calculada una vez, a mano, a partir
/// de la convención de ejes de la Fase 0 (+X=R, +Y=U, +Z=F) y el sufijo
/// de cada movimiento (1=90 grados en un sentido, 2=180, 3=90 en el
/// otro). `layer` es qué capa de ese eje toca este movimiento: +1 o -1
/// (un cuarto/medio giro de cualquiera de los 18 movimientos siempre
/// toca exactamente una capa exterior — nunca la del medio, ya que
/// parse_notation() rechaza M/E/S).
typedef struct s_move_axis
{
	t_axis	axis;
	int8_t	layer;
	float	quarter_deg;	// con signo: el ángulo que cubre UN paso de
							// 90 grados de este movimiento; el signo está
							// por verificar empíricamente por eje (ver
							// nota abajo), la magnitud siempre es 90 o
							// 180 según el sufijo del movimiento.
}	t_move_axis;

# define ANIM_QUEUE_CAP MAX_MOVES	// reutiliza la misma cota de
									// parse.h — una solución en cola
									// nunca puede ser más larga que la
									// entrada más larga que este
									// programa acepta en cualquier otro
									// sitio.

typedef struct s_anim_state
{
	t_move	queue[ANIM_QUEUE_CAP];
	size_t	head;
	size_t	tail;
	bool	active;
	t_move	current;
	float	elapsed_sec;
	float	duration_sec;		// |grados| / speed_deg_per_sec
	float	angle_deg;			// 0 -> target_deg a lo largo de
								// duration_sec
	float	target_deg;
	float	speed_deg_per_sec;	// la Fase 2 lo expone como el dial de
								// velocidad
	bool	paused;				// Fase 2
}	t_anim_state;

void	anim_init(t_anim_state *s);
bool	anim_push(t_anim_state *s, t_move move);	// false si la cola
													// está llena
void	anim_update(t_anim_state *s, t_render_scene *scene, t_cube *cube,
			float dt);
bool	anim_is_idle(const t_anim_state *s);	// sin movimiento activo,
												// cola vacía
```

### 3.3 La tabla MOVE_AXIS

```c
static const t_move_axis	MOVE_AXIS[MOVE_COUNT] = {
	[MOVE_U1] = {AXIS_Y,  1,  90}, [MOVE_U2] = {AXIS_Y,  1, 180},
	[MOVE_U3] = {AXIS_Y,  1, -90},
	[MOVE_D1] = {AXIS_Y, -1, -90}, [MOVE_D2] = {AXIS_Y, -1, 180},
	[MOVE_D3] = {AXIS_Y, -1,  90},
	[MOVE_R1] = {AXIS_X,  1, -90}, [MOVE_R2] = {AXIS_X,  1, 180},
	[MOVE_R3] = {AXIS_X,  1,  90},
	[MOVE_L1] = {AXIS_X, -1,  90}, [MOVE_L2] = {AXIS_X, -1, 180},
	[MOVE_L3] = {AXIS_X, -1, -90},
	[MOVE_F1] = {AXIS_Z,  1,  90}, [MOVE_F2] = {AXIS_Z,  1, 180},
	[MOVE_F3] = {AXIS_Z,  1, -90},
	[MOVE_B1] = {AXIS_Z, -1, -90}, [MOVE_B2] = {AXIS_Z, -1, 180},
	[MOVE_B3] = {AXIS_Z, -1,  90},
};
```

**Los grados con signo de arriba son una hipótesis de partida, no un hecho verificado** — exactamente igual que las propias `FACE_TABLES` de `moves.c`, el signo real solo importa una vez que lo veis girar en pantalla. El patrón a comprobar: el sufijo "1" de una cara (horario, visto desde fuera de esa cara) y su sufijo "3" (antihorario) siempre deben tener signos opuestos entre sí para la *misma* cara, y el sentido horario-visto-desde-fuera de `U`/`R`/`F` *no* es automáticamente el mismo signo, en una convención de ejes-mundo compartida, que el de `D`/`L`/`B` (que es exactamente por qué la tabla de arriba ya invierte el signo entre, p. ej., `U1`=+90 y `D1`=-90 — U y D giran "en el mismo sentido" como rotaciones del eje-Y del mundo solo si sois consistentes sobre desde qué cara estáis mirando). Construidla como una hipótesis a partir de la tabla de arriba, girad una `R` en pantalla, y corregid cualquier signo equivocado — hay como mucho 6 signos que acertar (uno por cara; los movimientos con sufijo `2` son siempre ±180 en cualquier caso, así que su signo nunca importa visualmente).

### 3.4 El algoritmo de `anim_update()`

```
si no está activo:
    si la cola está vacía: return
    current = pop_front(queue)
    elapsed_sec = 0
    axis_info = MOVE_AXIS[current]
    target_deg = axis_info.quarter_deg
    duration_sec = |target_deg| / speed_deg_per_sec
    active = true

si está activo y no en pausa:
    elapsed_sec += dt
    frac = clamp(elapsed_sec / duration_sec, 0, 1)
    eased = ease_in_out_cubic(frac)       // ver abajo
    angle_deg = target_deg * eased
    si frac >= 1.0:
        apply_move(cube, current)          // LA función ya existente de cube.h
        geometry_sync(scene, cube)         // resync completo, ver Fase 0 §2.4
        active = false
        angle_deg = 0
```

`ease_in_out_cubic(t)` — una curva estándar, pequeña, bien conocida, segura de dejar fija en el código:

```c
static float	ease_in_out_cubic(float t)
{
	if (t < 0.5f)
		return (4.0f * t * t * t);
	return (1.0f - powf(-2.0f * t + 2.0f, 3) / 2.0f);
}
```

(Esto necesita `-lm`; `LDLIBS_BONUS` en el Makefile ya enlaza `-lm` en Linux — confirmad que la rama de macOS también resuelve `powf` a través de sus propios frameworks, que lo hace vía libSystem, así que tampoco hace falta cambio de Makefile en ningún caso.)

### 3.5 Cambio en `draw.c` — la rotación transitoria

`draw_scene()` ahora recibe el `t_anim_state` actual (o solo su `axis`/`layer`/`angle_deg` cuando está activo) y, para cualquier cubie cuyo `(x, y, z)` fijo coincida con el `axis`/`layer` del movimiento que se está animando, lo dibuja con una rotación *extra* de `angle_deg` alrededor de la línea central de ese eje antes de la traslación normal por cubie — la secuencia `rlPushMatrix()` / `rlRotatef(angle_deg, axis_x, axis_y, axis_z)` / `rlTranslatef(...)` / dibujar / `rlPopMatrix()` de raylib, o de forma equivalente `DrawModelEx` con un eje y ángulo de rotación explícitos si usáis modelos cargados en vez de llamadas directas a `DrawCube`. Cualquier otro cubie se dibuja exactamente igual que en la Fase 0 — sin tocar.

### 3.6 Conectar `render_run()` para el Modo A

Cuando `has_scramble` es true: tras `geometry_init`/`geometry_sync`, ejecutad el solver in-process exactamente como hace `solve_and_print()` (reutilizad `solver_init`/`solve`/`solver_free` de `solve.h` directamente — la misma forma de llamada anti-trampa, solo que capturando el array de movimientos en vez de imprimirlo), y luego `anim_push()` cada movimiento de la solución devuelta, en orden, antes de entrar en el bucle. El cuerpo del bucle gana una línea: `anim_update(&anim, &scene, &cube, GetFrameTime());`.

### 3.7 Gate de salida — el test de deriva del Sprint 6 de `06-roadmap-bonus.md`

Generad una secuencia aleatoria de 1.000 movimientos, `anim_push()` toda ella, dejadla reproducirse a alta velocidad (subid `speed_deg_per_sec` temporalmente solo para este test) sin interacción del usuario, y comprobad **byte a byte** que el `cube` final (el que `anim_update` ha ido mutando vía `apply_move`) es igual a lo que produce aplicar la misma secuencia de 1.000 movimientos directamente vía `cube_apply_moves()` sobre una copia fresca de `SOLVED_CUBE`. La deriva de coma flotante es imposible aquí por construcción — `angle_deg` es un valor puramente visual y desechable que se descarta y reinicia en cada confirmación; nunca retroalimenta a `cube`. Este test existe para detectar un bug de *lógica* (p. ej., un eje/capa equivocado elegido para algún movimiento), no el bug de *deriva aritmética* de coma flotante que el roadmap original temía — merece la pena ejecutarlo de todas formas, ya que una entrada equivocada en `MOVE_AXIS` de otro modo se quedaría invisible hasta que alguien vea por casualidad girar exactamente esa cara.

**Esfuerzo:** ~1.5 días (la mayor parte es la comprobación empírica de signos de §3.3).

## Fase 2 — Modo B: transporte de reproducción (Must)

**Objetivo:** pausa/reanudar, un solo paso, y un control de velocidad. Exactamente los tres controles Must del Modo B de `03a` §2 — el scrub, la reproducción inversa, y el bucle automático son de nivel Could y llegan en la Fase 7.

**Depende de:** Fase 1.

### 4.1 Archivos modificados

```
include/render/anim.h   MODIFICADO (sin campos nuevos — paused ya se
                         añadió en la struct de la Fase 1; esta fase es
                         la primera en usarlo de verdad)
src/render/anim.c       MODIFICADO
include/render/hud.h    NUEVO
src/render/hud.c        NUEVO
```

### 4.2 Controles

```c
void	anim_toggle_pause(t_anim_state *s);          // s->paused = !s->paused
void	anim_step_one(t_anim_state *s);               // si está en pausa y
                                                       // entre movimientos:
                                                       // fuerza el progreso
                                                       // de un anim_update()
                                                       // quitando la pausa
                                                       // temporalmente
                                                       // justo la duración
                                                       // restante del
                                                       // movimiento actual
void	anim_set_speed(t_anim_state *s, float deg_per_sec);
```

La implementación más limpia de `anim_step_one()`: no tratarlo como caso especial en absoluto — simplemente llamar a `anim_update(s, scene, cube, s->duration_sec - s->elapsed_sec + EPSILON)` una vez (un `dt` único, lo bastante grande para garantizar que `frac` llegue a 1.0 y confirme), y luego volver a poner `s->paused = true` inmediatamente. Esto reutiliza exactamente la misma ruta de confirmación que la reproducción normal en vez de una ruta de código paralela de "aplicar al instante" — consistente con el principio de "una sola ruta de código" de todo este plan, de `03a` §6.

Atajos de teclado (`IsKeyPressed` de raylib): `Espacio` = alternar pausa, `Flecha derecha` = paso, `Flecha arriba`/`abajo` = velocidad +/- (multiplicar `speed_deg_per_sec` por 1.25/0.8 en cada pulsación, limitando a un rango razonable, p. ej. 60-720 grados/seg).

### 4.3 `hud.c` — HUD base

```c
void	hud_draw(const t_anim_state *anim, const t_render_mode *mode,
			int move_count, int total_moves);
```

Dibuja, vía `DrawText`/`DrawRectangle` de raylib (superposición 2D, llamada *fuera* de `BeginMode3D`/`EndMode3D`, en espacio de pantalla): el movimiento actual en notación (buscadlo con el `format_moves()` ya existente sobre un array de un solo movimiento, o simplemente una pequeña tabla estática de strings), un contador de movimientos (`"movimiento 14 / 42"`), y una barra de progreso simple (`DrawRectangle` proporcional a `move_count/total_moves`). Esto es deliberadamente mínimo — las Fases 4 y 7 añaden más encima.

### 4.4 Gate de salida

Reproducir una resolución a velocidad por defecto, pausarla a mitad, avanzar paso a paso 3 movimientos individualmente (cada uno completando su giro por completo y confirmándose antes de que la siguiente pulsación de paso haga algo), reanudar, cambiar la velocidad dos veces, dejarla terminar. Sin crash, sin ningún movimiento saltado, sin ningún movimiento que "salte" visualmente en vez de animarse cuando se avanza paso a paso.

**Esfuerzo:** ~1 día.

## Fase 3 — Modo C: giros manuales por teclado + cámara con separación por botón (Must)

**Objetivo:** la petición real de "usarlo como un cubo real." Giro libre por teclado, órbita de cámara que nunca se pelea con él. Esta es la fase que el usuario pidió explícitamente construir primero, por completo, antes de empezar cualquier trabajo de arrastre de ratón.

**Depende de:** Fase 2 (reutiliza `anim_push`/`anim_update` sin cambios — los movimientos del Modo C entran en la misma cola exacta que la salida del solver del Modo A).

### 5.1 Archivos nuevos

```
include/render/input.h   NUEVO
src/render/input.c       NUEVO
```

### 5.2 Estructuras de datos

```c
typedef enum e_render_mode
{
	MODE_AUTOPLAY,	// una cola producida por el solver se está
					// reproduciendo o está pausada
	MODE_MANUAL,	// en reposo, esperando giros de teclado/ratón
}	t_render_mode;

typedef struct s_orbit_camera
{
	float	yaw_deg;
	float	pitch_deg;	// limitado a [-80, 80] para que la cámara no
						// pueda voltearse
	float	distance;	// limitado a un rango razonable [4, 20]
}	t_orbit_camera;

void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
			float dt);
t_move	input_poll_keyboard(void);	// devuelve MOVE_COUNT (18, el valor
									// centinela de "ningún movimiento
									// real") cuando no se pulsó nada
									// este frame
```

### 5.3 Órbita de cámara — la mitad de "solo arrastre derecho" de la separación por botón

```c
void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera, float dt)
{
	Vector2	delta;

	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		delta = GetMouseDelta();
		orbit->yaw_deg -= delta.x * 0.3f;
		orbit->pitch_deg = Clamp(orbit->pitch_deg - delta.y * 0.3f, -80, 80);
	}
	orbit->distance = Clamp(orbit->distance - GetMouseWheelMove(), 4, 20);
	camera->position = (Vector3){
		orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
			* sinf(DEG2RAD * orbit->yaw_deg),
		orbit->distance * sinf(DEG2RAD * orbit->pitch_deg),
		orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
			* cosf(DEG2RAD * orbit->yaw_deg),
	};
	camera->target = (Vector3){0, 0, 0};
}
```

Esto deliberadamente **no** usa el helper de serie `UpdateCamera(&camera, CAMERA_ORBITAL)` de raylib (que auto-rota o se ata al arrastre izquierdo según el modo) — la separación por botón necesita que el arrastre esté condicionado específicamente al botón *derecho*, así que son una docena de líneas de matemática de coordenadas esféricas hechas a mano en su lugar. `(void)dt;` por ahora — no hace falta hasta que se quiera un easing de la propia órbita independiente de la tasa de fotogramas, que es un extra agradable, no un requisito.

### 5.4 Entrada de notación por teclado

```c
static const struct { KeyboardKey key; t_move face_base; } FACE_KEYS[6] = {
	{KEY_U, MOVE_U1}, {KEY_R, MOVE_R1}, {KEY_F, MOVE_F1},
	{KEY_D, MOVE_D1}, {KEY_L, MOVE_L1}, {KEY_B, MOVE_B1},
};

t_move	input_poll_keyboard(void)
{
	int	i;

	i = 0;
	while (i < 6)
	{
		if (IsKeyPressed(FACE_KEYS[i].key))
		{
			if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
				return (FACE_KEYS[i].face_base + 2);	// ' — antihorario
			if (IsKeyDown(KEY_TWO))
				return (FACE_KEYS[i].face_base + 1);	// 2 — medio giro
			return (FACE_KEYS[i].face_base);			// simple — horario
		}
		i++;
	}
	return (MOVE_COUNT);
}
```

Nota sobre el chording que merece probarse a ojo una vez esté en marcha: mantener pulsado `2` y luego pulsar una tecla de cara funciona limpiamente con `IsKeyDown`, ya que `2` se comprueba como "mantenido actualmente," pero dos teclas de cara pulsadas en el mismo frame solo respetan la primera en el orden de `FACE_KEYS` — aceptable, un cubo real tampoco deja girar dos caras a la vez.

### 5.5 La máquina de estados de modo, en el bucle de `app.c`

```
cada frame:
    orbit_camera_update(&orbit, &camera, dt)
    si mode == MODE_AUTOPLAY:
        anim_update(&anim, &scene, &cube, dt)
        si IsKeyPressed(KEY_SPACE): anim_toggle_pause(&anim)
        si IsKeyPressed(KEY_ESCAPE) y anim no está inactivo:
            vaciar la cola (head = tail = 0), dejar que el movimiento en
            curso termine de confirmarse por su cuenta, luego mode = MODE_MANUAL
    si no, si mode == MODE_MANUAL:
        move = input_poll_keyboard()
        si move != MOVE_COUNT y anim_is_idle(&anim):
            anim_push(&anim, move)
        anim_update(&anim, &scene, &cube, dt)   // vacía la cola de
                                                 // profundidad 1
```

Los giros manuales pasan por **exactamente la misma pipeline `anim_push`/`anim_update`/`apply_move`** que la reproducción automática — esta es la frase más importante de toda esta fase. No hay ninguna segunda ruta de código de "aplicar este giro directamente, sin animación" en ningún sitio. Eso es lo que hace que una mezcla manual sea una entrada de confianza para el solver más adelante (el "resuélvelo tú" de la Fase 4), y es una continuación directa del principio anti-trampa ya presente en `01-requirements.md` y `04-architecture.md`.

### 5.6 Gate de salida — el test de consistencia anti-trampa, ejercitado desde el teclado

Con la ventana abierta en `MODE_MANUAL`, pulsad físicamente unas 25 teclas de cara aleatorias (mezcla de simple/shift/2), y luego volcad el `cube` resultante (un `printf` de depuración de `format_moves` no aplica aquí, ya que no hay lista de movimientos — imprimid la struct `t_cube` en crudo, o conectad temporalmente una tecla de depuración oculta que lo pase por `solve_and_print()` in-process e imprima a stdout/stderr mientras la ventana sigue abierta). Confirmad que el solver devuelve una solución válida y que esa solución, reproducida, produce `SOLVED_CUBE`. Este es exactamente el test que `06-roadmap-bonus.md` ya especifica como gate del Sprint 6 — la Fase 3 es donde de verdad se ejercita desde entrada de teclado real en vez de un scramble de test fijo.

**Esfuerzo:** ~1.5 días.


## Fase 4 — Extras del modo práctica: mezclar, cronómetro, contador, deshacer/rehacer, "resuélvelo tú" (Should)

**Objetivo:** la capa de calidad de vida que hace que el Modo C se sienta como una funcionalidad terminada en vez de una demo técnica. Cinco añadidos pequeños e independientes.

**Depende de:** Fase 3.

### 6.1 Archivos nuevos

```
include/scramble.h      NUEVO — de nivel superior, no solo para render:
                         este es un módulo genuinamente útil en general
                         (es también exactamente el "generador de
                         scrambles integrado" nombrado como su propio
                         ítem en el Sprint 7 #2 de 06-roadmap-bonus.md),
                         así que no vive bajo include/render/.
src/cube/scramble.c     NUEVO
include/render/history.h NUEVO
src/render/history.c    NUEVO
```

### 6.2 Generador de scrambles

```c
/// @brief Rellena `out[0..count)` con una secuencia de movimientos legal
///        y aleatoria, sin dos movimientos consecutivos en la misma cara
///        (la única regla de calidad que importa para "esto parece un
///        scramble de verdad" — los scramblers de nivel WCA hacen más,
///        pero este proyecto no necesita ese listón).
///
/// @param out   Buffer de al menos `count` movimientos.
/// @param count Cuántos movimientos generar.
/// @param seed  Estado del PRNG, entrada/salida (lo posee quien llama —
///              sembradlo una vez desde time(NULL) al arrancar el
///              programa, no en cada llamada).
void	scramble_generate(t_move *out, size_t count, unsigned int *seed);
```

Algoritmo: `rand_r(seed) % MOVE_COUNT` como candidato; si `candidate / 3 == previous / 3` (misma cara que el último movimiento), volver a tirar. No hace falta nada más — esto es deliberadamente la versión simple, no un scrambler compatible con WCA.

En el Modo C, una pulsación de "mezclar" (p. ej. `S`) llama a `scramble_generate()` para una longitud fija (20 es un valor por defecto habitual), luego hace `anim_push()` de todos ellos y cambia a `MODE_AUTOPLAY` justo el tiempo necesario para reproducir visualmente la mezcla — reutilizando exactamente la misma ruta de reproducción que una resolución, lo cual es gratis una vez que existen las Fases 1-2. En cuanto la cola se vacía, se vuelve a `MODE_MANUAL`.

### 6.3 Cronómetro, contador de movimientos, TPS

Tres campos sencillos añadidos a cualquier estado de nivel superior que `app.c` ya posea (no `t_anim_state` — estos son estadísticas de sesión, no estado de animación):

```c
typedef struct s_session_stats
{
	double	timer_start_sec;	// GetTime() cuando empezó el cronómetro;
								// -1 si está inactivo
	int		move_count;
	bool	timing;
}	t_session_stats;
```

Empezar a cronometrar en el primer movimiento manual tras una mezcla (es decir, la primera llamada a `anim_push` en `MODE_MANUAL` tras completarse una mezcla), pararlo en el instante en que `cube_is_solved(&cube)` se vuelve true (comprobado una vez por frame en `MODE_MANUAL`, es una comparación de struct O(1), gratis de llamar cada frame). El TPS es simplemente `move_count / (GetTime() - timer_start_sec)`, calculado solo para mostrarlo, nunca guardado.

### 6.4 Deshacer / rehacer

```c
# define HISTORY_CAP MAX_MOVES

typedef struct s_history
{
	t_move	undo_stack[HISTORY_CAP];
	size_t	undo_top;
	t_move	redo_stack[HISTORY_CAP];
	size_t	redo_top;
}	t_history;

/// @brief El inverso exacto de un movimiento — derivado directamente del
///        diseño del enum MOVE_U1/U2/U3 (cara * 3 + giro, giro 0=horario,
///        1=180, 2=antihorario): invertir simplemente intercambia
///        horario<->antihorario y deja el 180 igual.
static inline t_move	move_inverse(t_move m)
{
	return ((t_move)((m / 3) * 3 + (2 - m % 3)));
}

void	history_record(t_history *h, t_move applied);	// apila applied en
														// undo_stack,
														// vacía redo_stack
bool	history_undo(t_history *h, t_move *out);		// desapila
														// undo_stack,
														// apila su
														// inverso en
														// redo_stack,
														// *out = inverso
														// para
														// anim_push()
bool	history_redo(t_history *h, t_move *out);		// desapila
														// redo_stack,
														// lo vuelve a
														// apilar en
														// undo_stack,
														// *out = el
														// movimiento
														// original
```

Llamad a `history_record()` justo en el punto donde `MODE_MANUAL` llama a `anim_push()` para un movimiento por teclado (no para movimientos de reproducción automática/mezcla — deshacer es un concepto solo del Modo C). `Ctrl+Z` / `Ctrl+Y` (o `Z`/`Y` a secas, a vuestro gusto) manejan `history_undo`/`history_redo`, cada uno empujando el movimiento resultante por la ruta normal de `anim_push()` — de nuevo, sin una segunda ruta de código de "aplicar directamente".

### 6.5 "Resuélvelo tú"

```c
/// @brief Ejecuta el solver in-process ya existente contra el cubo vivo
///        ACTUAL y encola su salida para reproducción automática. No
///        hace nada si ya está resuelto. Calca la pipeline de
///        solve_and_print() exactamente, menos el printf.
static void	solve_for_me(t_cube *cube, t_anim_state *anim,
				t_render_mode *mode)
{
	t_solver	solver;
	t_move		solution[SOLVE_MAX_MOVES];
	int			count;
	int			i;

	if (cube_is_solved(cube))
		return ;
	if (!solver_init(&solver))
		return ;
	count = solve(&solver, cube, solution);
	solver_free(&solver);
	if (count < 0)
		return ;
	i = 0;
	while (i < count)
		anim_push(anim, solution[i++]);
	*mode = MODE_AUTOPLAY;
}
```

Asociado a una tecla dedicada (p. ej. `Enter`). Nótese que esto llama a `solve()` sobre el `cube` *en vivo, actualmente mostrado* — la misma variable que el código de teclado/deshacer/mezcla del Modo C lleva mutando vía `apply_move()` todo el rato, nunca una copia separada de "lo que el usuario cree que es el estado". Ese único `t_cube` compartido es lo que hace que este botón sea de fiar.

### 6.6 Gate de salida

Mezclar con la tecla `S`, ver arrancar el contador de movimientos y el cronómetro, resolver la mitad a mano, deshacer 3 movimientos, rehacer 1, y luego pulsar "resuélvelo tú" y ver cómo termina y el cronómetro se para en el instante exacto en que el cubo se lee como resuelto. Sin desincronización entre el contador de movimientos del HUD y el número real de movimientos confirmados.

**Esfuerzo:** ~2.5 días combinados (mezcla ~0.5, cronómetro/contador ~0.5, deshacer/rehacer ~0.5, resuélvelo-tú ~0.5, conexión de HUD para todo ello ~0.5).

## Fase 5 — Base de acabado visual: iluminación + separación de pegatinas (Should)

**Objetivo:** los dos cambios que `03a` §4.1-4.2 marca como el mayor valor visual por hora. Puramente una fase de `draw.c` — ningún archivo de lógica/jugabilidad cambia.

**Depende de:** Fase 0 (solo geometría/dibujado; independiente de las Fases 1-4, se puede construir en paralelo por el otro dev).

### 7.1 Shader de iluminación

raylib incluye un ejemplo funcional de exactamente esto en su propia carpeta `examples/shaders/` (`rlights.h` más un par GLSL `lighting.vs`/`lighting.fs`) — **localizad los nombres de archivo actuales en el árbol vendorizado `lib/raylib/examples/shaders/`** (las rutas exactas pueden variar ligeramente entre versiones de raylib) y copiad los tres archivos a un nuevo directorio `assets/shaders/` en este repo, en vez de acceder al árbol de ejemplos de `lib/` en tiempo de ejecución. Cargadlo una vez en la configuración de `render_run()`:

```c
Shader	lighting = LoadShader("assets/shaders/lighting.vs",
					"assets/shaders/lighting.fs");
// CreateLight(...) de rlights.h para 1-2 luces, según su propio código
// de ejemplo.
```

y luego asignad a cada material de cubie el shader `lighting` en vez del material sin iluminar por defecto, antes de que empiece el bucle de dibujado. Esto es trabajo de copiar-adaptar del propio ejemplo de raylib, no diseño de algoritmo nuevo — presupuestad tiempo para leer ese ejemplo una vez, no para inventar código de shader desde cero.

### 7.2 Separación entre pegatinas / marco de plástico

Sustituid el único `DrawCube` por cubie (el marcador de posición de la Fase 0) por dos dibujados:

1. Un `DrawCube` gris oscuro/negro ligeramente más grande — el "cuerpo de plástico."
2. Por cada entrada poblada de `face[]` del cubie (es decir, no las direcciones internas/ocultas), un quad de color fino — bien `DrawCubeV` con una profundidad casi nula (p. ej. `0.85 * cubie_size` de ancho/alto, `0.02 * cubie_size` de profundidad) posicionado en el centro de la cara, desplazado hacia fuera la mitad del tamaño del cuerpo más un pequeño épsilon (para evitar z-fighting), o un quad crudo con `DrawTriangleStrip3D` si queréis evitar del todo la dimensión de profundidad. Cualquiera de las dos vale; la versión de cubo es menos código.

Esto es puramente aditivo a `draw_scene()` — ningún otro archivo cambia.

### 7.3 Gate de salida

Solo visual: el cubo renderizado debería leerse como "un cubo de plástico real con iluminación" en vez de "cajas de colores planas", en comparación lado a lado con una captura de la Fase 0. Sin test funcional — esta fase no puede romper nada del lado de la lógica ya que nunca toca `t_cube`, `t_anim_state`, ni los campos `x/y/z` de `t_render_scene`, solo cómo se dibujan los colores de `face[]`.

**Esfuerzo:** ~1 día (medio de iluminación, medio de separación de pegatinas).

## Fase 6 — Giro por clic-y-arrastre de ratón (Could)

**Objetivo:** completa la separación por botón. Arrastre izquierdo sobre una pegatina gira su capa; arrastre derecho (ya construido en la Fase 3) sigue orbitando. Empezad esto solo una vez que la ruta de teclado de la Fase 3 haya pasado su gate — según la decisión de secuenciación explícita del usuario en el §0 de arriba.

**Depende de:** Fase 3.

### 8.1 Archivos modificados/nuevos

```
include/render/input.h   MODIFICADO — añade la API de arrastre de abajo
src/render/input.c       MODIFICADO
```

### 8.2 Estructuras de datos

```c
typedef struct s_drag_state
{
	bool	active;
	t_axis	axis;
	int8_t	layer;
	Vector3	plane_right;	// vectores unitarios que abarcan el plano
	Vector3	plane_up;		// local de la cara clicada, en espacio mundo
	float	accum_deg;		// rotación en vivo, sin confirmar, mientras
							// se arrastra
}	t_drag_state;

bool	input_pick_start(t_drag_state *drag, const t_render_scene *scene,
			Camera3D camera);				// llamar al pulsar el
											// botón izquierdo
void	input_pick_drag(t_drag_state *drag, Vector2 mouse_delta);
t_move	input_pick_release(t_drag_state *drag);	// devuelve MOVE_COUNT
													// si el arrastre fue
													// demasiado pequeño
													// para contar como un
													// giro (tratarlo como
													// un no-op)
```

### 8.3 `input_pick_start()` — raycasting e identificación de cara

1. `Ray ray = GetMouseRay(GetMousePosition(), camera);`
2. Para cada uno de los 26 cubies, construid su `BoundingBox` a partir de `(x, y, z) * SPACING ± half_size` y llamad a `GetRayCollisionBox(ray, box)`. Seguid la colisión más cercana (`RayCollision.distance`).
3. En la colisión más cercana, `RayCollision.normal` da la dirección de cara en espacio mundo que se clicó (una de las 6 direcciones unitarias de eje). Esa dirección, junto con el `(x, y, z)` fijo del cubie, determina `axis` (el eje perpendicular a la cara clicada — un clic en `FACE_RIGHT`/`FACE_LEFT` implica que el eje de arrastre es Y o Z, decidido en el *siguiente* paso, no en este) y confirma qué `layer` de ese plano tocaría el giro eventual (el valor de coordenada compartido por toda la cara clicada — p. ej., clicar en cualquier punto de un cubie situado en `x=1` con normal `+X` significa que el giro, sea cual sea, toca `layer = 1` en cualquiera de los ejes Y/Z que resuelva el arrastre).
4. Calculad `plane_right`/`plane_up`: dos vectores unitarios en espacio mundo que abarcan el plano de la cara clicada (p. ej., para una cara con normal `+X`, `plane_right = (0,0,1)` y `plane_up = (0,1,0)` bajo la convención de ejes de este documento). Esto es sobre lo que el siguiente paso proyecta el arrastre.

### 8.4 `input_pick_drag()` — resolver el eje de giro a partir de la dirección de arrastre

Cada frame mientras el botón izquierdo está pulsado:

1. Proyectad `mouse_delta` (espacio de pantalla 2D) sobre `plane_right` y `plane_up` — la forma correcta más barata: usad `GetWorldToScreen` sobre dos puntos (`cubie_center` y `cubie_center + plane_right`) una vez al empezar el arrastre para obtener una dirección 2D en espacio de pantalla para `plane_right`, y lo mismo para `plane_up`, y luego producto escalar del `mouse_delta` 2D crudo contra esas dos direcciones 2D para saber cuánto del arrastre va "a lo largo de right" contra "a lo largo de up."
2. El que de los dos tenga mayor magnitud decide el eje de rotación: un arrastre mayormente a lo largo de `plane_up` rota alrededor del eje definido por `plane_right` (y viceversa) — es la relación estándar de "arrastra perpendicular al eje sobre el que quieres rotar," la misma que impone la geometría de un cubo real.
3. Acumular `accum_deg += chosen_component * DRAG_SENSITIVITY` (una constante ajustada, empezad alrededor de `0.5` y ajustad a ojo).

### 8.5 `input_pick_release()` — encajar y resolver a un movimiento real

1. Encajar `accum_deg` al más cercano de `{-180, -90, 0, 90, 180}`.
2. Si el valor encajado es `0`, devolver `MOVE_COUNT` (arrastre demasiado pequeño — tratarlo como un no-evento, p. ej. el usuario simplemente clicó sin arrastrar de forma significativa).
3. Si no, buscad qué `t_move` tiene exactamente esa combinación `(axis, layer, quarter_deg)` en `MOVE_AXIS` (un recorrido lineal de 18 entradas sobra de rápido) y devolvedlo.
4. El bucle de `app.c` llama a esto al soltar el ratón; si el resultado no es `MOVE_COUNT`, pasa por **la misma ruta `anim_push()`** que todo lo demás usa — el propio arrastre fue puramente contabilidad visual en `t_drag_state`, sin tocar `cube` ni `scene` directamente jamás.

### 8.6 El único punto de integración en `draw.c`

Mientras `drag.active` es true, `draw_scene()` necesita mostrar la capa afectada siguiendo visualmente el arrastre *antes* de confirmarse — reutilizad exactamente el mismo mecanismo de rotación transitoria de la Fase 1 §3.5 (que ya sabe dibujar una capa rotada un ángulo arbitrario alrededor de un eje), solo que alimentado con `drag.accum_deg` en vez de `anim.angle_deg`. Sin código de dibujado nuevo — un `if` extra eligiendo cuál de los dos valores en vivo usar.

### 8.7 Gate de salida

Arrastrar-clic cada una de las 6 caras desde unos cuantos ángulos de cámara distintos (incluyendo desde "detrás," donde la dirección de arrastre que significa horario se invierte) y confirmar que cada una produce el movimiento pretendido — esto es inherentemente un pase de calibración a ojo, no algo que un test unitario pueda cubrir del todo; presupuestad tiempo real de prueba interactiva, no solo tiempo de implementación. Una vez que se sienta bien, volved a ejecutar el test de consistencia anti-trampa de la Fase 3 (§5.6) pero mezclando con arrastre de ratón en vez de teclado, para confirmar que los giros por ratón son exactamente tan de fiar como los de teclado.

**Esfuerzo:** ~2.5-3 días (el pase de calibración/sensación es el cuello de botella, no el código).

## Fase 7 — Resto del acabado Could-have

**Objetivo:** todo lo demás del nivel "Could" del MoSCoW de `03a`. Cada uno de estos es independiente de los demás — construidlos en cualquier orden, descartad cualquiera bajo presión de tiempo sin afectar al resto.

**Depende de:** Fase 4 (la línea de tiempo/reproducción inversa/bucle automático reutilizan el patrón del array de solución de `solve_for_me` y el `move_inverse()` de `history`); el resto depende solo de la Fase 0/5.

### 9.1 Presets de cámara + órbita automática en reposo

Tres o cuatro constantes `t_orbit_camera` fijas (p. ej. `{yaw: 45, pitch: 30, distance: 8}` para una vista isométrica estándar) asociadas a teclas numéricas, cada una interpolada durante ~0.3s en vez de saltar de golpe (reutilizad `ease_in_out_cubic` de la Fase 1). Órbita automática en reposo: si no hay entrada de teclado/ratón durante N segundos (seguid `last_input_time = GetTime()`, actualizado dondequiera que se detecte entrada), incrementad lentamente `orbit.yaw_deg` una pequeña constante por frame hasta que la entrada se reanude.

**Archivos:** solo `input.c`/`app.c`. **Esfuerzo:** ~0.5 día.

### 9.2 Temas de color / skins

```c
typedef struct s_palette { Color u, d, f, b, r, l; } t_palette;
static const t_palette PALETTE_CLASSIC = { WHITE_C, YELLOW_C, GREEN_C, BLUE_C, RED_C, ORANGE_C };
static const t_palette PALETTE_COLORBLIND_SAFE = { /* cambiar rojo/verde
	por algo más distinguible, p. ej. un esquema cargado hacia
	azul/naranja */ };
```

Las tablas `CORNER_COLORS`/`EDGE_COLORS` de `geometry.c` (§4.2) pasan a generarse a partir de la `t_palette` activa, en vez de literales fijos a mano — una pequeña refactorización (construir las tablas a partir de los 6 colores base con la misma lógica de letra-U/R/F/D/L/B-por-nombre ya descrita, en vez de escribirlas literalmente), y luego una tecla rápida cicla `active_palette` y llama a `geometry_sync()` una vez para repintar.

**Archivos:** `geometry.c`/`geometry.h`. **Esfuerzo:** ~0.5 día.

### 9.3 Sonido de giro + celebración al resolver ("juice")

```c
include/render/fx.h   NUEVO
src/render/fx.c        NUEVO
```

`InitAudioDevice()` una vez al arrancar, `LoadSound("assets/sfx/turn.wav")` (cualquier clic/whoosh corto — grabad uno o conseguid uno CC0), `PlaySound()` en cada confirmación dentro de la rama de confirmación de `anim_update()` (Fase 1 §3.4 — una línea extra ahí). Celebración: un pequeño array de tamaño fijo de "partículas" (`Vector3 pos, vel; float life;`), generadas con velocidades aleatorias hacia fuera en el instante en que `cube_is_solved()` pasa a true, cada una dibujada como una `DrawSphere` que se encoge y actualizada (`pos += vel * dt; life -= dt`) hasta que `life <= 0`.

**Esfuerzo:** ~0.5 día.

### 9.4 Geometría de cubies con esquinas redondeadas

La versión más barata que aún se ve bien: mantened el enfoque de "cuerpo de plástico + quads de pegatina insertados" de la Fase 5, pero redondead las esquinas del *cuerpo* mediante una pequeña malla pre-cocinada (un cubo biselado, generado una vez — p. ej. con un script que use `GenMeshCube` de raylib como base y desplace manualmente los vértices de las esquinas, o una herramienta externa que exporte un `.obj`/`.glb` cargado una vez con `LoadModel`) en vez de intentar fingir el redondeo con texturas. Este es el ítem más variable en tiempo de toda la lista — tratad la estimación como blanda.

**Esfuerzo:** ~1 día, podría alargarse.

### 9.5 Sombra de suelo / skybox

Sombra de suelo: un `DrawCircle3D` (o un `DrawCube` aplastado) semitransparente y oscuro en `y = -SPACING*1.6` bajo el cubo, dibujado antes que el propio cubo. Skybox: la versión más simple es una esfera grande orientada hacia dentro (`DrawSphere` con un truco/flag para renderizar su interior, o simplemente un degradado vertical fingido con un `ClearBackground` mediante un quad a pantalla completa dibujado primero en 2D) — la versión de degradado es mucho más barata que un skybox con textura real y aquí se ve bien igual.

**Esfuerzo:** ~0.5 día.

### 9.6 Scrub de movimientos/línea de tiempo (solo botones/cuadro de texto, sin arrastre)

La versión "saltar al movimiento N", explícitamente **no** la versión con tirador arrastrable (esa está en el nivel Won't). Requiere que el array de la solución de la reproducción automática sobreviva más allá del punto en que se encola (actualmente la configuración del Modo A de `solve_for_me`/`render_run` mete los movimientos en la cola de `anim` y podría descartar el array — en vez de eso, conservadlo, más una copia guardada del estado del cubo al inicio de la resolución):

```c
t_move	solution_moves[SOLVE_MAX_MOVES];
int		solution_count;
t_cube	solution_start_cube;	// instantánea tomada una vez, antes de
								// que se reproduzca el movimiento 0
```

"Saltar al movimiento N": reiniciar `cube = solution_start_cube`, reproducir `solution_moves[0..N)` instantáneamente mediante un bucle ajustado de `apply_move()` (sin animación), llamar a `geometry_sync()` una vez al final, vaciar la cola de `anim` y meter `solution_moves[N..solution_count)` para que la reproducción se reanude desde ahí con normalidad. **Reproducción inversa**: meter `move_inverse(solution_moves[i])` (el mismo helper exacto de la Fase 4 §6.4) para `i` desde `solution_count-1` hasta `0`. **Bucle automático**: cuando la cola del `MODE_AUTOPLAY` se vacía y `cube_is_solved()`, esperar ~2 segundos (seguid un temporizador), y luego volved a llamar a `scramble_generate()` + una resolución y encolado al estilo `solve_for_me()`.

**Archivos:** principalmente `app.c`, un pequeño añadido a `hud.c` para la UI de "saltar al movimiento N" (un cuadro de texto o botones +10/-10, según la recomendación de `03a` §2 en contra de un tirador arrastrable). **Esfuerzo:** ~1 día.

## 10. Manifiesto de archivos consolidado

```
include/render.h                NUEVO  Fase 0  punto de entrada agnóstico de raylib
include/render/app.h            NUEVO  Fase 0
include/render/geometry.h       NUEVO  Fase 0
include/render/draw.h           NUEVO  Fase 0
include/render/anim.h           NUEVO  Fase 1
include/render/hud.h            NUEVO  Fase 2
include/render/input.h          NUEVO  Fase 3  (ampliado en la Fase 6)
include/scramble.h              NUEVO  Fase 4  (nivel superior, compartido)
include/render/history.h        NUEVO  Fase 4
include/render/fx.h             NUEVO  Fase 7

src/render/app.c                NUEVO  Fase 0  (modificado en cada fase posterior)
src/render/geometry.c           NUEVO  Fase 0  (ampliado en la Fase 7 §9.2)
src/render/draw.c               NUEVO  Fase 0  (modificado en las Fases 1,5,6)
src/render/anim.c               NUEVO  Fase 1
src/render/hud.c                NUEVO  Fase 2  (ampliado en las Fases 4,7)
src/render/input.c              NUEVO  Fase 3  (ampliado en la Fase 6)
src/cube/scramble.c             NUEVO  Fase 4  (nivel superior, compartido)
src/render/history.c            NUEVO  Fase 4
src/render/fx.c                 NUEVO  Fase 7

src/main.c                      MODIFICADO solo en la Fase 0 (flag -r,
                                 scramble opcional, una llamada nueva a
                                 render_run())

assets/shaders/{lighting.vs,lighting.fs,rlights.h}   Fase 5 (copiados de
                                 los propios ejemplos de raylib)
assets/sfx/turn.wav             Fase 7 (cualquier sonido corto, CC0 o
                                 grabado)
```

**Impacto en el Makefile: ninguno, en todas las fases.** `src/render/*.c` se autodescubre mediante la regla ya existente `RENDER_SRCS := $(shell find $(RENDER_DIR) -name '*.c' ...)` — un archivo nuevo bajo `src/render/` no necesita ningún cambio en el Makefile, exactamente como promete el propio comentario de cabecera del Makefile. `src/cube/scramble.c` se autodescubre igualmente mediante la regla `SRCS` obligatoria, lo cual está bien — es un módulo de propósito general, no solo de renderizado, y estar también en el binario obligatorio no cuesta nada (simplemente nunca se llama desde el camino obligatorio de `main()`). La única línea que alguna vez necesita una mano humana es el propio `src/main.c`, una vez, en la Fase 0.

## 11. Estimación de esfuerzo total

| Fase | Nivel | Esfuerzo |
|---|---|---|
| 0 — Esqueleto de renderizado | prerrequisito | ~1 día |
| 1 — Modo A reproducción automática | Must | ~1.5 días |
| 2 — Modo B transporte | Must | ~1 día |
| 3 — Modo C teclado | Must | ~1.5 días |
| 4 — Extras de práctica | Should | ~2.5 días |
| 5 — Base visual | Should | ~1 día |
| 6 — Arrastre de ratón | Could | ~2.5-3 días |
| 7 — Resto del acabado | Could | ~3.5 días (suma de §9.1-9.6) |
| **Total** | | **~14.5-15 días** para dos personas, la mayor parte paralelizable a lo largo de la misma costura `anim.c`/`geometry.c`+`draw.c` que `06-roadmap-bonus.md` ya define en el Sprint 6. |

Las Fases 0-3 (el nivel Must, ~5 días) son la parte que tiene que existir para que el bonus cuente en absoluto y para que "usable como un cubo real" sea cierto. Las Fases 4-5 (~3.5 días) merecen la pena si el presupuesto de tiempo del Sprint 7 lo permite. Las Fases 6-7 (~6-6.5 días) son genuinamente bonus-sobre-el-bonus — el orden MoSCoW dentro de ellas (giro por arrastre antes que acabado puro) sigue aplicando si solo cabe parte de las Fases 6-7.
