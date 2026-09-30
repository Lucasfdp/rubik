# Otros Puzzles — qué costó el 2×2×2, y por qué el 4×4×4 / Megaminx / Square-1 no

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

La lista de bonus del subject dice:

> Ways to work with other puzzles (4x4x4, 2x2x2, Megaminx, Square-1?)

Eso es una petición de mostrar una vía, no un mandato de entregar cuatro solvers. Dado R11 (el bonus vale cero salvo que la parte obligatoria sea perfecta) y que este ítem está explícitamente el último en la lista de prioridad de `06-roadmap-bonus.md`, el alcance tomado aquí es: **implementar el 2×2×2 de verdad** (resultó reutilizar casi todo lo ya construido), y **documentar, con precisión y contra el código real, por qué los otros tres no son una versión más pequeña del mismo trabajo**. Esa segunda mitad vale tanto en la defensa como la primera — demuestra que se entendió la arquitectura, no que se extendió por suerte.

## Lo que se entregó: 2×2×2

Se ejecuta con `-p 2x2x2` en el binario de bonus:

```
./rubik_bonus "R U2 F' D" -p 2x2x2
```

### Por qué salió barato

Un 2×2×2 tiene esquinas y nada más — sin aristas, sin centros. `t_cube` (`cube.h`) ya guarda esquinas y aristas como dos arrays totalmente separados (`corner_perm`/`corner_orient` frente a `edge_perm`/`edge_orient`), y `coord.h` ya tiene `encode_twist`/`decode_twist` (orientación de esquinas, 3⁷) y `encode_cperm`/`decode_cperm` (permutación de esquinas, 8!) como coordenadas que leen **solo** los arrays de esquinas. Así que "resolver un 2×2×2" se reduce a: aplicar el `apply_move()` de 18 movimientos de siempre, exactamente igual que antes (las aristas viajan de acompañantes y no se vuelven a mirar), y buscar la secuencia de movimientos más corta que lleve `(twist, cperm)` a `(0, 0)`.

Esa búsqueda es una única pasada de IDA*, no las dos fases de Kociemba — solo hay un objetivo (esquinas resueltas), así que no hay nada que una frontera de fase pueda dividir. Reutiliza, sin modificar:

- las mismas seis tablas de movimiento que ya produce `move_tables_build()` (solo se leen `->twist` y `->cperm`);
- `prune_build()` e `ida_search()` tal cual existen, porque un estado aquí se describe con la misma forma de tres coordenadas y dos pares de poda que `t_ida_phase` ya espera — la tercera "coordenada" es una constante que nunca cambia (`g_zero_table[MOVE_COUNT] = {0}`), lo que convierte cada par de poda en una tabla 1-D simple, para twist solo y para cperm solo. No hizo falta cambiar nada en `prune.c` ni en `ida.c`.

El módulo nuevo entero es `src/solve/two_by_two.c` (~70 líneas) más su cabecera — ver `include/twobytwo.h` para el contrato completo, en particular por qué la validación ahí es *solo* `cube_corner_twist_sum(cube) == 0` y no el `cube_is_valid()` completo que usa el camino del 3×3×3: un 2×2×2 no tiene aristas, así que no hay ningún invariante de paridad de permutación que enlace las esquinas con ninguna otra cosa que comprobar. Cualquier permutación de esquinas es alcanzable por sí sola.

### La cota de 11 movimientos

`TWOBYTWO_MAX_MOVES` es `11`, y no es una suposición. Resolver solo-esquinas de un 2×2×2 con el conjunto generador de 3 caras {U, R, F} (la convención habitual de speedcubing, ya que un 2×2×2 no tiene centros que distingan cuál de las seis caras se llama "R" frente a una rotación del cubo entero) tiene un diámetro probado de 11 en métrica half-turn — un resultado computacional ya establecido desde hace tiempo (enumeración completa por anchura del grupo de 3.674.160 estados), no algo re-derivado aquí. Nuestra representación es un superconjunto estricto de ese generador (los 18 movimientos, las seis caras distinguibles porque nuestro modelo de cubies mantiene un marco de referencia fijo exista o no un centro real en un 2×2×2) — un generador más grande solo puede encontrar una solución óptima igual de corta o más corta, nunca más larga, así que 11 sigue siendo una cota válida aquí también. `test_two_by_two.c` comprueba la longitud de la solución de cada scramble aleatorio contra esa cota directamente.

