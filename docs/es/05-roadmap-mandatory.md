# Roadmap — Parte Obligatoria

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

A lo largo de este roadmap, los dos tocáis el interior del solver. La costura es la **capa de coordenadas** (ver `04-architecture.md`), porque es una interfaz limpia que se puede acordar de antemano y probar de forma independiente por ambos lados.

## Sprint 0 — Fundamentos (los dos, programación en pareja, ~2 días)

Hacedlo juntos, en la misma sala/llamada, escribiendo código codo con codo. Esto fija el vocabulario compartido para todo lo que viene después, y un desacuerdo aquí cuesta una semana entera más adelante si no se detecta ahora.

- Acordar la indexación de cubies: orden de esquinas URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB; orden de aristas UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR — coincide exactamente con la notación de Kociemba (ver `02a-cube-notation.md`), que es lo que hace que vuestros vectores de test se puedan comparar con solvers de referencia. **Escribidlo en `DECISIONS.md` y no lo cambiéis nunca** — todo el código que cualquiera de los dos escriba a partir de aquí asume exactamente este orden.
- Acordar el orden del string de layout de facelets (U-R-F-D-L-B, 9 pegatinas cada una).
- Construir la struct `t_cube`, las tablas de permutación de los 18 movimientos, y `apply_move()`.
- Escribir un arnés de pruebas: aplicar un movimiento 4 veces → debería volver al estado resuelto (cuatro giros de 90° = 360° = sin cambio). Aplicar la secuencia `R U R' U'` seis veces → debería volver al estado resuelto (es una identidad conocida de la teoría del cubo). Aplicar un scramble, y luego su inverso exacto → debería volver al estado resuelto.
- Esqueleto del Makefile, más un target `make valgrind` desde el primer día (no añadido después como parche).

## Sprint 1 — Dividir en la costura de coordenadas (~4 días)

| | Dev A | Dev B |
|---|---|---|
| **Se encarga de** | `parse/`, `cube/facelet.c`, `main.c` | `coord/encode.c`, `coord/movetable.c` |
| **Construye** | Parser de notación (incluyendo rechazar M/E/S/x/y/z), validación de legalidad del cubo, conversión facelet↔cubie, gestión completa de errores, la CLI | Los seis codificadores/decodificadores de coordenadas, el constructor genérico de tablas de movimiento, pruebas unitarias que demuestren `movetable[encode(c)][m] == encode(apply(c,m))` |
| **Puerta** (hay que pasarla antes de seguir) | `./rubik ""`, `./rubik "R2 X"`, `./rubik` sin argumentos, y otras 10 entradas basura fallan todas de forma limpia con un mensaje útil y sin fuga de memoria | Cada coordenada hace ida y vuelta correctamente, y cada entrada de tabla de movimiento coincide con el resultado a nivel de cubie, en 100.000 estados aleatorios |

## Sprint 2 — Dividir por fase (~5 días)

| | Dev A | Dev B |
|---|---|---|
| **Se encarga de** | `coord/prune.c` (constructor genérico por BFS) + **Fase 1** | `solve/ida.c` (motor genérico) + **Fase 2** |
| **Construye** | Tablas `twist×slice` y `flip×slice`; el test de objetivo de la fase 1; el conjunto de movimientos de la fase 1 (los 18 movimientos) | IDA* con la heurística de poda; tablas `cperm×sperm` y `eperm×sperm`; el test de objetivo de la fase 2; el conjunto de movimientos de la fase 2 (10 movimientos) |
| **Puerta** | Desde 1.000 estados aleatorios, la fase 1 alcanza G1 en ≤12 movimientos, todas las veces | Desde 1.000 estados G1 aleatorios, la fase 2 resuelve en ≤18 movimientos, todas las veces |

*Fijaos en el cruce deliberado:* el Dev A escribe el constructor de tablas de poda, pero es el Dev B quien realmente lo consume dentro del motor de búsqueda — y viceversa para la lógica de fases. Esto es intencionado: ninguno de los dos debería poder entregar un módulo que el otro no entienda lo suficiente como para explicarlo.

## Sprint 3 — Integración y endurecimiento (los dos, ~3 días)

- Conectar la fase 1 con la fase 2, y luego añadir el **bucle de iteración sub-óptima** descrito en `02-algorithms.md` (seguir enumerando soluciones de fase 1, quedarse con el total combinado más corto, parar al llegar a un presupuesto de tiempo).
- Añadir poda por redundancia a la generación de movimientos (sin repeticiones de la misma cara, una regla de orden fija para pares de caras opuestas).
- **Construir un arnés de benchmark:** correr 1.000 scrambles aleatorios y reportar el número medio de movimientos, el máximo, el tiempo medio de solución, y el **tiempo máximo**. Este es el artefacto que realmente demuestra R4 en la defensa — subid su salida al repo.
- Correr Valgrind limpio en cada camino de código, incluyendo cada camino de error (no solo el camino feliz).
- Terminar el Makefile: `all clean fclean re`.

## Sprint 4 — Congelación de la parte obligatoria

Nadie toca el bonus hasta que cada ítem de esta checklist esté en verde:

- [ ] `./rubik "F R U2 B' L' D'" | cat -e` saca exactamente una línea, terminada en `$`, nada más en la línea
- [ ] La media de `wc -w` sobre 1.000 scrambles es ≤ 50 (se espera aproximadamente 21)
- [ ] El tiempo máximo (wall-clock) sobre 1.000 scrambles está por debajo de 3 s (se espera menos de 100 ms)
- [ ] Cada camino de error produce un mensaje limpio, un código de salida distinto de cero, y está limpio en Valgrind
- [ ] La firma de función del solver recibe un *estado* del cubo, nunca un string (la regla anti-trampa, `01-requirements.md`)
- [ ] Los dos podéis explicar **cada módulo** — incluyendo el del otro — en una pizarra, en frío
