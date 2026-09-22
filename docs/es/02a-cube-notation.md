# Notación del Cubo — Nombres de Cubies, Índices de Facelets, Giros y Volteos

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos. Este archivo es el complemento concreto del §3.0 de `02-algorithms.md`: esa sección presenta los modelos de *cubie* y *facelet* en abstracto; este le pone nombre real a cada pieza y a cada pegatina, para que podáis leer código de solvers y papers — incluido el propio Kociemba — sin traducir mentalmente sobre la marcha.)*

## 1. Fijad primero vuestro marco de referencia

`U`, `R`, `F`, `D`, `L`, `B` son posiciones en el espacio, no colores. Antes de que cualquiera de los nombres de abajo signifique algo, tú y tu compañero tenéis que acordar, una vez, qué cara física es `U` y cuál es `F` — todo lo demás sale automáticamente:

- Elegid una cara para que sea `U` (arriba), y una cara adyacente para que sea `F` (frente).
- Sujetad el cubo así. `R` queda a vuestra derecha, `L` a vuestra izquierda, `D` es la base, `B` es la parte de atrás.

Los colores reales no le importan al algoritmo — lo único que importa es que los dos uséis *el mismo* marco en todas partes (parseo de entrada, renderizador, fixtures de test). Si queréis que vuestra salida se pueda comparar a ojo con solvers de referencia y herramientas de speedcubing, usad la convención de la WCA: **Blanco = U, Verde = F** (lo que fija Amarillo = D, Azul = B, Rojo = R, Naranja = L). No es obligatorio usarla, pero os ahorra tener que re-deducir "hacia dónde es arriba" cada vez que comparéis vuestra salida con algo online.

## 2. Los 26 cubies que se mueven

Los centros están fijos entre sí y el solver normalmente los ignora (ver *Cubie* en el glosario) — pero igualmente necesitan nombre, porque el renderizador y el modelo de facelets sí los usan.

**6 centros** — con el nombre de su propia cara:

```
U   R   F   D   L   B
```

**8 esquinas** — con el nombre de las tres caras que tocan, capa de arriba primero, luego capa de abajo:

```
URF   UFL   ULB   UBR
DFR   DLF   DBL   DRB
```

**12 aristas** — con el nombre de las dos caras que tocan: capa de arriba, capa de abajo, y luego capa del medio:

```
UR   UF   UL   UB
DR   DF   DL   DB
FR   FL   BL   BR
```

Estos son exactamente los nombres que usa la propia implementación de dos fases de Kociemba y la mayoría de la literatura sobre solvers — merece la pena leerlos con fluidez en vez de re-deducirlos, porque `07-defence-prep.md` da por hecho que podéis señalar un cubie y nombrarlo a simple vista.

## 3. Índices de facelets — la vista de 54 pegatinas

El *modelo de facelets* (§3.0 de `02-algorithms.md`) numera las 9 pegatinas de cada cara de `0` a `8`, de izquierda a derecha y luego de arriba a abajo, con la cara colocada sobre la red desplegada estándar:

```
          U
    L     F     R     B
          D
```

(El borde inferior de `U` toca el borde superior de `F` al plegar; el borde superior de `D` toca el borde inferior de `F`; y así sucesivamente alrededor de la red.)

Como una esquina toca 3 caras y una arista toca 2, cada índice de facelet en cada cara pertenece a un cubie concreto. Esta es la tabla completa de correspondencias — es lo único de este archivo que merece la pena tener abierto en una segunda pestaña mientras escribís `facelet.c`:

| idx | U | R | F | D | L | B |
|---|---|---|---|---|---|---|
| 0 | ULB | URF | UFL | DLF | ULB | UBR |
| 1 | UB | UR | UF | DF | UL | UB |
| 2 | UBR | UBR | URF | DFR | UFL | ULB |
| 3 | UL | FR | FL | DL | BL | BR |
| 4 | **U** | **R** | **F** | **D** | **L** | **B** |
| 5 | UR | BR | FR | DR | FL | BL |
| 6 | UFL | DFR | DLF | DBL | DBL | DRB |
| 7 | UF | DR | DF | DB | DL | DB |
| 8 | URF | DRB | DFR | DRB | DLF | DBL |

