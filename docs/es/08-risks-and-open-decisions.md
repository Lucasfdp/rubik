# Registro de Riesgos y Decisiones Abiertas

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

## Registro de riesgos

| Riesgo | Gravedad | Mitigación |
|---|---|---|
| Empezar el trabajo de bonus antes de que la parte obligatoria esté perfecta → el bonus se evalúa como cero (R11) | **Alta** | La checklist de congelación del Sprint 4 (`05-roadmap-mandatory.md`) es una puerta estricta. La parte obligatoria y el bonus son binarios separados, así que una regresión en el bonus no puede tocar el obligatorio. |
| Desacuerdo entre los dos sobre la indexación de cubies | **Alta** | Fijado juntos en el Sprint 0, escrito en `DECISIONS.md`, nunca cambiado después. |
| Tabla de poda no admisible (que sobreestima la distancia) → respuestas incorrectas de forma intermitente | Alta | Test de propiedad: para estados aleatorios, comprobar que siempre se cumple `valor_poda ≤ longitud_solución_real`. |
| Entrada de cubo inválida → la búsqueda nunca termina → fallo de R6 | Alta | Validar los invariantes de paridad/orientación *antes* de entrar en la búsqueda. Sin excepciones a esto. |
| raylib no compila en las máquinas del cluster | Media | Pico de compilación de 1 hora en el Sprint 5, **antes** de escribir código de renderizado; plan de respaldo con MLX acordado de antemano. |
| La deriva de coma flotante en la animación 3D desincroniza el render del estado lógico | Media | Aplicar al completar + reiniciar la transformación de la animación a la identidad después de cada movimiento; el test de deriva de 1.000 movimientos del Sprint 6 detecta esto. |
| La generación de tablas al arrancar empuja el tiempo total cerca del límite de 3s | Baja | Las tablas pesan menos de 10 MB y se generan por BFS en bastante menos de un segundo — medidlo directamente en el Sprint 3 de todas formas, no lo asumáis sin más. |
| Uno de los dos no puede defender el módulo del otro | Media | El ensayo de intercambio del Sprint 8; el cruce deliberado ya integrado en los Sprints 1–2 y 6. |
| La entrada de ratón para órbita de cámara y giro de capa se pelean entre sí; el backlog de acabado visual consume tiempo pensado para el núcleo de algoritmo/render | Media | Ver `03a-3d-experience-and-interaction.md` §7 para las mitigaciones concretas (convención de separación por botón decidida en el Sprint 5; el backlog MoSCoW como línea de corte pre-acordada). |

## Decisiones abiertas para los dos

1. **¿Solo Kociemba, o Kociemba + Thistlethwaite?** (`02-algorithms.md` §3.C vs. §3.E). Recomendación: construir la base compartida de forma genérica, entregar Kociemba como el solver principal, y añadir Thistlethwaite en el Sprint 7 solo si vais por delante del calendario.
2. **¿raylib o MLX?** (`03-graphics.md` §4.A vs. §4.B). Recomendación: decidir según el resultado del pico de compilación del Sprint 5, no por preferencia personal — el resultado del pico es la señal más fiable.
3. **¿Quién es Dev A y quién Dev B?** El track A se inclina más hacia I/O y la fontanería de corrección; el track B se inclina más hacia combinatoria y búsqueda. Intercambiad roles entre el Sprint 2 y el Sprint 6, para que ninguno de los dos haga el mismo tipo de trabajo dos veces.
4. **¿Un binario con un flag `-v` para lo visual, o dos binarios separados?** Recomendación: dos — esto protege el build obligatorio de cualquier rotura por el lado del bonus (ver `04-architecture.md`).
5. **¿Generar las tablas al arrancar, o cachearlas en disco?** Recomendación: al arrancar. Es lo bastante rápido como para quedar bien dentro del presupuesto de tiempo, y elimina una pregunta incómoda en la defensa sobre si las tablas en caché cuentan como trampa precalculada.
6. ~~Separación por botón vs. tecla modificadora vs. regla de espacio vacío~~ — **DECIDIDO**: separación por botón (arrastre izquierdo gira una capa, arrastre derecho orbita la cámara). Ver `03b-3d-implementation-plan.md` Fase 3 para la implementación concreta.
7. ~~¿Modo manual solo por teclado, o también giro por clic-y-arrastre de ratón?~~ — **DECIDIDO**: el teclado se entrega primero, funcionando por completo (`03b-3d-implementation-plan.md` Fase 3), antes de intentar el clic-y-arrastre de ratón (Fase 6).
8. **¿El modo Práctica/Manual se enseña en la defensa?** (`03a-3d-experience-and-interaction.md` §9). Recomendación: decidirlo una vez exista y haya demostrado ser fiable — es un momento de demo fuerte si funciona bien, un riesgo si no.
