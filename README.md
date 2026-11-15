# BlazMidiCurve

Plugin MIDI LV2 que modifica exclusivamente la velocidad de los eventos **NOTE ON** mediante una curva editable por el usuario.

No procesa audio. No genera audio. No modifica ningún otro mensaje MIDI.

Extremadamente ligero: O(1) por NOTE ON, 0% CPU en reposo, 0 muestras de latencia.

**Nota del autor:** Aviso de que este proyecto ha sido creado en su mayor parte mediante *vibecoding* (programación asistida por IA). La calidad del código puede no ser óptima. Mientras que los programas sean pequeños e independientes, no lo considero mala práctica.
Este programa es fruto de una necesidad personal. Trataré de pasarme por el repositorio cada cierto tiempo. Siéntanse libres de usarlo, forkearlo o lo que quieran.

## Compatibilidad

Funciona en cualquier DAW que soporte LV2:

- Ardour
- Carla
- Reaper (Linux)
- Qtractor
- MusE
- Bitwig
- LMMS (vía Carla)

## Compilación

### Dependencias

```bash
sudo apt install lv2-dev libx11-dev libcairo2-dev cmake g++ pkg-config
```

### Compilar e instalar

```bash
git clone https://github.com/kidandromeda/blazmidicurve.git
cd blazmidicurve
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
make install-user
```

El bundle se instala en:

```text
~/.lv2/blazmidicurve.lv2/
├── manifest.ttl
├── LICENSE
└── midicurve_lv2.so
```

## Puertos

| # | Puerto       | Dirección | Tipo          | Descripción        |
|---|--------------|-----------|---------------|--------------------|
| 0 | `midi_in`    | Entrada   | atom:Sequence | MIDI de entrada    |
| 1 | `midi_out`   | Salida    | atom:Sequence | MIDI procesado     |
| 2 | `ui_notify`  | Salida    | atom:Sequence | Notificaciones UI  |
| 3 | `ui_request` | Entrada   | atom:Sequence | Peticiones de UI   |

## Uso

1. Cargar **BlazMidiCurve** como inserto MIDI en una pista.
2. Editar la curva en la interfaz:
   - **Clic**: seleccionar nodo
   - **Arrastrar**: mover nodo
   - **Doble clic**: crear nodo
   - **Doble clic sobre nodo**: eliminar nodo
3. Botones disponibles:
   - **Reset**: curva identidad (0→0, 127→127)
   - **Save**: guardar preset
   - **Load**: cargar preset

## Licencia

[MIT](LICENSE)
