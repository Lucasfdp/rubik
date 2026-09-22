# Algoritmo del Solver — Opciones y Decisión

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

**Recomendación: Kociemba de dos fases, sobre un framework genérico de coordenadas + IDA*.**

## 3.0 La base compartida (construir esto primero, sea cual sea el algoritmo elegido)

Antes de comparar algoritmos, ayuda saber que las opciones B–E de abajo comparten cerca del 70% de su código. Ese código compartido es:

1. **Modelo de cubies** — el cubo representado como 8 piezas de esquina y 12 piezas de arista, cada una rastreada por *dónde está* (su permutación — ver glosario) y *cómo está girada/volteada* (su orientación). Las esquinas tienen 3 orientaciones posibles (0, 1 o 2 — pensad "sin girar, girada en sentido horario, girada en sentido antihorario"); las aristas tienen 2 (volteada o no).
2. **Modelo de facelets** — el cubo como 54 pegatinas individuales. Esto es lo que se usa para leer la entrada, escribir la salida, y alimentar el renderizador 3D — es la vista de "qué aspecto tiene", frente a la vista de "qué pieza está dónde" del modelo de cubies. (Para los nombres reales de las piezas, la numeración de índices de facelets, y cómo se codifican el giro y el volteo, ver `02a-cube-notation.md`.)
3. **Aplicación de movimientos** — los 18 movimientos posibles (6 caras × {90° horario, 90° antihorario, 180°}), cada uno implementado como una tabla de permutación: una tabla precalculada de "la pieza en la posición X pasa a la posición Y" para ese movimiento.
4. **Codificadores de coordenadas** — funciones que comprimen un cubo completo del modelo de cubies en un único entero pequeño (una "coordenada" — ver abajo). Este es el truco que hace que todo el espacio se pueda buscar.
5. **Tablas de movimiento** — tablas precalculadas con la forma `nueva_coordenada = tabla[coordenada_vieja][movimiento]`. Se construyen una vez, al arrancar, aplicando cada movimiento a cada valor de coordenada a nivel de cubie y guardando el resultado.
6. **Generador de tablas de poda** — una búsqueda en anchura genérica (BFS — ver glosario) sobre un espacio de coordenadas, empezando desde el estado objetivo y yendo hacia atrás, que registra "cuántos movimientos mínimo para llegar al objetivo desde este valor de coordenada."
7. **Motor de IDA*** — el algoritmo de búsqueda real (profundización iterativa con una heurística de tabla de poda — ver glosario) que usa las tablas de arriba para encontrar una solución.

Así que la decisión real no es "qué código base", sino "qué coordenadas, y cuántas fases." **Construir 1–7 de forma genérica hace que la elección del algoritmo sea barata y reversible**, que es por qué la opción E (correr dos algoritmos y quedarse con la mejor respuesta) sale casi gratis una vez que existe esta base. Este es el punto estructural más importante de todo este archivo.

---

## 3.A Método capa por capa / de principiante

Resolver primero la primera capa, luego las aristas de la capa del medio, luego la última capa usando un conjunto fijo de secuencias de movimientos memorizadas (así es como la mayoría de humanos aprenden a resolver un cubo).

| A favor | En contra |
|---|---|
| Trivial de entender y explicar | **Falla R4 directamente** — media de 100–210 movimientos, el subject exige ≤50 |
| Sin tablas, sin búsqueda, sin sobrecarga de memoria | Mucho código tedioso de gestión de casos; irónicamente no es *menos* trabajo de implementación que los algoritmos "difíciles" |
| Resuelve al instante (<1 ms) | No hay nada que defender algorítmicamente — no demuestra las "nociones moderadas de teoría de grupos" que pide el subject |

