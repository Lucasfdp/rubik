# Requisitos, Restricciones y Números de Contexto

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

## 1. Lo que el subject realmente exige

| # | Requisito | Dónde afecta |
|---|---|---|
| R1 | El programa recibe un **mix (scramble) como argumento único**, saca la solución por stdout | CLI + parser |
| R2 | La notación es **F R U B L D** con solo los modificadores `'` y `2`. **Sin cortes M/E/S, sin rotaciones x/y/z** — ni en la entrada ni en la salida | Parser + conjunto de movimientos |
| R3 | La métrica es **HTM** (half-turn metric — métrica de giro completo, ver glosario): cualquier giro de 90° o 180° de una cara = 1 movimiento. La puntuación es `wc -w` sobre nuestra salida | Formato de salida: una sola línea, separada por espacios, sin palabras extra |
| R4 | **Media ≤ 50 movimientos**, **tiempo máximo de respuesta 3 segundos** | Elección del algoritmo (`02-algorithms.md`) |
| R5 | **Devolver el inverso del scramble es hacer trampa.** Tampoco vale "meter una secuencia sin efecto para rellenar" | Arquitectura — ver "la regla anti-trampa" más abajo |
| R6 | Sin segfault, sin fuga de memoria, sin double free, sin bucle infinito. "Cero tolerancia." | Puerta de Valgrind en el CI/Makefile |
| R7 | Los errores deben gestionarse correctamente | Tokens inválidos, cubo inválido, entrada vacía, sin argumento, demasiados argumentos |
| R8 | Makefile con las reglas habituales | `all clean fclean re` (+ `bonus`) |
| R9 | Cualquier librería está permitida **si podemos justificarla**. Una librería que hace el trabajo por nosotros no vale | raylib = abstracción de ventanas/gráficos, vale. Una librería que resuelve cubos = no vale |
| R10 | Debemos poder **explicar el algoritmo con palabras sencillas y conceptos visuales** | `07-defence-prep.md` |
| R11 | El bonus solo se evalúa si la parte obligatoria está **perfecta** | Orden: la obligatoria se entrega primero, sin excepciones |

Una nota rápida sobre la jerga de esa tabla: "la parte obligatoria" (mandatory) es simplemente la parte requerida y no-bonus del proyecto — la que hay que terminar y dejar perfecta antes de que cualquier trabajo de bonus cuente para algo (ver R11).

## La regla anti-trampa (la decisión de arquitectura más importante de todas)

R5 es la forma más fácil de suspender esta defensa, y es una decisión de arquitectura, no de código. Hay que aplicarla a nivel de tipos — es decir, hacer que sea imposible equivocarse según cómo están escritas las firmas de las funciones, no solo algo que prometéis recordar:

```
solve(const t_cube *state) -> char *
```

El solver **nunca recibe el string del scramble**. Recibe un *estado* del cubo (la representación interna de qué color de pegatina está en cada sitio — ver "cubie" y "facelet" en el glosario). `main` parsea el scramble, lo aplica a un cubo resuelto, descarta el string, y le pasa al solver únicamente el estado resultante.

Por qué importa: cualquier evaluador que pregunte "¿cómo sabemos que no estáis simplemente invirtiendo el mix?" recibe una respuesta de una línea, verificable — leer la firma de la función. Es físicamente imposible que reciba el string que necesitaría para invertirlo.

Beneficio secundario: como el solver solo ve un estado, podéis alimentarlo con un estado de *cualquier* origen — un generador de estados aleatorios, un archivo, movimientos hechos a mano en el visor 3D — y sigue funcionando. Esto os da dos puntos de bonus (generador de scrambles, y resolver un cubo girado manualmente en el visor) casi gratis.

## 2. Números de contexto que todo el equipo debería conocer

No son trivia — cada uno explica directamente una decisión de diseño posterior.

- El cubo tiene **43.252.003.274.489.856.000** estados alcanzables (unos 4,3×10¹⁹). Esto es *por qué* no se puede simplemente buscar en todo el problema directamente — ningún ordenador puede almacenar o recorrer un espacio de ese tamaño. Es el número que justifica cada truco de "coordenada" y "tabla de poda" en `02-algorithms.md`.
- **El Número de Dios es 20 en HTM.** Esto significa: toda posición de esas 4,3×10¹⁹ puede resolverse en 20 movimientos o menos, y esto se *demostró* (no solo se observó) en 2010, usando unos 35 años-CPU de cómputo donados por Google. Es el suelo teórico — ningún solver, por listo que sea, puede bajar de "20 movimientos en el peor caso" porque está demostrado que 20 bastan y que algunas posiciones necesitan exactamente eso.
- HTM (half-turn metric, métrica de giro completo) es la misma métrica que usa el subject: cualquier giro de cualquier cara — 90° o 180° — cuenta como un movimiento. Es el "movimiento" que R3 cuenta con `wc -w`.
- Para calibrar, métodos de resolución humanos: **CFOP** (un método competitivo habitual) promedia ~56 movimientos; el **método capa por capa de principiante** promedia unos 110, y una muestra más grande (10.000 scrambles) sitúa la media del método de principiante más cerca de 210 cuartos de giro, en un rango de 80 a 321.

**Leed esa última viñeta otra vez.** Un solver capa por capa — el enfoque intuitivo, "resolverlo como lo haría una persona" — **no** cumple R4 (≤50 movimientos de media). Esta es la trampa en la que caen la mayoría de los primeros intentos: es el algoritmo más fácil de escribir y de explicar, y también el que tiene garantizado fallar el requisito de número de movimientos. Ver `02-algorithms.md` §A para el veredicto completo sobre por qué sigue siendo útil, solo que no como el solver que se entrega.
