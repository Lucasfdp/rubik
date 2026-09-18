# Roadmap — Parte Bonus (3D)

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

Mismo principio que el roadmap obligatorio: los dos tocáis el trabajo 3D. La costura aquí es **motor de animación vs. escena/apariencia**, que es una interfaz de verdad — `anim_push(move)`, `anim_update(dt)`, `anim_current_transform(cubie_id)` — no una división arbitraria.

## Sprint 5 — Esqueleto del renderizador (los dos, en pareja, ~2 días)

Juntos otra vez, por la misma razón que en el Sprint 0: esto fija el contrato compartido para todo lo que viene después.

- El pico de compilación: conseguir que el stack elegido (raylib, según `03-graphics.md`, o MLX como respaldo) compile y enlace de verdad en una máquina del cluster. **Hacedlo antes de escribir código de renderizado de verdad.** Si raylib falla aquí, cambiad a MLX *ahora*, en la primera semana — no descubrirlo en la tercera semana cuando ya no queda tiempo para pivotar.
- Ventana, cámara, los 26 cubies estáticos colocados en la rejilla, control orbital con el ratón, y colores de pegatina leídos correctamente desde un estado lógico resuelto.
- Acordar con precisión las firmas de la interfaz `anim_*`, y escribirlas en `DECISIONS.md`.

## Sprint 6 — Dividir el bonus (~5 días)

| | Dev A | Dev B |
|---|---|---|
| **Se encarga de** | `render/anim.c` — el motor de animación | `render/geometry.c` + `render/hud.c` — escena y apariencia |
| **Construye** | Selección de cara por coordenada de rejilla; interpolación 0→±90° con easing; **aplicar al estado lógico al completar**; la cola de movimientos; controles de reproducir / pausar / paso / velocidad | Malla de cubies + mapeo de colores de pegatina desde el estado vivo del cubo; sombreado plano o iluminación básica; el HUD (contador de movimientos, movimiento actual mostrado en notación, barra de progreso de la solución); presets de cámara |
| **Puerta** | Reproducir una secuencia aleatoria de 1.000 movimientos a máxima velocidad; el estado renderizado final debe ser **exactamente** igual al estado lógico — cero deriva de coma flotante | Revolver el cubo vía la interfaz de giro manual del renderizador, volcar el estado del cubo resultante, pasárselo al solver, y verificar que efectivamente se resuelve. Esto demuestra que el estado renderizado y el estado lógico nunca divergen |

## Sprint 7 — Ítems de bonus nombrados (~3 días)

Elegid de la propia lista de bonus del subject. Ordenados aquí de mayor a menor valor — haced primero los ítems de mayor valor:

1. **Resolución animada en tiempo real** — el ítem de bonus estrella. Ya hecho a partir del Sprint 6.
2. **Generador de scrambles integrado**, con longitud/cantidad configurables (por ejemplo, flags `-g LONGITUD`, `-n CANTIDAD`). Barato, nombrado explícitamente en la lista de bonus del subject, y además sirve como motor para vuestro propio arnés de benchmark del Sprint 3.
3. **Selección multi-algoritmo** (ver `02-algorithms.md` §3.E) — añadir Thistlethwaite como segundo algoritmo y comparar/imprimir ambos. El Dev A se encarga de la implementación del algoritmo, el Dev B de la interfaz de comparación.
4. **Subpasos "comprensibles para humanos"** — anotar la solución impresa con etiquetas de fase, p. ej. `[Fase 1: orientar todo] R U2 F' ... [Fase 2: terminar en G1] U D2 R2 ...`. Casi gratis con Kociemba, ya que el límite entre fases ya se conoce internamente. Esto queda muy bien en la defensa.
5. **Soporte para 2×2×2** — el modelo de cubies se generaliza bastante directamente a esto; los codificadores de coordenadas no (necesitarían rehacerse). Solo intentarlo si sobra tiempo de verdad.
6. **Solver óptimo detrás de un flag `-optimal` con timeout** (ver `02-algorithms.md` §3.D) — el ítem de mayor coste de esta lista. Hacedlo el último, o no hacerlo.

Según el subject, cada flag de opción debe llevar el prefijo `-`, y la entrada de mix/scramble debe seguir siendo siempre una secuencia de movimientos válida sin importar qué combinación de flags se use.

## Sprint 8 — Ensayo de defensa (los dos, 1 día)

Intercambiad módulos: cada uno presenta el código *del otro*, de memoria, en la pizarra. Lo que no se pueda explicar sobre la marcha se arregla o se elimina antes de la defensa real. Este es también el último punto de control para R11 — si algo de la parte obligatoria retrocedió mientras se construía el bonus, todo el bonus vale cero, así que esta es la última oportunidad de detectarlo.
