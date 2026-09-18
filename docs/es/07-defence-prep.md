# Preparación de la Defensa

*(Parte de los documentos de preparación de Rubik — ver `00-overview.md` para el índice y `GLOSSARY.md` para las definiciones de los términos.)*

El subject dice: *"debéis poder explicar vuestro algoritmo con palabras sencillas y conceptos visuales."* Así que las respuestas de abajo están escritas para funcionar en una pizarra, no en código — practicad diciéndolas en voz alta, no solo leyéndolas.

**1. "¿Cómo sabemos que no estáis simplemente invirtiendo el scramble?"**
→ Señalad la firma de función del solver (ver la regla anti-trampa en `01-requirements.md`): solo recibe un estado del cubo, nunca el string del scramble, así que no hay nada que invertir. Ofreceos a resolver un estado que el evaluador construya a mano en el visor 3D — como el solver funciona desde *cualquier* estado, no solo desde los producidos al parsear un string de scramble, esto es una prueba en vivo, sobre la marcha.

**2. "¿Por qué dos fases?"**
→ Dibujad el embudo en la pizarra: 4,3×10¹⁹ estados posibles es demasiado para buscar directamente, por cualquier ordenador. La fase 1 no resuelve nada por sí misma — hace que el cubo se comporte *bien*: cada arista y esquina orientada correctamente, las aristas de la capa del medio en su sitio. Esa única restricción reduce el problema restante a unos 20.000 millones de estados, que — a diferencia de 4,3×10¹⁹ — sí que es lo bastante pequeño como para buscarlo directamente. La fase 2 termina entonces el trabajo usando solo movimientos que están garantizados a no deshacer lo que consiguió la fase 1.

**3. "¿Qué es una coordenada?"**
→ Una huella digital con pérdida. `twist = 2187` simplemente significa "cómo están giradas las 8 esquinas," escrito como un número en base 3 de 7 dígitos (porque el giro de la 8ª esquina queda matemáticamente forzado por las otras siete). Millones de disposiciones de cubo genuinamente distintas comparten la misma huella — y ese es precisamente el punto: se busca sobre huellas, que es un espacio pequeño, en vez de sobre cubos reales, que es astronómicamente grande.

**4. "¿Qué es una tabla de poda?"**
→ Una respuesta precalculada a "¿cuántos movimientos, como mínimo, desde esta huella hasta el objetivo?" — para cada huella posible, calculada una vez de antemano. Si la búsqueda lleva 10 movimientos de profundidad, buscando una solución de longitud 12, y la tabla dice "se necesitan al menos 5 movimientos más desde aquí," la búsqueda puede abandonar esa rama de inmediato — es imposible que llegue a longitud 12 con solo 2 movimientos restantes por probar. La única regla estricta: la tabla nunca debe *sobreestimar*, o la búsqueda puede devolver una respuesta equivocada.

**5. "¿Es óptimo?"**
→ No, y deliberadamente no. El solver verdaderamente óptimo (el de Korf, ver `02-algorithms.md` §3.D) tarda minutos u horas por cubo. Este solver produce soluciones de unos 21 movimientos, frente a un óptimo del peor caso demostrado de 20 y un requisito del subject de 50 — todo en milisegundos. Mostrad la salida del benchmark del Sprint 3 en `05-roadmap-mandatory.md` como prueba.

**6. "Justificad raylib."** *(o: "justificad MiniLibX," si fuisteis por ese camino)*
→ raylib abre una ventana, os da un contexto gráfico de OpenGL, y dibuja quads de colores dada una cámara. No tiene ni idea de qué es un cubo de Rubik. La disposición de la rejilla, la lógica de selección de cara, la animación y el easing, el paso de aplicar al estado lógico, y cada línea del solver real son enteramente vuestras. (Si fuisteis por la vía de MLX en su lugar: "también escribimos el rasterizador entero nosotros mismos" — una respuesta aún más fuerte.)