**Veredicto: no entregar esto.** Un uso legítimo: implementarlo como un oráculo desechable para verificar el motor de movimientos ("¿mi modelo de cubo se comporta realmente como un cubo real?"). Incluso eso es opcional — una prueba más simple de ida y vuelta (aplicar un scramble, luego su inverso, comprobar que se vuelve al estado resuelto) cubre el mismo terreno más barato.

---

## 3.B Algoritmo de cuatro fases de Thistlethwaite

La idea: descender por una cadena de **subgrupos** anidados (ver glosario — informalmente, un subgrupo aquí significa "un conjunto más pequeño de movimientos permitidos que aun así permite alcanzar un conjunto restringido de estados del cubo"), restringiendo qué movimientos se permiten en cada paso:

```
G0 = <U, D, L, R, F, B>        los 18 movimientos       — cualquier cubo revuelto
G1 = <U2, D2, L, R, F, B>      orientaciones de arista fijadas
G2 = <U2, D2, L, R, F2, B2>    orientaciones de esquina + aristas de la capa UD fijadas
G3 = <U2, D2, L2, R2, F2, B2>  solo giros de 180°
G4 = {identidad}                resuelto
```

(Leyendo esa notación: `U2` significa "en esta fase solo se permiten giros de 180° de la cara U," `U` sola significa que cualquier giro de U sigue permitido. A medida que se avanza por la cadena, los movimientos se restringen *cada vez más*, porque cada fase fija una propiedad que los movimientos posteriores, más restringidos, ya no pueden deshacer.)

La versión original de Thistlethwaite acotaba las cuatro fases en 7, 13, 15 y 17 movimientos en el peor caso (52 movimientos en total). Una búsqueda posterior más exhaustiva de los espacios intermedios (llamados espacios de coset — ver glosario) demostró que el peor caso real es en realidad 7, 10, 13 y 15 movimientos por fase — 45 en total.

Las tablas necesarias tienen estas cantidades de entradas (es el número de posiciones distintas dentro de cada espacio de coset intermedio — no hace falta memorizarlas, solo saber que todas caben fácilmente en RAM): 1.024 · 1.082.565 · 29.400 · 663.552.

| A favor | En contra |
|---|---|
| **Más fácil de explicar visualmente** — "cada fase fija una propiedad que las fases posteriores no pueden romper" | Media de 40–45 movimientos: cumple R4, pero con mucho menos margen que Kociemba |
| La tabla más grande tiene ~1,08M entradas — cabe en RAM sin problema, se genera en bastante menos de un segundo | Cuatro fases = cuatro conjuntos de coordenadas que hay que hacer bien, frente a dos en Kociemba |
| Cada fase se puede probar de forma independiente ("¿está el cubo en G2 ahora?") | El resultado está visiblemente lejos de las soluciones óptimas de 20 movimientos que referencia el subject |
| Las tablas de distancia exacta (construidas con BFS simple) significan que no hay sutilezas de heurística que se puedan hacer mal | Al final se acaba haciendo casi todo el trabajo de implementación de Kociemba, para un peor número de movimientos |

**Veredicto:** excelente algoritmo *didáctico*, y un segundo algoritmo genuinamente barato una vez que existe la base compartida (§3.0). No es el que hay que entregar en solitario.

---

## 3.C Algoritmo de dos fases de Kociemba ← **recomendado**

La idea de Kociemba en 1992: el G1 de Thistlethwaite (ver arriba) *ya* es lo bastante restrictivo. Una vez que un cubo llega a G1, el resto del puzzle vive en un espacio lo bastante pequeño como para buscarlo directamente — sin necesidad de seguir subdividiendo en G2/G3. Así que la cadena de cuatro fases colapsa a dos:

```
Fase 1: cualquier estado  →  G1 = <U, D, R2, L2, F2, B2>    usando los 18 movimientos
Fase 2: G1                →  resuelto                        usando solo los 10 movimientos de G1
```

