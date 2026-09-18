# Arquitectura Objetivo

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

Los límites de los módulos de abajo están elegidos para que (a) la regla anti-trampa (ver `01-requirements.md`) quede reforzada *estructuralmente*, por la propia forma del código, no solo por convención — y (b) dos personas puedan trabajar en piezas separadas al mismo tiempo sin pisarse constantemente los cambios.

```
src/
  main.c            CLI: parseo de argumentos, despacho de opciones, salida
  parse/
    notation.c      "R2 D' B'" -> t_move[]   (rechaza M/E/S/x/y/z)
    validate.c      legalidad del cubo: paridad de permutación, twist %3, flip %2
  cube/
    cubie.c         perm + orientación de esquina/arista; el modelo autoritativo
    facelet.c       conversión 54-pegatinas <-> cubie (necesaria para I/O y el renderizador)
    moves.c         los 18 movimientos como tablas de permutación
  coord/
    encode.c        cubie -> twist / flip / slice / cperm / eperm / sperm
    movetable.c     genérico: coord x movimiento -> coord
    prune.c         genérico: constructor de tabla de poda por BFS
  solve/
    ida.c           motor IDA* genérico sobre (coords, tablas de poda, conjunto de movimientos)
    kociemba.c      fase 1 + fase 2 + bucle de iteración sub-óptima
    thistle.c       (opcional) la variante de cuatro fases
  render/           <-- solo bonus, nunca enlazado en el binario obligatorio
    geometry.c      26 cubies en la rejilla
    anim.c          cola de movimientos, easing, aplicación al completar
    draw.c          específico del stack: raylib O rasterizador mlx
    hud.c           contador de movimientos, controles
```

## Dos reglas que mantienen esto honesto

- **`solve/` no debe incluir nada de `render/`, y `render/` no debe incluir nada de `parse/`.** La única interfaz del renderizador con el resto del programa es: recibir un estado del cubo, recibir una lista de movimientos. Ese es todo el contrato. Esto es lo que hace estructuralmente imposible que el renderizador filtre de algún modo el string del scramble original hacia el proceso de resolución, o viceversa.
- **`make` compila el binario obligatorio con cero código de gráficos enlazado en él.** `make bonus` compila un *segundo* binario, separado, que incluye el renderizador. Esto importa por R11 (el bonus solo cuenta si la parte obligatoria es perfecta): si la mitad gráfica tiene un bug o falla al compilar en la máquina del evaluador, el binario obligatorio no se ve afectado en absoluto y sigue compilando y funcionando. La pequeña duplicación en la configuración del build merece la pena por esa protección.

## Por qué la división está en la capa de coordenadas, y no en otro sitio

La capa de coordenadas (`coord/encode.c`, concretamente la firma de función `uint16_t encode_x(const t_cube *)`) es una costura limpia para dividir el trabajo entre dos personas porque se puede acordar sobre el papel *antes* de que ninguno de los dos escriba código, y es comprobable de forma independiente por ambos lados: quien lleve los codificadores de coordenadas puede escribirlos y verificarlos sin necesitar que el motor de búsqueda ya exista, y quien lleve el motor de búsqueda puede escribirlo y probarlo contra un codificador de mentira antes de que el real esté terminado. Ver `05-roadmap-mandatory.md` para cómo la división real sprint a sprint usa esta costura.
