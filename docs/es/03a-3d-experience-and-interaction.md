# Experiencia 3D — Modos de Reproducción, Interacción y Acabado

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos. Este documento asume que la decisión de stack de `03-graphics.md` ya está tomada — raylib, vendorizada bajo `lib/raylib` — y construye sobre ella. No la reabre; si el build spike de raylib del Sprint 5 de `06-roadmap-bonus.md` falla y se pasa a MLX como plan B, todo lo de abajo sigue aplicando, solo que implementado sobre un rasterizador hecho a mano en vez de las llamadas de raylib.)*

## 0. Qué añade este documento

`03-graphics.md` responde "con qué renderizamos, y cómo se anima un movimiento." `06-roadmap-bonus.md` planifica ese trabajo. Ninguno de los dos diseña cómo se *siente* realmente usar el resultado final más allá de "reproduce una resolución." Este documento es esa capa: las distintas formas en que alguien puede ejecutar el binario 3D, lo bien que puede verse, y el menú completo de extras — para poder elegir de una lista pensada en vez de ir añadiendo cosas sobre la marcha.

Todo aquí es aditivo al límite del módulo `render/` ya existente en `04-architecture.md` — nada de esto propone tocar `solve/`, `parse/`, ni el binario obligatorio.

## 1. Tres formas de ejecutar el binario del bonus

Pensad en esto como tres "modos" que soporta el mismo renderizador, no tres programas distintos. Los tres comparten la misma escena de 26 cubies, el mismo motor `anim_*`, y el mismo cubo lógico — solo se diferencian en *quién controla los movimientos*.

| Modo | Quién controla los movimientos | Propósito |
|---|---|---|
| **A — Reproducción automática** | El solver. Mezcla de entrada -> resolver -> reproducir la solución automáticamente. | Es la base del bonus evaluado: "mostrar gráficamente el giro del cubo en tiempo real." Se entrega primero. |
| **B — Reproducción controlada** | La lista de movimientos del solver, pero un humano controla el ritmo/dirección. | Convierte la demo en algo que se puede estudiar de verdad — avanzar una resolución movimiento a movimiento, ralentizarla, saltar a un punto concreto. |
| **C — Práctica / manual** | El humano, directamente, un movimiento a la vez, exactamente como sujetar un cubo físico. | La petición de "usar el modelo como un cubo real": mezclar libremente, resolver a mano libremente, un cronómetro, y — bajo demanda — pasarle el estado actual al solver en vez de a un humano. |

Esto no son estados mutuamente excluyentes en el código — B es en realidad "A con un botón de pausa y un dial de velocidad," y C es "sin lista de movimientos, los movimientos vienen de eventos de entrada." El mismo motor de animación por debajo para los tres, según la interfaz `anim_push` / `anim_update` / `anim_current_transform` ya acordada en `06-roadmap-bonus.md`.

## 2. Modo B — el espectro tiempo real/cámara lenta

Esto es lo que pedisteis como "tiempo real, modos más lentos." Es un único parámetro de velocidad más un puñado de controles de transporte, no varias rutas de renderizado separadas.

