# Rubik (42, v5) — Documentos de Preparación y Decisiones

**Objetivo: resolver un cubo 3×3×3 en ≤50 movimientos de media, ≤3s, en C. Equipo de 2.**

Este es el índice. Cada archivo enlazado cubre una zona de decisión completa, con explicaciones en lenguaje sencillo de los términos técnicos a medida que aparecen. Cualquier cosa que no reconozcas a simple vista casi seguro está en `GLOSSARY.md` — ese archivo está pensado para tenerlo abierto en una segunda pestaña mientras lees los demás.

## Orden de lectura

1. **`01-requirements.md`** — lo que el subject realmente nos obliga a hacer, y los números que explican *por qué* (tamaño del espacio de estados, el Número de Dios, por qué falla el método capa por capa).
2. **`02-algorithms.md`** — la decisión del solver. Cinco opciones comparadas, se recomienda el algoritmo de dos fases de Kociemba.
3. **`02a-cube-notation.md`** — el nombre de cada cubie y cada facelet, la indexación de facelets de Kociemba, y cómo se codifican el giro de esquina y el volteo de arista. Leedlo junto al §3.0 de `02-algorithms.md`.
4. **`03-graphics.md`** — la decisión del bonus 3D. raylib vs. rasterizador hecho a mano vs. OpenGL puro.
5. **`04-architecture.md`** — cómo está organizado el código para que la regla anti-trampa sea estructural y dos personas puedan trabajar en paralelo.
6. **`05-roadmap-mandatory.md`** — plan sprint a sprint para la parte obligatoria.
7. **`06-roadmap-bonus.md`** — plan sprint a sprint para el bonus 3D.
8. **`07-defence-prep.md`** — las preguntas que te harán en la evaluación, con respuestas listas para pizarra.
9. **`08-risks-and-open-decisions.md`** — qué podría salir mal, y las decisiones que aún os quedan por tomar a los dos.
10. **`09-sources.md`** — toda afirmación de arriba que no sea "esto lo decidimos nosotros" viene de aquí.
11. **`GLOSSARY.md`** — todos los términos técnicos usados en todos los archivos anteriores, definidos desde cero. No se asume conocimiento previo de teoría de grupos ni de gráficos.

## Recomendación, por adelantado

| Decisión | Recomendación | Confianza |
|---|---|---|
| Algoritmo del solver | **Kociemba de dos fases**, sobre un framework genérico de coordenadas + IDA* | Alta |
| Algoritmo alternativo / de respaldo | **Thistlethwaite de cuatro fases** (~70% de código compartido, punto bonus barato) | Media |
| Stack 3D | **raylib** (C99, licencia permisiva, sin dependencias del sistema que instalar) | Alta |
| Alternativa purista para el 3D | **MiniLibX + nuestro propio rasterizador por software** | Media |
| Descartado | Korf IDA*+PDB como el solver *que se entrega*; ncurses/ASCII como el bonus | Alta |

El razonamiento detrás de cada fila está en `02-algorithms.md` y `03-graphics.md`. Si no estáis de acuerdo con alguna, las tablas de trade-offs de esos archivos son con las que hay que argumentar — para eso están.

## La regla más importante de todas

Antes que nada: la función del solver nunca debe ver el string del scramble. Solo recibe un *estado* del cubo. Esta única decisión de diseño es lo que convierte "demuestra que no estáis haciendo trampa" en una respuesta de una sola línea en vez de un riesgo en la defensa. Explicación completa en `01-requirements.md`, bajo "la regla anti-trampa".
