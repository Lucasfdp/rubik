# Bonus 3D — Decisión de Stack y Renderizado

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

**Recomendación: raylib**, con MiniLibX + un rasterizador hecho a mano como plan de respaldo si raylib no compila en las máquinas del campus.

## Qué tiene que significar "un gráfico de verdad"

El subject es explícito y esto condiciona todo lo demás: *"una serie de figuras o letras se considera un debug, no un bonus de verdad,"* y *"también podéis intentar mostrar gráficamente el giro del cubo en tiempo real (¡esto estaría realmente bien!)."*

Así que el listón es: **un cubo 3D real con giros de cara animados**, no una malla 2D de colores, no una cuadrícula de ncurses. Cada opción de abajo se juzga contra ese listón.

## 4.0 El modelo de renderizado (idéntico sea cual sea el stack elegido)

Este diseño es independiente del stack a propósito, para que la decisión de stack se pueda tomar tarde (o incluso cambiar) sin tener que reescribir la lógica.

1. **Geometría.** 26 cubies visibles (o 27 si se incluye una pieza central oculta por simplicidad) situados en posiciones enteras de una **rejilla** (lattice) — "rejilla" aquí simplemente significa una malla 3D fija, en este caso la disposición 3×3×3 de coordenadas `(x, y, z)` donde cada una de x, y, z es una de `{-1, 0, 1}`. Cada cubie es un cubo pequeño con 6 caras, cada cara siendo una figura plana de 4 lados llamada **quad**. Total: ~162 quads — trivial para cualquier renderizador, hecho a mano o basado en GPU.
2. **Color.** Los colores de las pegatinas se leen **del estado lógico del cubo** (el mismo modelo de cubie/facelet que usa el solver), nunca se guardan por separado dentro del renderizador. Esto es intencionado: si el *solver* tiene un bug, se vuelve visualmente obvio en pantalla en vez de quedar silenciosamente enmascarado por un renderizador que lleva su propia copia separada de los colores.
3. **Autoridad del estado.** El cubo lógico (la estructura de datos del solver) es la única fuente de verdad. El renderizador solo guarda "estado lógico + una animación en curso."
4. **Animar un movimiento, p. ej. `R` (girar la cara derecha):** seleccionar los 9 cubies cuya coordenada `x` es `+1`; rotar ese grupo de 9 como una unidad alrededor del eje X, de 0° a −90°, repartido suavemente en varios fotogramas usando **easing** (una curva que hace que el movimiento acelere/desacelere de forma natural en vez de moverse a velocidad constante como un robot). Al terminar la rotación, **aplicar la permutación real a las posiciones lógicas de la rejilla y reiniciar la rotación de la animación a cero.** Nunca dejéis que las posiciones rotadas intermedias (interpoladas) se acumulen en el estado lógico — si lo hacéis, pequeños errores de redondeo de coma flotante se van acumulando a lo largo de cientos de movimientos y el cubo visible acaba desincronizándose del real.
5. **Cola de movimientos.** El solver produce una lista de movimientos; el renderizador los reproduce uno a uno a una velocidad configurable (milisegundos por movimiento), con controles de pausa / paso / velocidad.

Este es el enfoque estándar que usa prácticamente todo visualizador de cubos maduro: representar el cubo como cubies individuales con colores de pegatina por cara, animar los giros en la escena 3D, y luego aplicar el resultado al estado lógico una vez termina la animación. Elegir qué cubies girar es simplemente una comprobación de coordenadas (p. ej., "¿la x de este cubie es +1?"), y luego rotar solo esos.

---

## 4.A raylib ← **recomendado**

**raylib** es una librería gráfica multiplataforma, escrita en C99 (el mismo estándar de C que ya estáis usando), construida sobre OpenGL (la API gráfica estándar que soportan la mayoría de tarjetas gráficas). No tiene dependencias externas que instalar por separado — todo lo que necesita viene incluido — y está acelerada por hardware (es decir, usa la tarjeta gráfica, no solo la CPU, así que es rápida).

Da, ya listo para usar: un objeto `Camera3D` y controles de cámara orbital (para que el usuario pueda girar la vista con el ratón), bloques `BeginMode3D`/`EndMode3D` para dibujar en 3D, funciones `DrawCube`/`DrawCubeWires`, y una pequeña librería matemática (`raymath`) para las matemáticas de vectores/matrices/rotaciones que de otro modo habría que escribir a mano. Eso es exactamente y solo lo que necesita este bonus.