| Control | Qué hace | Notas |
|---|---|---|
| **Deslizador de velocidad / atajos** | Milisegundos por movimiento, desde casi instantáneo (~60 ms, "tiempo real molón") hasta deliberadamente lento (~1.5 s, "ver exactamente qué está girando") | Ya previsto en el Sprint 6 de `06-roadmap-bonus.md` ("controles de pausa / paso / velocidad") — esta tabla solo detalla qué hay en el panel |
| **Pausa / reanudar** | Congela `anim_update(dt)` en la fracción de interpolación actual | Trivial — simplemente dejar de pasarle `dt` |
| **Paso (un movimiento)** | Avanzar exactamente un movimiento de la cola, y volver a pausar | Bueno para enseñar / depurar una etapa concreta de la resolución |
| **Scrub / línea de tiempo** | Saltar directamente al movimiento N de la solución | Necesita el estado lógico *acumulado* en el movimiento N, no solo la lista de movimientos — la forma más barata es reproducir los movimientos 0..N instantáneamente sobre una copia de trabajo del cubo lógico (sin animación) para obtener el estado, y luego reanudar la animación desde ahí. No intentéis interpolar hacia atrás a través de movimientos ya confirmados — ver el trade-off de reproducción inversa abajo. |
| **Reproducción inversa** | Reproducir la solución hacia atrás, giro a giro | Dos implementaciones: (1) *barata* — aplicar el inverso de cada movimiento en orden inverso como giros normales hacia adelante (deshacer una `R` es simplemente `R'`); (2) *elaborada* — animar de verdad el movimiento de deshacer. Con (1) basta; se ve idéntico en pantalla. |
| **Demo en bucle automático** | Al terminar, volver a mezclar y resolver automáticamente | Bueno para una demo desatendida en la sala de defensa / un stand dejado en marcha |

**Trade-off que merece la pena nombrar:** el scrub real arrastrando un tirador de línea de tiempo (como un editor de vídeo) es un "estaría bien," no un "hace falta" — requiere recalcular el estado en cada fotograma del arrastre. Un simple cuadro de texto "saltar al movimiento N" o botones +10/-10 consigue el 90% del valor por una fracción del trabajo de UI. Se recomienda la versión simple; construir la línea de tiempo arrastrable solo si sobra tiempo de verdad.

## 3. Modo C — "usarlo como un cubo real" (la parte nueva)

Esta es la pieza que ninguno de los documentos existentes diseña en profundidad — `06-roadmap-bonus.md` solo menciona "mezclar el cubo mediante la UI de giro manual del renderizador" una vez, como *gate de test*, no como funcionalidad. Aquí está el diseño real.

### 3.1 Qué hace falta para que se sienta bien

Un cubo real tiene exactamente un vocabulario de entrada: agarras una capa y la giras. Dos formas de ofrecer eso con teclado/ratón, no mutuamente excluyentes — entregar la primera, añadir la segunda si el tiempo lo permite:

1. **Entrada por notación de teclado (hacer esto primero — barato, cero ambigüedad).** Mapear `U R F D L B` (y mayúscula para `'`, un dígito para `2`) directamente a `anim_push`. Esto solo ya entrega "mezclar libremente y resolver a mano libremente" con una tarde de trabajo, y nunca puede ser ambiguo como sí puede serlo la selección con el ratón. Entregar esto incluso si el giro por arrastre de ratón (abajo) nunca se termina — es el respaldo que garantiza que el Modo C exista de todas formas.
2. **Giro por clic-y-arrastre de ratón (la versión impresionante).** Clic en una pegatina, arrastrar, la capa a la que pertenece gira siguiendo el arrastre, encajando al giro de 90° más cercano al soltar. Esta es la que de verdad *parece* manipular un cubo físico y merece el tiempo extra si el calendario lo permite.

### 3.2 Cómo funciona realmente la selección por clic-y-arrastre

Esta es la única pieza técnica genuinamente nueva, así que se detalla por completo:

