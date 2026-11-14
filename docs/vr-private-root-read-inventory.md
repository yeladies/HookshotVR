# Read-only global shader inventory

## English

Some game programs read the camera through global memory. Replacing a uniform descriptor is insufficient for those reads. [Private GPU roots](vr-gpu-root-aliases.md) supply a bounded camera-alias diagnostic; this separate inventory identifies shader effects that still need routing and field review. It does not generate a draw-selection file or permit scene replay.

The backend emits global-load and global-store helper functions together. A whole-module scan therefore reports stores even when a particular shader never calls the store helper. The private-root inventory builds the entry function's transitive call graph, checks every block of every reached function, and separately reports ignored functions. Reached stores to physical/storage memory, atomics, image writes, unsupported pointer conversions and unresolved calls remain rejected. Tests cover indirect call chains, cycles, called/uncalled writers and unresolved callees. The ordinary baseline audit retains its original whole-module policy.

A dedicated physical-read inventory can recognize `PhysicalStorageBuffer` loads and integer-to-physical-pointer conversions. It still reports `supported_read_only_subset=false`, `safe_to_replay=false`, `physical_read_ranges_proved=false` and `private_root_routing_verified=false`. Recognizing the effect does not prove where an address came from or that its camera route is private. This is not a SPIR-V validator.

The native capture now exports `stores_global_memory` separately from `uses_global_memory`. A candidate needs an explicit false write flag for every stage, no descriptor writes, complete vertex/fragment modules and matching read-only effect evidence. Missing flags in older captures stay unverified. Boolean types are checked; numeric zero is not accepted as a false flag.

### Local observation

A fresh associated game capture contained 64 program inventories and 95 referenced unique modules. The independent native metadata and entry-call-graph inventory agreed on 23 vertex/fragment programs with global reads and no reached external writes. No program lacked the new global-write metadata. The root-camera diagnostic still passed; the process peaked at 6625 MiB. The 23 are effect-inventory candidates, not drawable scene coverage.

All observed global camera candidates also read preserved fields outside the current matrix patch, including additional blocks around byte offsets 560–660, 1024 and 2368–2416. Their semantic meaning remains unproved. Four previously reviewed projection/viewport scalar fields are insufficient to declare the entire block eye-correct. Unknown data remains byte-exact. Neither global game-shader root binding nor complete shaded stereo is enabled by this report.

```powershell
python -I -B vr/audit_private_root_reads.py <private-capture-directory> --grammar <spirv.core.grammar.json> --output work/validation/root-read-inventory.json
```

Use an immutable associated private capture. Generated shader modules, raw observations, captures and reports stay local under ignored work paths. Only the authored inventory, tests and aggregate technical documentation belong in GitHub.

### Next integration gates

1. Completed in the bounded depth diagnostic: consume GPU-guarded arguments with exact unchanged controls and source restoration. This does not establish global-shader routing.
2. Review the extra camera fields and every descriptor/global alias before a global-reading game draw consumes private roots.
3. Build complete per-eye producer/consumer passes, reuse bounded frame resources, then connect GPU-only image delivery and separate UI composition to OpenXR.

A full root pair belongs to a bounded frame/resource lifetime. It must not be newly allocated for every draw of a complete scene. Physical scale, IPD and headset performance remain separate measurements.

## Castellano

Algunos programas del juego acceden a la cámara por memoria global. Cambiar únicamente un descriptor de uniforme no cubre esos accesos. El inventario nuevo analiza sus efectos, pero no genera selección de dibujos ni autoriza repetir la escena.

El backend incluye juntos auxiliares de lectura y escritura. Por eso una inspección de todo el módulo detectaba escrituras que determinados shaders nunca ejecutan. El análisis específico sigue las llamadas desde la entrada y revisa todos los bloques de cada función alcanzada. Siguen rechazándose escrituras compartidas, atomics, escrituras de imágenes, conversiones de puntero no admitidas y llamadas sin resolver. Hay pruebas de cadenas, ciclos y auxiliares llamados o sin llamar. La auditoría original conserva su criterio más restrictivo.

El motor exporta ahora por separado si usa y si escribe memoria global. Cada etapa necesita una marca explícita de no escritura, descriptores sin escrituras y coincidencia con el análisis del shader. Las capturas antiguas sin esta información quedan sin verificar. Reconocer una lectura física no demuestra su origen, rango ni redirección privada; el informe mantiene desactivados replay y preparación de dibujos.

Una captura real nueva incluyó 64 programas y 95 módulos únicos referenciados. Coinciden metadatos y análisis en 23 programas de vértices/fragmentos con lecturas globales y sin escrituras externas alcanzables. Ninguno carecía de la marca nueva. La prueba de raíces siguió pasando; pico de proceso 6625 MiB. No equivale a 23 dibujos ya habilitados ni a cobertura de escena.

Esos programas también leen campos adicionales de cámara, alrededor de 560–660, 1024 y 2368–2416 bytes. Su significado aún necesita revisión; los cuatro escalares conocidos no resuelven todo el bloque. Se mantienen los bytes desconocidos intactos y todavía no se conectan raíces privadas a esos shaders.

El siguiente orden es comprobar dibujos privados mediante argumentos protegidos, revisar campos y alias, completar productores/consumidores de ambos ojos con recursos reutilizables y conectar entrega GPU e interfaz separada a OpenXR. No se debe reservar una pareja completa de tablas por cada dibujo de la escena. Escala física, IPD y rendimiento en visor siguen pendientes. No hace falta intervención del usuario para estas pruebas internas.