### Tests

`tests/test_two_by_two.c` (`make test_two_by_two`, incluido en `make test`): ya-resuelto, scrambles aleatorios resueltos y reverificados con `two_cube_is_solved()`, el invariante de giro de esquinas rechazando un estado inválido y — el test que realmente demuestra la afirmación de diseño — dos cubos con las mismas esquinas pero *distinta* permutación y orientación de aristas obtienen soluciones idénticas byte a byte, porque `two_solve()` nunca lee `edge_perm` ni `edge_orient`.

### El bonus 3D también lo renderiza

`render_run()` abre directamente con `-p 2x2x2 -r`, o una ventana en marcha cambia en vivo con **K** (`switch_puzzle()` en `render/app.c`), entre las dos vistas — el HUD siempre muestra cuál está activa, junto a las pistas de paleta/algoritmo/auto-loop.

La razón de que esto saliera barato es la misma que con el solver: `t_render_scene` de `render/geometry.c` ya mantiene sincronizadas, cada frame (`geometry_sync()`), las `RENDER_CUBIE_COUNT = 8 + 12 + 6` posiciones, esquinas primero (posiciones `0..CORNER_COUNT-1`), sin importar qué puzzle se muestre — una vista de 2×2×2 nunca necesitó una segunda escena, una segunda tabla de posiciones, ni un `geometry_init_2x2()`. Lo que cambia es *cuántas de esas posiciones se dibujan o se recogen con el ratón*, decidido cada frame por tres funciones auxiliares pequeñas y conscientes de `t_puzzle` en `geometry.h`:

- `geometry_visible_count()` — `RENDER_CUBIE_COUNT` para un 3×3×3, solo `CORNER_COUNT` para un 2×2×2, ya que sus posiciones de arista/centro existen y se siguen sincronizando pero nunca están entre las primeras N que un 2×2×2 dibuja;
- `geometry_body_size()` — `CUBIE_BODY_SIZE` normalmente, o `CUBIE_BODY_SIZE_2X2` (`= CUBIE_SPACING + CUBIE_BODY_SIZE`) para un 2×2×2, para que sus 8 cubies de esquina crezcan hasta llenar el hueco que antes ocupaban las (ahora invisibles) aristas, manteniendo la misma anchura de costura que el 3×3×3 tiene entre sus propios cubies — una sola constante derivada, sin tabla de posiciones nueva;
- `geometry_half_extent()` — el tamaño de caja delimitadora que necesita el raycast de una sola caja de `input_pick_start()` (`docs/en/11-drag-review.md` §1.4/B4) para acertar correctamente sobre los cubies más grandes del 2×2×2 al girarlos con el ratón.

`draw_scene()` itera hasta `visible_count` en vez de un `RENDER_CUBIE_COUNT` fijo, y escala el tamaño de sus stickers y los filetes de esquina redondeada por `body_size / CUBIE_BODY_SIZE`, para que los cubies más grandes de un 2×2×2 tengan stickers de tamaño proporcional en vez de los antiguos, pequeños, flotando sobre un cuerpo mucho mayor. El giro por arrastre del ratón en `input.c` no necesitó ningún cambio de lógica más allá de ese único tamaño de caja: una selección en un 2×2×2 siempre es una selección de esquina (las tres coordenadas de la rejilla son distintas de cero), lo que ya estaba encauzado por el camino ordinario de bloqueo de eje — la degradación ya existente para "no hay pieza de centro aquí" y "no hay capa media aquí" (`is_center_pick()`, `drag->blocked`) ya cubre un puzzle sin ninguna de las dos, porque se escribió para preguntar "qué dijo la geometría que toqué", nunca "qué puzzle es este".

Resolver sigue el mismo cambio: `solve_with_algo()` comprueba `app->puzzle` antes de `app->algo` — `PUZZLE_2X2X2` siempre llama a `two_solve()` (solo hay un algoritmo entre el que elegir), y `app_is_solved()` elige `two_cube_is_solved()` o el `cube_is_solved()` completo del mismo modo, así que el cronómetro de práctica, el efecto de celebración al resolver, y el auto-loop leen "resuelto" correctamente para el puzzle que esté en pantalla. Cambiar (K) nunca toca `app->cube` en sí: las dos vistas son el mismo cubo completo todo el tiempo, exactamente como ya coinciden `-p 2x2x2` y la resolución normal — un 2×2×2 es "mirar menos piezas de las mismas", no un cubo distinto.