**El objetivo de la fase 1, en palabras sencillas:** conseguir que las 12 aristas estén orientadas correctamente, las 8 esquinas orientadas correctamente, y que las 4 aristas de "capa UD" (las que pertenecen a la capa del medio — FR, FL, BL, BR) estén en algún sitio de la capa del medio (no necesariamente en el *orden* correcto todavía, solo en la capa correcta).

**Coordenadas** (estos números merece la pena memorizarlos — los vais a querer en la pizarra durante la defensa):

| Fase | Coordenada | Tamaño | Significado |
|---|---|---|---|
| 1 | `twist` | **2.187** = 3⁷ | Orientaciones de las esquinas. Solo hace falta rastrear 7 esquinas — la 8ª queda *determinada*, porque el giro total entre todas las esquinas siempre es múltiplo de 3 (un invariante matemático del cubo). |
| 1 | `flip` | **2.048** = 2¹¹ | Orientaciones de las aristas. Misma idea: solo hace falta rastrear 11 aristas, la 12ª queda determinada porque el volteo total siempre es par. |
| 1 | `slice` | **495** = C(12,4) | *Cuáles 4 de las 12 posiciones de arista* contienen actualmente las 4 aristas de capa UD, sin importar su orden. (C(12,4) es "12 sobre 4" — el número de formas de elegir un grupo sin orden de 4 elementos entre 12.) |
| 2 | `cperm` | **40.320** = 8! | Permutación (disposición) de las 8 esquinas. |
| 2 | `eperm` | **40.320** = 8! | Permutación de las 8 aristas de las capas U/D. |
| 2 | `sperm` | **24** = 4! | Permutación de las 4 aristas de la capa del medio, entre sí. |

Una **coordenada**, en términos sencillos, es una huella digital con pérdida: un entero pequeño que captura *una propiedad concreta* del estado del cubo mientras descarta todo lo demás. Millones de disposiciones de cubo distintas pueden compartir el mismo valor de `twist`, porque `twist` solo se fija en la orientación de las esquinas, no en su posición. Eso es intencionado — es lo que hace que el espacio de búsqueda sea lo bastante pequeño como para manejarlo.

El espacio de estados completo de la fase 1 es el **producto cartesiano** de `twist × flip × slice` (ver glosario para qué es un producto cartesiano — brevemente, es "cada combinación posible de un valor de cada conjunto"): 2.187 × 2.048 × 495 ≈ 2.200 millones. Eso es demasiado grande para guardarlo como una única tabla gigante, así que se factoriza en subtablas por pares (ver la fila de tablas de poda de abajo) — se consultan dos tablas más pequeñas y se combinan los resultados, en vez de una tabla con 2.200 millones de entradas.

**Tablas de poda realmente necesarias** (una "tabla de poda" es una tabla precalculada de "movimientos mínimos necesarios desde este valor de coordenada" — ver glosario; con un byte por entrada basta, no hace falta nada más sofisticado):

| Tabla | Entradas | Bytes @1B | Fase |
|---|---|---|---|
| `twist × slice` | 2.187 × 495 = 1.082.565 | ~1,0 MB | 1 |
| `flip × slice` | 2.048 × 495 = 1.013.760 | ~1,0 MB | 1 |
| `twist × flip` *(opcional, más fuerte)* | 2.187 × 2.048 = 4.478.976 | ~4,3 MB | 1 |
| `cperm × sperm` | 40.320 × 24 = 967.680 | ~0,9 MB | 2 |
| `eperm × sperm` | 40.320 × 24 = 967.680 | ~0,9 MB | 2 |

El total es menos de 10 MB, y cada tabla se genera con BFS simple en bastante menos de un segundo. **Esto importa mucho para R4**: significa que se pueden regenerar todas las tablas desde cero en cada arranque del programa y *aun así* quedar muy por debajo del límite de 3 segundos — lo que significa que nunca hay que defender la pregunta incómoda de "pero las tablas están en caché de una ejecución anterior, ¿eso cuenta como trampa?".