| A favor | En contra |
|---|---|
| **Camino más rápido a un cubo animado que gira** — realistamente un día para un prototipo funcional | No viene preinstalado en las máquinas de 42; hay que vendorizarlo (incluir una copia en el repo) y compilarlo vosotros mismos — ver la historia de compilación abajo |
| Gratis, código abierto, licencia permisiva (licencia zlib/libpng) — imposible ningún argumento de licencias en la defensa | Un evaluador puede preguntar "¿eso no es una librería que hace el trabajo por vosotros?" — la respuesta: dibuja quads y gestiona una ventana/cámara; la lógica del cubo, la rejilla, la animación, y cada permutación son vuestras |
| C99 puro, encaja con el resto del proyecto — no hace falta C++ ni un sistema de build separado | Incluye internamente otras dos librerías (GLFW para ventanas, glad para cargar funciones de OpenGL), así que el árbol de dependencias real es más grande de lo que parece |
| `raymath` os ahorra escribir matemáticas de matrices 4×4 / rotaciones que no necesitáis *demostrar* que sabéis escribir desde cero | Algo menos de crédito de "lo construimos desde nada" que las opciones B o C de abajo |
| Cámara, entrada de ratón/teclado, y texto en pantalla (para un HUD de contador de movimientos) vienen todos incluidos | |

**Historia de compilación para máquinas de 42 (sin acceso admin/sudo).** Clonar el repo de raylib, luego desde dentro de `raylib/src` ejecutar `make PLATFORM=PLATFORM_DESKTOP` para compilar la librería estática (un archivo `.a` que se enlaza directamente en vuestro binario, sin necesidad de instalación a nivel de sistema). Vendorizarlo como un submódulo de git bajo `lib/raylib` en vuestro repo, enlazar `libraylib.a` localmente, y añadir una regla de Makefile para que `make bonus` la compile automáticamente en un checkout nuevo. Necesita las cabeceras de desarrollo de Mesa (la implementación de código abierto de OpenGL) y X11 (el sistema de ventanas) para compilar — concretamente `libx11-dev`, `libgl1-mesa-dev`, y `xorg-dev` en una máquina Linux de la familia Debian. **Verificad que están presentes en las máquinas del cluster antes de comprometeros con esta opción** — es una comprobación de 10 minutos, hacedla primero, antes de escribir ningún código de renderizado.

## 4.B MiniLibX + nuestro propio rasterizador por software ← **la alternativa purista**

Proyectar los 162 quads vosotros mismos, completamente a mano: transformar cada punto a través del espacio de modelo → mundo → vista → proyección (el pipeline estándar de gráficos 3D, hecho con vuestras propias matemáticas de matrices en vez de con una librería), descartar los quads que miran hacia atrás (no dibujar las caras que apuntan lejos de la cámara), ordenar los quads por profundidad (lo más cercano a la cámara al final, llamado el **algoritmo del pintor** — pintar primero lo lejano, luego pintar encima lo cercano, igual que haría un pintor de verdad sobre un lienzo), y rellenar cada quad con un **rasterizador** por barrido (scanline) (una rutina que convierte una figura 2D en los píxeles reales que la componen) que escribe directamente en un buffer de imagen de MLX.

| A favor | En contra |
|---|---|
| **Riesgo de justificación cero** — MiniLibX (MLX) es la propia librería del colegio y no hace nada por vosotros aquí | Escribís el pipeline 3D *entero* vosotros mismos: matrices, proyección, recorte, descarte, relleno de polígonos |
| Máximo crédito en la defensa — "nosotros escribimos el renderizador" es una frase fuerte en una evaluación de 42 | Sin aceleración por GPU, sin iluminación gratis — el sombreado hay que hacerlo a mano (un sombreado plano por cara basta y queda bien para esto) |
| Probablemente ya habéis hecho un trabajo de base parecido en proyectos gráficos anteriores de 42 (fdf, cub3d); el modelo mental debería resultar familiar | MLX se comporta ligeramente distinto en macOS que en Linux, y su gestión de eventos es muy básica |
| 162 quads son pocos, así que el algoritmo del pintor — normalmente un atajo defectuoso para escenas complejas — no tiene ningún caso patológico aquí: los cubies nunca se solapan ni se interpenetran, así que ordenar por distancia del centro a la cámara es genuinamente correcto | Riesgo real de quedaros sin tiempo y no entregar bonus en absoluto si esto resulta más difícil de lo esperado |
| Sin dependencia externa, sin picos de compilación, sin submódulo que mantener | El anti-aliasing (suavizado de bordes dentados), el movimiento de cámara suave, y el pulido en general cuestan tiempo real, nada trivial |

**Esta es una opción real, no un premio de consolación.** MLX es deliberadamente minimalista — precisamente *por eso* escribir un pipeline 3D de verdad encima de ella puntúa bien en la defensa. Elegid esta opción si queréis que el trabajo gráfico en sí mismo sea parte de lo que estáis aprendiendo, y estáis dispuestos a aceptar un comienzo más lento a cambio.