## Por qué los otros tres no son "2×2×2, pero más"

La razón por la que el 2×2×2 salió casi gratis es que es un **subconjunto estricto** del modelo ya existente: menos tipos de pieza, los mismos tipos de pieza por lo demás, el mismo alfabeto de movimientos, la misma maquinaria de teoría de grupos. Los otros tres rompen, cada uno, un supuesto *distinto* de esos, y ninguno es un subconjunto.

### 4×4×4 — rompe el modelo de piezas y el alfabeto de movimientos

Un 4×4×4 no tiene los recuentos fijos de 8/12/6 que asumen `CORNER_COUNT`/`EDGE_COUNT`/`RENDER_FACE_COUNT`. Tiene 8 esquinas (bien, igual que el 3×3×3) pero 24 piezas de arista que vienen en *12 pares indistinguibles a simple vista* ("wings") y 24 piezas de centro en 6 grupos de 4 que llevan color pero ninguna identidad fija entre sí. Nada de eso encaja en `edge_perm[EDGE_COUNT]`/`edge_orient[EDGE_COUNT]` de `t_cube` — la "identidad" de un wing solo tiene sentido una vez emparejado con su pareja, y una pieza de centro no tiene ningún concepto de orientación en `t_cube` en absoluto (los centros se dejan fuera de `t_cube` a propósito ahora mismo, según su propio comentario, porque en un 3×3×3 "nunca se mueven entre sí, así que no llevan información" — falso en cuanto el puzzle tiene más de un centro por cara).

El alfabeto de movimientos también se rompe: `t_move` es un `enum` plano de exactamente `6 caras × 3 giros = 18`, y cada tabla de movimiento del código está dimensionada y direccionada como `state * MOVE_COUNT + move`. Un 4×4×4 necesita giros de *capa interior* (`Rw`, `2R`, o como se quiera notar) además de los giros de capa exterior — movimientos que no existen en absoluto en el alfabeto del 3×3×3, no valores extra de uno ya existente.

Y el método estándar de resolución (reducción: emparejar los wings de arista, agrupar los centros, y luego resolver lo que queda como un 3×3×3) introduce **casos de paridad que un 3×3×3 real nunca puede producir** — paridad de OLL y de PLL, que solo surgen porque dos piezas wing originalmente idénticas pueden acabar intercambiadas durante el emparejamiento. Los invariantes de `validate_cube()` (giro de esquina ≡ 0 mod 3, volteo de arista ≡ 0 mod 2, paridad de permutación coincidente) son leyes físicas del 3×3×3; un 4×4×4 con paridad a mitad de resolución *legítimamente* viola justo lo que esa función existe para rechazar. Ninguna de las constantes combinatorias de `coord.h` (3⁷, 2¹¹, C(12,4), 8!) es el número correcto para un puzzle cuyos recuentos de piezas son distintos en tipo, no solo en cantidad.

### Megaminx — rompe el tipo de almacenamiento del conjunto de movimientos, no solo su tamaño

Un Megaminx es un dodecaedro: 12 caras, 20 esquinas (cada una toca 3 caras, como una esquina de 3×3×3), 30 aristas (cada una toca 2 caras). La *forma* del modelo de esquina/arista de hecho se generaliza — una esquina de Megaminx es combinatoriamente el mismo tipo de objeto que un `t_corner`. Lo que se rompe es el tamaño y el almacenamiento: `12 caras × 3 giros = 36` movimientos, y `t_move_mask` — el tipo que usa cada tabla de poda, tabla de movimiento y el propio driver de IDA* en este código para pasar "qué movimientos están permitidos" — es un `uint32_t` (`movetable.h`). Treinta y seis bits de movimiento no caben en 32. `MOVES_ALL`, `MOVE_BIT()`, `move_in_mask()`, y cada firma de función que lleva un `t_move_mask` tendrían que pasar a un tipo de máscara de 64 bits (o de dos palabras) — un cambio mecánico pero real que toca `movetable.h`, `prune.h`, `ida.h` y cada función que los llama.