La fila 4 (el centro) va en negrita porque es la única fila donde el nombre del cubie y la letra de la cara coinciden — una forma fácil de comprobar a ojo si os habéis equivocado en algún otro punto de la tabla.

### La cadena de estado de 54 caracteres

Concatenando las columnas en orden — `U0..U8, R0..R8, F0..F8, D0..D8, L0..L8, B0..B8` — se obtiene una única cadena de 54 caracteres, uno por facelet, cada uno con el *color que muestra ahí mismo en ese momento*. Este es el orden canónico de Kociemba, y es el formato que esperan la mayoría de solvers de referencia, vectores de test y herramientas online de "introduce tu scramble" — merece la pena igualarlo exactamente si en algún momento queréis comprobar la salida de vuestro solver contra una de ellas.

## 4. Orientación de esquinas — giro `0` / `1` / `2`

Cada esquina tiene 3 facelets. Una de ellas es su pegatina "tipo U/D" — la que debería estar en una cara `U` o `D` cuando el cubo está resuelto. El **giro** (twist) cuenta cuántos pasos en sentido horario le faltan a esa pegatina para mirar realmente hacia `U`/`D`:

- `0` — la pegatina tipo U/D mira a `U` o `D`. Orientación correcta.
- `1` — girada un paso en sentido horario.
- `2` — girada dos pasos en sentido horario (equivalente a un paso en sentido antihorario).

*Invariante* (ya está en el glosario, merece la pena repetirlo aquí porque es la aserción natural para un test unitario): sumando los 8 giros de todas las esquinas, el resultado mod 3 siempre es `0` — con el cubo resuelto, y después de cualquier secuencia de movimientos legales. Si esa suma alguna vez no es cero, o vuestra tabla de movimientos o vuestro scramble de test están mal, y merece la pena comprobarlo antes de mirar cualquier otra cosa.

## 5. Orientación de aristas — volteo `0` / `1`

Cada arista tiene 2 facelets, uno de los cuales es su pegatina de referencia. La definición es más incómoda de plantear desde primeros principios que la de las esquinas, pero hay una regla sencilla de implementar y que coincide con la convención estándar: **los giros de cuarto de vuelta de `F` o `B` voltean la orientación de las 4 aristas que mueven; `U`, `D`, `L`, `R` nunca lo hacen.** Llevad la cuenta del volteo de forma incremental según aplicáis movimientos, usando esa regla, y no necesitáis ninguna definición geométrica desde cero.

(Si habéis hecho speedcubing, esto es el mismo concepto de "EO" — orientación de aristas — que se usa antes de los métodos de construcción por bloques; no es casualidad, porque es el mismo hecho de teoría de grupos del que depende el estado objetivo de la fase 1 de Kociemba.)

*Invariante*: sumando los 12 volteos de todas las aristas, el resultado mod 2 siempre es `0`. Mismo uso que el invariante de las esquinas de arriba — comprobadlo después de cada movimiento aplicado en vuestros tests.

## 6. Dónde encaja esto en el código

Enlaza directamente con la organización de módulos de `04-architecture.md`: `cube/cubie.c` es donde vive la permutación de esquinas/aristas más estos valores de orientación como modelo autoritativo; `cube/facelet.c` es la capa de conversión entre eso y la cadena de 54 caracteres de arriba, necesaria tanto para el parseo de entrada como para el renderizador. Si algún test alguna vez discrepa con una implementación de referencia, lo primero que hay que comprobar es si ambos lados usan este mismo orden de facelets y el mismo marco de referencia `U`/`F` — un desajuste silencioso ahí produce un cubo que *parece* sutilmente mal en todos los tests sin que ninguna función concreta esté obviamente rota.
