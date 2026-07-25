# BlazMidiCurve

Clon funcional de **midiCurve.dll** de Piz Tools para Linux.

Formato: **LV2** (arquitectura preparada para VST3).

## Descripción

Plugin MIDI que modifica exclusivamente la velocidad de los eventos **NOTE ON** mediante una curva editable por el usuario.

No procesa audio. No genera audio. No modifica ningún otro mensaje MIDI.

Extremadamente ligero: O(1) por NOTE ON, 0% CPU en reposo, 0 muestras de latencia.

## Compatibilidad

Funciona en cualquier DAW que soporte LV2:

- Ardour
- Carla
- Reaper Linux
- Qtractor
- MusE
- Bitwig
- LMMS (vía Carla)

## Compilación

```bash
# Dependencias
sudo apt install lv2-dev libx11-dev libcairo2-dev cmake g++

# Compilar
cd midi-curve-lv2
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)

# Tests
ctest

# Instalar para el usuario actual
make install-user
```

## Bundle

```
~/.lv2/blazmidicurve.lv2/
├── manifest.ttl       # Descripción del plugin y UI
├── LICENSE            # MIT
└── midicurve_lv2.so   # Plugin y UI (mismo binario)
```

## Puertos

| # | Puerto     | Dirección | Tipo           | Descripción        |
|---|------------|-----------|----------------|--------------------|
| 0 | `midi_in`  | Entrada   | atom:Sequence  | MIDI + control     |
| 1 | `midi_out` | Salida    | atom:Sequence  | MIDI procesado     |
| 2 | `state`    | Salida    | atom:Sequence  | Estado de la curva |

## Uso

1. Cargar BlazMidiCurve como inserto MIDI en un track
2. Editar la curva en la UI:
   - **Click**: seleccionar nodo
   - **Drag**: mover nodo
   - **Doble click**: crear nodo
   - **Doble click sobre nodo**: eliminar nodo
3. Botones de preset:
   - **Reset**: curva identidad (0→0, 127→127)
   - **Invert**: curva invertida (0→127, 127→0)
   - **Smooth**: suavizado por media móvil
   - **Linear**: identidad
   - **Exp**: curva exponencial
   - **Log**: curva logarítmica
   - **Random**: curva aleatoria

## Estado del proyecto

MVP funcional. Pendiente:
- [ ] Pruebas en hosts reales (Ardour, Carla, etc.)
- [ ] Generación VST3
- [ ] Más opciones de preset
- [ ] Optimización de renderizado UI

## Licencia

MIT