**Calidad del resultado.** La implementación de referencia del propio Kociemba, sin ninguna optimización extra, produce soluciones en el rango de 20–23 movimientos en milisegundos bajos en hardware moderno. Toda posición, en el contexto que referencia el propio subject, es resoluble de forma óptima en como máximo 20 movimientos — así que 20–23 está muy cerca del óptimo, mientras corre órdenes de magnitud más rápido que un solver que busca el óptimo real (ver la opción D más abajo).

**El bucle de iteración sub-óptima** — la parte que la gente se salta, y no debería: no os quedéis con la *primera* solución de la fase 1. Seguid enumerando soluciones de fase 1 de longitud creciente; para cada una, ejecutad la fase 2 sobre el resultado; quedaos con el total combinado (longitud fase 1 + longitud fase 2) más corto encontrado hasta el momento. Parad al llegar a un presupuesto de tiempo o a una longitud objetivo. Esta única técnica es la que convierte un resultado mediocre de ~25 movimientos en el resultado de ~20–21 movimientos citado arriba.

| A favor | En contra |
|---|---|
| **~20–23 movimientos** — margen enorme bajo el límite de 50 de R4 | Más maquinaria de coordenadas que Thistlethwaite (aunque solo 2 fases, no 4) |
| Resuelve en **milisegundos**, las tablas se construyen en **bastante menos de un segundo** | Las heurísticas de poda necesitan cuidado — una tabla incorrecta rompe la corrección en silencio (ver "trampas conocidas" abajo) |
| La respuesta canónica y bien documentada para este problema exacto | La coordenada `eperm` **no se puede rastrear a través de los movimientos de la fase 1** — ver trampas abajo |
| Se explica de forma limpia con una sola imagen: "primero hacer que el cubo se comporte bien, luego terminar solo con movimientos seguros" | El bucle de iteración sub-óptima añade una capa de control de búsqueda más allá de una búsqueda simple de un solo disparo |
| La reducción por simetría (una optimización que aprovecha las simetrías físicas del cubo para encoger aún más las tablas) es **opcional** — se puede omitir por completo y aun así quedar 1000 veces por debajo del presupuesto de tiempo | |

**Veredicto: entregar esto.**

### Trampas conocidas (escribidlas en la pizarra antes de empezar a programar)

1. `eperm` (permutación de las aristas de las capas U/D) solo tiene sentido *dentro* de G1. No intentéis construir una tabla de movimientos para ella a través de los movimientos de la fase 1 — en su lugar, calculadla directamente desde el cubo a nivel de cubie en el límite entre la fase 1 y la fase 2. (Un enfoque más avanzado la rastrea de forma incremental con coordenadas `u_edges`/`d_edges` separadas; lo más simple es recalcularla directamente.)
2. Los valores de poda siempre deben ser una **cota inferior** de la distancia real — es decir, la tabla nunca debe afirmar "necesitas al menos 8 movimientos más" cuando la respuesta real es 6. Si una tabla alguna vez *sobreestima*, IDA* (ver glosario — esta propiedad se llama ser "admisible") devolverá respuestas incorrectas, o ninguna respuesta, de una forma muy difícil de depurar porque solo aparece con entradas concretas.
3. Orden de movimientos / poda por redundancia: no dejéis que la búsqueda pruebe `R` justo después de `R` (eso equivale a un solo `R2` o a nada, y siempre es un desperdicio), y fijad una regla de orden para los pares de caras opuestas (por ejemplo, no probar `L` inmediatamente después de `R` si ya se ha probado `R` después de `L` en la misma rama). Saltarse esto aproximadamente duplica el tiempo de búsqueda sin ningún beneficio.
4. La **paridad** (ver glosario) de orientación de esquinas y aristas debe validarse en la entrada, antes de empezar la búsqueda. Un cubo "físicamente imposible" — uno que nunca podría resultar de revolver de verdad un cubo real — hace que la búsqueda persiga una solución que no existe, lo cual se ve exactamente igual que un bucle infinito. Eso es un fallo instantáneo de R6, y es completamente evitable con una comprobación de validez previa.