## 4.C OpenGL 3.3 core puro + GLFW + glad

OpenGL es la API gráfica en sí misma (sobre la que se apoyan raylib y MLX, de una forma u otra, en última instancia). GLFW gestiona la creación de ventanas y la entrada; glad carga los punteros de función de OpenGL reales en tiempo de ejecución (un trabajo de fontanería necesario en la mayoría de plataformas). Usarlos directamente significa escribir vuestros propios vertex/fragment shaders (pequeños programas que corren en la GPU para controlar cómo se dibuja todo), y gestionar manualmente VAOs/VBOs/EBOs — buffers que guardan los datos de vuestra geometría 3D en la GPU (ver glosario para el desglose completo de estos tres).

| A favor | En contra |
|---|---|
| Máximo control de todas las opciones; un pipeline de shaders real; la habilidad de gráficos más transferible | **3–5 veces más trabajo de configuración que raylib** antes de ver siquiera el primer triángulo en pantalla |
| Genuinamente impresionante para un evaluador con mentalidad gráfica | La compilación de shaders y su gestión de errores, el buffer de profundidad, y vuestras propias matemáticas de matrices hay que construirlo todo desde cero |
| GLFW y glad son ambas dependencias pequeñas y bien documentadas | Mayor riesgo de que el bonus quede sin terminar cuando llegue el plazo de la parte obligatoria |

**Veredicto:** solo merece la pena si la parte obligatoria termina pronto y queréis específicamente la experiencia de aprender OpenGL. Dato a tener en cuenta: raylib ya *es* GLFW + glad por debajo, envuelto en una capa mucho más cómoda — así que elegir esta opción es esencialmente "raylib, menos la comodidad."

## 4.D SDL2 + OpenGL

SDL2 es otra librería de ventanas/entrada, con un papel similar al de GLFW.

| A favor | En contra |
|---|---|
| SDL2 es algo más probable que ya esté instalada que raylib | Sigue requiriendo todo el trabajo de OpenGL de la opción C — SDL2 solo sustituye la parte de ventanas, no el pipeline de renderizado |
| Mejor soporte de entrada/audio/mando que GLFW (irrelevante para este proyecto) | API más pesada que GLFW para lo que hace falta aquí |

**Veredicto:** sin ventaja real sobre raylib para este proyecto. Descartar.

## 4.E ncurses / ASCII / malla de colores

**Descartado directamente por el texto del subject**: *"una serie de figuras o letras se considera un debug, no un bonus de verdad."* Se puede seguir construyendo una malla 2D de colores (el cubo "desplegado" plano) — pero tratadlo puramente como una **herramienta de debug** durante el desarrollo, nunca lo presentéis como el bonus en sí.

## 4.F Solver en C que envía datos a un visor web (three.js)

Hacer que el solver en C produzca listas de movimientos que una página web separada en JavaScript/three.js (una librería 3D de navegador muy popular) consuma y anime.

| A favor | En contra |
|---|---|
| El resultado más bonito por hora invertida | Dos lenguajes y dos bases de código separadas; se lee como esquivar el requisito real del bonus (que implica que los gráficos deberían ser parte del propio proyecto en C) |
| Trivialmente compartible como un gif en el README | Necesita un navegador corriendo en el momento de la defensa; una demo más frágil |

**Veredicto:** no es un sustituto del bonus real. Posiblemente merezca la pena hacerlo *después*, puramente como una demo bonita para el README, una vez todo lo demás esté terminado.

## 4.G Matriz de decisión de gráficos

| | Tiempo hasta el primer cubo girando | Riesgo de justificación | Crédito en defensa | Deps. externas | Riesgo de no entregar |
|---|---|---|---|---|---|
| **A** raylib | ~1 día | Bajo | Medio | raylib (vendorizada) | **Bajo** |
| **B** MLX + rasterizador propio | ~4–6 días | **Ninguno** | **Alto** | ninguna | Medio-alto |
| **C** OpenGL + GLFW | ~3–4 días | Bajo | Alto | GLFW, glad | Alto |
| **D** SDL2 + OpenGL | ~3–4 días | Bajo | Alto | SDL2 | Alto |
| **E** ncurses | ~1 día | — | **No llega al listón** | ncurses | — |
| **F** visor three.js | ~1 día | Medio | Bajo | node/navegador | Bajo |

**Regla de decisión:** si el pico de compilación de raylib en una máquina del cluster tiene éxito en menos de una hora, coged **A**. Si os da problemas (faltan cabeceras de X11/Mesa, sin acceso de escritura en ningún sitio útil), coged **B** en su lugar — perdéis algo de pulido visual, pero ganáis una historia más fuerte de "esto lo construimos nosotros."