1. **Rayo desde el ratón.** `GetMouseRay(mousePos, camera)` de raylib convierte la posición 2D del cursor en un rayo 3D desde la cámara a través de ese píxel. Esto es **raycasting**: lanzar una línea imaginaria dentro de la escena y preguntar qué es lo primero que golpea.
2. **Prueba de colisión contra los cubies.** Probar ese rayo contra la **bounding box** (caja delimitadora) de cada uno de los 26 cubies (una caja simple, `BoundingBox { min, max }`, que sustituye a la forma real del cubie porque es mucho más barato probar un rayo contra una caja que contra los quads reales) usando `GetRayCollisionBox` de raylib. Tomar la colisión más cercana.
3. **¿Qué cara se clicó?** El resultado de la colisión incluye un punto de impacto y una normal de superficie (cuál de las 6 direcciones señala la cara clicada). Esa normal, junto con la posición en la rejilla del cubie, indica a cuál de las 12 capas girables *podría* pertenecer el clic (un clic en la cara `+x` de un cubie podría pertenecer al giro de la capa R, o a una rotación sobre un eje distinto si luego se arrastra lateralmente por esa misma cara — ver el siguiente paso).
4. **La dirección del arrastre decide el eje.** Mientras el ratón se mueve con el botón pulsado, proyectar el delta del ratón sobre el plano de la cara clicada (un vector 2D simple en las direcciones locales arriba/derecha de esa cara). La dirección del plano a la que más se acerque el arrastre determina el eje de rotación; el signo del arrastre determina sentido horario o antihorario. Es el mismo truco que usa todo simulador de cubo controlado por ratón (p. ej. los conocidos de navegador) — elegir la cara, y dejar que un arrastre en ese mismo plano elija el giro, en vez de intentar inferir la intención solo del clic inicial.
5. **Encajar al soltar.** Mientras se arrastra, rotar visualmente los cubies de la capa seleccionada siguiendo al ratón (rotación parcial, no confirmada). Al soltar, encajar al giro legal de 90°/180° más cercano, animar el último tramo con el `easing` normal de `anim_*`, y confirmar en el estado lógico exactamente igual que un movimiento de reproducción automática — **reutilizando la misma ruta de confirmación que usa el Modo A.** Esto es importante: el Modo C nunca debe tener una ruta de código "aplicar este giro" separada de la del Modo A/B. Una única función `apply_move()`, llamada desde tres fuentes de entrada distintas (lista del solver, teclado, gesto de arrastre). Eso es lo que mantiene intacto el espíritu anti-trampa de `01-requirements.md` — un cubo mezclado a mano en el Modo C y luego pasado al solver atraviesa exactamente el mismo código de permutación que usa el binario obligatorio, así que no hay forma de que los dos se desincronicen silenciosamente.
6. **Rechazar un nuevo arrastre a mitad de animación.** Si un giro (de cualquier fuente) todavía se está animando, ignorar nuevos intentos de selección hasta que se confirme. Un cubo físico tampoco deja agarrar una segunda capa a mitad de un giro.

### 3.3 Cámara contra giro — el conflicto de entrada que hay que diseñar de antemano

El control de cámara orbital (rotar la *vista* de todo el cubo) y el giro de capas (rotar *parte* del cubo) quieren consumir ambos el arrastre con el botón izquierdo del ratón. Elegid una convención antes de escribir código de entrada, y anotadla en `DECISIONS.md` junto a la decisión de indexación de cubies del Sprint 0:

| Convención | Cómo funciona | Trade-off |
|---|---|---|
| **Separación por botón (recomendada)** | Arrastre con botón izquierdo sobre una pegatina = girar; arrastre con botón derecho (o central) en cualquier sitio = orbitar cámara | Un botón por intención, cero ambigüedad, pero puede resultar incómodo para quien use un trackpad sin un clic derecho real |
| **Tecla modificadora** | Arrastre izquierdo = orbitar por defecto; mantener Mayús/Ctrl + arrastre izquierdo = girar | Funciona bien en trackpad; cuesta un instante de "¿en qué modo estoy?" |
| **Regla de espacio vacío** | Arrastre izquierdo que empieza sobre un cubie = girar; arrastre izquierdo que empieza en fondo vacío = orbitar | No necesita modificador, encaja con la interacción natural, pero necesita una prueba de colisión al inicio del arrastre, no solo en la posición actual |

Se recomienda la **separación por botón**: no necesita estado extra, es inequívoca, y los ratones de las máquinas del campus todos tienen botón derecho (solo el caso de trackpad-únicamente resulta incómodo, y los clusters de 42 usan ratones de escritorio).