---

## 3.D IDA* con bases de datos de patrones de Korf (el solver óptimo)

El enfoque de Korf precalcula tres **bases de datos de patrones** (tablas de poda grandes, una por grupo de piezas): una para las 8 esquinas, una para 6 de las 12 aristas, y otra para las 6 aristas restantes. Durante la búsqueda, toma el *máximo* de las tres consultas (no la suma — porque cada giro mueve cuatro aristas y cuatro esquinas a la vez, así que las tres estimaciones no son independientes y sumarlas sobreestimaría, rompiendo la admisibilidad). La longitud mediana de solución óptima que produce esto es 18 movimientos.

| A favor | En contra |
|---|---|
| **Demostrablemente óptimo** — la solución más corta posible de verdad para cada cubo | **Viola el presupuesto de 3 segundos de R4 por órdenes de magnitud.** Scrambles difíciles a profundidad 18–20 pueden tardar minutos u horas |
| Satisface directamente el ítem de bonus "un algoritmo que baje de las soluciones más optimizadas" | Las bases de datos de patrones pesan 85 MB+ y tardan minutos en generarse; cachearlas en disco se vuelve casi obligatorio, lo que a su vez invita su propia pregunta en la defensa |
| Reutiliza el mismo motor IDA* que ya se necesita para Kociemba | Riesgo de R6: una búsqueda que legítimamente tarda minutos es indistinguible de un bucle infinito para un evaluador impaciente |

**Veredicto: no es el solver que se entrega.** Merece la pena añadirlo *más adelante*, de forma opcional, detrás de un flag como `./rubik -optimal "<mix>"`, con un límite de tiempo real (wall-clock, tiempo transcurrido de verdad, no tiempo de CPU) que caiga de vuelta a la respuesta de Kociemba si no termina a tiempo. El propio subject dice que los bonus que tardan más de unos segundos no se consideran razonables — así que hay que protegerlo detrás de un flag explícito y nunca dejar que sea el camino por defecto.

---

## 3.E Selección multi-algoritmo (ítem de bonus, casi gratis)

La lista de bonus del subject nombra explícitamente "una elección entre varios algoritmos, o una selección de la mejor solución entre varios algoritmos." Si la base compartida del §3.0 se construyó de forma genérica, correr tanto Thistlethwaite como Kociemba e imprimir el que dé un resultado más corto son quizá 40 líneas de código de conexión.

| A favor | En contra |
|---|---|
| Ítem de bonus nombrado explícitamente, coste marginal muy bajo | Solo sale gratis si el §3.0 se construyó de verdad de forma genérica desde el primer día — adaptarlo después sale caro |
| Refuerza notablemente la defensa — se pueden comparar números de movimientos en vivo, sobre la marcha | Dos algoritmos corriendo = dos cosas que pueden fallar de forma independiente |

---

## 3.F Matriz de decisión de algoritmos

| | Media mov. | Tiempo solución | RAM tablas | Tiempo gen. tablas | Esfuerzo impl. | Cumple R4 |
|---|---|---|---|---|---|---|
| **A** Capa por capa | 100–210 | <1 ms | 0 | — | Medio | ❌ |
| **B** Thistlethwaite | ~40–45 | <10 ms | ~2 MB | <1 s | Medio-alto | ✅ (margen justo) |
| **C** Kociemba | **~20–23** | **~1–50 ms** | **<10 MB** | **<1 s** | Alto | ✅✅ |
| **D** Korf (óptimo) | 18 (óptimo) | segundos–horas | ~85 MB | minutos | Alto | ❌ |
| **E** B + C, mejor de ambos | ~20–23 | <60 ms | ~12 MB | <1 s | C + añadido pequeño | ✅✅ |