Más allá de eso: 20!, 30!, y los recuentos de orientación de una pieza con simetría de 3 posiciones (esquina) o de 2 (arista) sobre un dodecaedro son todas constantes combinatorias nuevas que exigen su propia derivación desde cero de qué pares de coordenadas forman una tabla de poda manejable (la división del 3×3×3 en twist/flip/slice y cperm/eperm/sperm es *específica de la cadena de grupos que Kociemba encontró para el grupo del cubo* — la estructura de fases de Thistlethwaite/Kociemba no se "porta" sin más; un análogo dodecaédrico es su propia pregunta de investigación, sin resolver en este proyecto). Y cada tabla de posiciones de `render/geometry.c` (`CORNER_POS`, `EDGE_POS`, `CENTER_POS`, todas dimensionadas y pensadas para una rejilla cúbica) necesitaría un equivalente en rejilla dodecaédrica hecho desde cero.

### Square-1 — rompe la propia representación del estado

Los dos puzzles anteriores mantienen el supuesto central de este código: un número fijo de piezas, cada una ocupando siempre exactamente una de un número fijo de posiciones, así que un "estado" es un par permutación-más-orientación. El Square-1 no tiene esa propiedad en absoluto. Sus capas de arriba y de abajo están hechas cada una de 8 piezas en forma de cuña que **no son del mismo tamaño** — una cuña de "esquina" ocupa 60° y una de "arista" ocupa 30°, así que el número de piezas que caben entre dos giros es variable, no fijo en 8 o 12. El movimiento característico (el "slice", que voltea e intercambia las mitades de arriba y de abajo) solo es legal cuando las capas de arriba y de abajo están, por casualidad, divididas en dos grupos de exactamente seis anchos-de-cuña cada uno — una *precondición geométrica de alineación* sobre el propio movimiento, sin análogo en ninguna parte de este código, donde cada uno de los 18 movimientos es legal desde cualquier estado sin condición.

Modelar esto con honestidad significa reemplazar los arrays de tamaño fijo de `t_cube` por algo como una lista ordenada de tokens de cuña de tamaño variable por capa — una estructura de datos distinta, no una versión más grande de `t_corner`/`t_edge`. `apply_move()`, cada codificador de coordenadas, cada tabla de movimiento, y la rejilla de cubies del renderizador (`RENDER_CUBIE_COUNT`, posiciones fijas) están todos construidos sobre "N piezas, N posiciones fijas", y el Square-1 no tiene ni N piezas fijas ni posiciones fijas. Esto es un proyecto nuevo que comparte nombre, no una extensión.

## Resumen

| Puzzle | Modelo de piezas | Alfabeto de movimientos | Teoría de grupos / coordenadas | Renderizador | Veredicto |
|---|---|---|---|---|---|
| 2×2×2 | Subconjunto (solo esquinas) — reutiliza `t_cube` tal cual | Subconjunto (los mismos 18) | Reutiliza `twist`/`cperm` tal cual | Reutiliza la misma escena de 26 posiciones, solo dibuja/recoge menos de ellas | **Entregado, CLI y 3D** |
| 4×4×4 | Tipos de pieza nuevos (pares wing, centros anónimos) | Movimientos nuevos (capas interiores) | Casos de paridad nuevos que `validate_cube()` NO debe rechazar | Tabla de posiciones + geometría de wings/centros nueva | Reescritura de `cube/`, `coord/`, parte de `parse/` |
| Megaminx | Mismo *tipo* de piezas, recuentos nuevos (20/30) | `t_move_mask` (`uint32_t`) demasiado pequeño para 36 movimientos | Constantes combinatorias y estructura de fases todas nuevas | Rejilla dodecaédrica desde cero | Reescritura del tipo de máscara de `movetable.h` + todo `coord/` |
| Square-1 | Sin número fijo de piezas/posiciones en absoluto | Movimientos con precondiciones de alineación | Ningún modelo de coordenadas de permutación aplica | Cubies de geometría variable | Proyecto nuevo: `t_cube` en sí no encaja |

El hilo común: la velocidad y la corrección de este código vienen ambas de elegir una única forma de estado fija, pequeña y bien entendida (`corner_perm[8]`, `corner_orient[8]`, `edge_perm[12]`, `edge_orient[12]`) y construir cada capa posterior — coordenadas, tablas de movimiento, tablas de poda, IDA*, la rejilla de cubies del renderizador — como código *genérico en los valores de coordenada* pero *no genérico en qué es una pieza*. El 2×2×2 encaja porque necesita menos piezas del mismo tipo. Los otros tres necesitan, cada uno, una noción distinta de "pieza", y ese supuesto es estructural en todo lo que hay por encima de `cube.h`, no un detalle confinado a un archivo.