### 3.4 Funcionalidades extra del modo Práctica (una vez funcione la selección, o incluso solo con teclado)

- **Botón de mezclar** — llama exactamente al mismo generador de mezclas construido para el ítem "generador de mezclas integrado" de la lista de bonus (`06-roadmap-bonus.md` Sprint 7 #2); el Modo C es un segundo consumidor de esa misma función, no un motivo para construir una separada.
- **Cronómetro** — arranca en el primer movimiento tras una mezcla, se detiene al comprobar el estado resuelto, muestra el tiempo en marcha. La convención de inspección/cronómetro de un speedcuber si se quiere ir a por ello (mantener pulsada la barra espaciadora para prepararse es la estándar), pero un simple inicio/parada basta.
- **Contador de movimientos + TPS (turns per second, giros por segundo)** — gratis una vez que ya se están contando movimientos; TPS es solo movimientos dividido entre tiempo transcurrido.
- **Deshacer / rehacer** — cada giro manual ya pasa por `apply_move()`; mantener una pequeña pila de los movimientos inversos para deshacer, y una pila de rehacer que se vacía con cualquier giro manual nuevo (semántica estándar de deshacer/rehacer).
- **Traspaso "resuélvelo tú"** — un botón que toma el estado lógico en vivo actual, se lo pasa al solver exactamente igual que la ruta de la CLI obligatoria, y cambia la vista al Modo A/B para reproducir la solución devuelta. Es la funcionalidad más satisfactoria para hacer una demo en la defensa: mezclarlo a mano, mal, a propósito, delante del evaluador, y luego pulsar resolver.
- **Detección de estado resuelto** — necesaria para el cronómetro y para un aviso de celebración; trivial, es la misma comprobación que usa el propio solver para terminar su espacio de búsqueda a distancia 0.

## 4. Hacer que se *vea* bien — la capa de acabado visual

Todo en esta sección es genuinamente opcional respecto a las secciones 1-3 (que son el requisito real del bonus); tratadla como un backlog del que tirar según sobre tiempo, no como una lista de tareas por completar.

### 4.1 Iluminación y sombreado

raylib incluye un ejemplo básico de shader de iluminación (diffuse + algo de specular — **diffuse** es el sombreado plano dependiente de la dirección que hace que una cara se vea más brillante cuando mira hacia la luz; **specular** es el pequeño brillo que hace que una superficie parezca satinada/plástica en vez de mate) que es una mejora directa sobre el `DrawCube` plano/sin iluminar por defecto de raylib. Es el cambio visual de mayor valor por hora disponible: un cubo con aspecto de plástico y suavemente iluminado en vez de un juguete de sombreado plano se lee de inmediato como "alguien le puso cuidado a esto."

| Opción | Esfuerzo | Beneficio |
|---|---|---|
| `DrawCube` sin iluminar por defecto | Gratis (ya es la base) | Parece una vista de depuración |
| Shader de iluminación incluido en raylib (1-2 luces) | ~medio día | Gran salto visual por muy poco trabajo — **hacer este** |
| Shader personalizado (luz de contorno, brillo por pegatina, sombras suaves) | 1-2+ días | Rendimientos decrecientes salvo que alguien del equipo quiera específicamente la experiencia de escribir shaders |

### 4.2 Geometría y materiales

- **Cubies con esquinas redondeadas** en vez de cajas de aristas vivas — bien con una malla ligeramente biselada, bien con un truco barato (una textura/normal map sutil de esquina redondeada en cada quad de pegatina). Puramente cosmético, pero es el detalle que más hace que un render "se lea" como una foto de un cubo real en vez de una demo de cajas de colores.
- **Separación entre pegatinas / marco de plástico** — dejar un borde fino negro o gris oscuro entre pegatinas en vez de caras coloreadas tocándose borde con borde. Barato (basta con encoger ligeramente cada quad de pegatina dentro de su cara) y con un beneficio de realismo percibido muy alto.
- **Temas de color / skins** — el esquema clásico de 6 colores por defecto, más una o dos paletas alternativas (p. ej. una paleta segura para daltonismo — cambiando rojo/verde por algo más distinguible — y quizá una paleta divertida solo por variedad en la demo). Se lee del mismo mapeo lógico color-por-facelet, solo que a través de otra tabla de consulta, así que sale barato una vez que el render base funciona.

### 4.3 Cámara y entorno

- **Cámara orbital con límites razonables** — ya cubierto por `Camera3D` + `UpdateCamera` de raylib (según `03-graphics.md`); solo recordad limitar el zoom/pitch para que el usuario no pueda voltear la cámara boca abajo ni atravesar el cubo.
- **Presets de cámara** — un atajo cada uno para una vista isométrica de 3 caras de frente, un primer plano "mira cómo gira esta cara" que sigue automáticamente la capa que está girando, y una órbita automática lenta para un modo de reposo/salvapantallas de demo.
- **Fondo** — un degradado plano o un skybox simple (una gran esfera/cubo de fondo con textura que simula un entorno lejano, dando a la escena sensación de lugar) supera a un color plano por casi ningún coste extra.
- **Sombra de contacto en el suelo** — una única sombra suave difusa bajo el cubo (falsa, no un shadow map real) hace que parezca apoyado sobre una superficie en vez de flotando. Barato, merece la pena.

### 4.4 "Juice" — efectos de feedback

("Juice" es un término habitual en desarrollo de videojuegos para pequeños efectos de feedback que hacen que las interacciones se sientan satisfactorias aunque no cambien la lógica subyacente — sacudida de pantalla, ráfagas de partículas, sonido, etc.)

- **Sonido de clic/whoosh al girar** en cada movimiento confirmado (raylib tiene soporte de audio trivial: `InitAudioDevice` + `PlaySound`).
- **Celebración al completar la resolución** — una breve ráfaga de partículas, un destello de color, o un florituro de cámara al detectar el estado resuelto. Puramente cosmético, alto valor de demo, una tarde de trabajo.
- **Resaltado del movimiento** — un contorno o brillo breve en la capa que está girando, para que quien vea una reproducción automática rápida pueda seguir qué cara acaba de moverse.

### 4.5 HUD

Ya previsto en el Sprint 6 de `06-roadmap-bonus.md` ("contador de movimientos, movimiento actual mostrado en notación, barra de progreso de la solución"). Extras que merece la pena añadir a esa base:

- **Etiqueta de fase** — gratis en cuanto exista el ítem #4 del Sprint 7 ("subpasos comprensibles para humanos"); mostrar la fase actual de Kociemba junto al movimiento.
- **Indicador de modo** — cuál de A/B/C está activo actualmente, más el ajuste de velocidad actual.
- **Panel de estadísticas** — número total de movimientos, tiempo de resolución transcurrido, movimientos por segundo, cosas que ya se están rastreando para otras funcionalidades.
- **Pistas de teclas en pantalla** — una leyenda pequeña siempre visible ("arrastra una pegatina para girar - arrastre derecho para orbitar - espacio para mezclar") para que el Modo C se pueda descubrir sin necesidad de un README.

## 5. Backlog maestro de extras (MoSCoW)

**MoSCoW** es un conjunto simple de etiquetas de prioridad — **M**ust have (imprescindible), **S**hould have (debería tener), **C**ould have (podría tener), **W**on't have (no esta vez) — útil aquí porque la lista de abajo es mucho más larga de lo que permite el tiempo disponible, y la idea es decidir *ahora*, sobre el papel, qué se recorta primero si el Sprint 7 se alarga, en vez de decidirlo bajo presión de plazo.

| Ítem | Prioridad | Esfuerzo est. | Depende de |
|---|---|---|---|
| Reproducción automática de la salida del solver (Modo A) | **Must** | listo tras el Sprint 6 | -- |
| Controles de reproducir/pausar/paso/velocidad (Modo B) | **Must** | ~1 día | Modo A |
| Giros manuales por teclado (Modo C, teclado) | **Must** | ~1 día | ruta compartida `apply_move()` |
| Botón de mezclar + cronómetro + contador de movimientos | **Should** | ~1 día | Modo C teclado |
| Shader de iluminación básico | **Should** | ~medio día | ninguno |
| Separación entre pegatinas / marco de plástico | **Should** | ~medio día | ninguno |
| Deshacer/rehacer en el Modo C | **Should** | ~medio día | Modo C teclado |
| Botón de traspaso "resuélvelo tú" | **Should** | ~medio día | solver ya invocable in-process |
| Giro por clic-y-arrastre de ratón (Modo C, ratón) | **Could** | ~2-3 días | versión de teclado ya entregada |
| Presets de cámara + órbita automática en reposo | **Could** | ~medio día | ninguno |
| Temas de color / skins | **Could** | ~medio día | ninguno |
| Sonido de giro + celebración al resolver | **Could** | ~medio día | ninguno |
| Geometría de cubies con esquinas redondeadas | **Could** | ~1 día | ninguno |
| Scrub de movimientos/línea de tiempo | **Could** | ~1 día | Modo B |
| Sombra de suelo / skybox | **Could** | ~medio día | ninguno |
| Trabajo de shader personalizado (luz de contorno, sombras suaves) | **Won't** (salvo ir muy adelantados) | 1-2+ días | iluminación básica entregada |
| Tirador de scrub de línea de tiempo arrastrable | **Won't** | ~1-2 días | scrub simple entregado |
| Visualización VR/AR | **Won't** | grande, sin acotar | totalmente fuera de alcance -- sin valor de defensa proporcional al coste |

Leed la fila "Should" antes de tocar nada de "Could" — esos cinco ítems son los que hacen que el Modo C se sienta realmente completo y que el render se vea cuidado, por unos 3.5 días combinados. Todo lo que hay por debajo de esa línea es bonus-sobre-el-bonus genuino.

## 6. Añadidos de arquitectura

Un módulo nuevo, una extensión de interfaz, una máquina de estados — todo lo demás en `04-architecture.md` se mantiene tal cual está escrito.

```
render/
  ...
  input.c         NUEVO -- raycasting/selección, giro por arrastre, la
                  separación de botón cámara-contra-giro, mapeo de
                  notación de teclado. Depende solo de cube/ (para
                  apply_move) y de las posiciones en la rejilla de los
                  cubies de geometry.c -- misma regla que todo lo demás en
                  render/: nunca incluye nada de parse/ ni de solve/,
                  salvo una única llamada explícita a solve/ para el
                  traspaso "resuélvelo tú".
```

- **Máquina de estados del modo** — tres estados, `MODE_AUTOPLAY`, `MODE_PAUSED`, `MODE_MANUAL`. Transiciones legales: autoplay <-> paused libremente; manual -> autoplay solo mediante la acción explícita "resuélvelo tú"; autoplay -> manual no está permitido mientras hay una resolución en cola (terminarla o cancelarla primero). Mantenedlo como un enum explícito y un switch, no una dispersión de booleanos — es lo bastante pequeño como para que el diagrama de estados quepa en `DECISIONS.md` como un párrafo.
- **Extensión de la interfaz `anim_*`** — las firmas existentes (`anim_push(move)`, `anim_update(dt)`, `anim_current_transform(cubie_id)`) no necesitan cambiar. Lo nuevo es *quién llama a `anim_push`*: la lista de movimientos del solver (Modo A/B) o el código de selección/teclado de `input.c` (Modo C) — ambos convergen en la misma llamada. Añadir una más: `anim_push_partial(cubie_group, axis, angle)` para la rotación en vivo, todavía no confirmada, mientras hay un arrastre en curso, que es solo visual y nunca toca el estado lógico hasta que soltar dispara el `anim_push` normal.
- **Un único punto de entrada `apply_move()`.** Las tres fuentes de entrada llaman a la misma función que ya usan tanto la reproducción del binario obligatorio como el solver. Esta es la regla de diseño estructural de todo este documento — es lo que hace que las mezclas manuales del Modo C sean una entrada de confianza para el solver, y es una extensión directa del principio anti-trampa ya establecido en `01-requirements.md` y `04-architecture.md`.

## 7. Riesgos específicos de esta capa

| Riesgo | Severidad | Mitigación |
|---|---|---|
| La selección por arrastre de ratón (§3.2) resulta delicada/con bugs bajo presión de tiempo | Media | La entrada por notación de teclado (§3.1.1) se entrega primero y de forma independiente -- el Modo C existe y es demostrable aunque el giro por arrastre nunca llegue a terminarse |
| La órbita de cámara y el giro de capa se pelean por la misma entrada (ambos quieren el arrastre izquierdo) | Media | Decidir la convención de separación por botón (§3.3) en el Sprint 5, anotarla en `DECISIONS.md`, antes de escribir código de entrada |
| El backlog de acabado visual (§4-5) consume tiempo que pertenece al núcleo obligatorio/bonus | Media-Alta | La tabla MoSCoW en §5 es la línea de corte pre-acordada -- si el Sprint 7 se alarga, parar al final de "Should," no a mitad de "Could" |
| Una segunda ruta de código, divergente, aplica los giros manuales de forma distinta a los del solver | Alta (invalida el argumento anti-trampa) | La regla de `apply_move()` único en §6; probarlo directamente -- mezclar vía Modo C, pasarle el estado resultante al solver, comprobar que lo resuelve (esto ya es exactamente el gate del Sprint 6 en `06-roadmap-bonus.md`, solo que ahora también ejercitado desde la ruta de ratón/teclado, no solo con una mezcla de test fija) |

## 8. Dónde encaja esto en el roadmap existente

No hacen falta sprints nuevos — esto refina lo que ya está dentro del Sprint 7 de `06-roadmap-bonus.md`, y lo reordena ahora que "usable como un cubo real" es un objetivo explícito y no implícito:

- **Sprint 6** (sin cambios) entrega el Modo A, que es `apply_move()` más la cola dirigida por el solver — construidlo de forma que `input.c` pueda llamar a la misma función más adelante, y así este documento entero no cuesta nada extra después.
- **Sprint 7**, reordenado: el Modo C por teclado y los controles de reproducir/pausar/paso/velocidad pasan al *principio* del Sprint 7 (son baratos y son la petición real de "cubo real"), por delante de los ítems de multi-algoritmo/solver óptimo listados originalmente, que siguen siendo valiosos pero son independientes de este documento. Los ítems "Should" de acabado visual encajan al lado, ya que son pequeños y no bloquean nada. El giro por arrastre de ratón y todo lo marcado "Could" arriba solo se intentan una vez hechas las filas "Must"/"Should" y si todavía queda margen antes del Sprint 8.

## 9. Decisiones abiertas para llevar a `08-risks-and-open-decisions.md`

1. **Separación por botón vs. tecla modificadora vs. regla de espacio vacío** para la entrada de órbita-de-cámara contra giro-de-capa (§3.3) — se recomienda separación por botón.
2. **¿Modo C solo por teclado, o también giro por arrastre de ratón?** — se recomienda construir primero el teclado sin condiciones, tratando el arrastre de ratón como un objetivo adicional.
3. **¿El modo Práctica/Manual se enseña en la defensa, o se queda como un extra privado de "esto también lo hicimos"?** — es un momento de demo fuerte ("miradme mezclarlo mal, y ahora miradlo resolverse"), pero solo merece la pena ensayarlo si es fiable; decidirlo una vez exista.
