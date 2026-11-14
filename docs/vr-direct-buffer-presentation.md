# Direct GPU pair presentation / Presentación directa de la pareja GPU

## English

The new `record_same_state_buffer_copy` helper copies two tightly packed RGBA eye planes from one compute-output buffer directly into left/right rectangles of one target image. It removes the two intermediate eye images and their extra image-to-image copies from this path. It does not create or acquire an OpenXR swapchain itself.

Before recording any command, admission checks a nonzero single-pair state/origin/pose, the current recording, matching device and generation, bounded extents and buffer size, same ordinary queue family, matching supported RGBA format and an explicitly acquired/waited, nonaliasing target. The source owner must prove and retain allocation/lease lifetime and prepare the correct color transfer function. Copying raw texels does not perform gamma conversion.

The helper orders prior buffer writes, discards only the acquired target's old contents, copies both eye regions in one `vkCmdCopyBufferToImage` call, and transitions the target to `COLOR_ATTACHMENT_OPTIMAL`. It does not transfer queue ownership, wait a fence or release the XR image. Those remain the session/resource owner's responsibility, including retaining everything on uncertain GPU completion.

### Verification

- The game-free contract test verifies exact ranges, both regions, barriers/final layout and rejection before commands for invalid identities, memory extents and acquisition state.
- The actual RTX 2060 SUPER oracle checks 32,768 pixels over four reused submissions: two packed source formats, independent plane parallax, rotated/translated views and complete invalid-depth rejection. The full source stays bit-exact. The direct route does not allocate intermediate eye images.
- A private frozen 1152×648 game packet produces a PPM **bit-identical** to the previous image-mediated route with the same shader parameters. The packet is initially uploaded and the result read back for inspection; this is not live game/OpenXR delivery.
- All 111 VR CTest cases pass. This phase changes standalone VR components only; the native renderer remains the already checked live-pair checkpoint.

```powershell
work/vr-build/hookshot_vr_depth_pair_smoke.exe --run-direct
work/vr-build/hookshot_vr_depth_pair_capture.exe --run-direct --capture work/<private-frame>.hsvrd --output work/validation/<fresh-pair>.ppm
```

Next: connect a reusable native reconstruction owner and the checked late-pose math to current OpenXR located views, copy to a real acquired/waited swapchain, balance release/end-frame on all paths and keep menu/HUD composition separate. Headset validation is still pending. No XR session was opened for these checks, and no game packet/image is published.

## Castellano

El nuevo helper copia directamente las dos regiones RGBA de un buffer de cómputo a los rectángulos izquierdo/derecho de una imagen. Así elimina las dos imágenes intermedias de los ojos y las copias adicionales entre imágenes. No crea ni adquiere por sí solo un swapchain OpenXR.

Antes de grabar comandos comprueba estado/pose/origen común, grabación actual, dispositivo y generación, límites de memoria/tamaño, misma cola ordinaria, formato y destino adquirido/esperado sin alias. Ordena las escrituras anteriores, copia ambos ojos en una llamada y deja la imagen en el layout de presentación esperado. El propietario conserva referencias, espera la finalización y gestiona liberación/cierre; si no conoce la finalización GPU debe retener recursos. La copia no corrige gamma ni transfiere colas.

Pruebas reales RTX 2060 SUPER: 32.768 píxeles en cuatro envíos reutilizados, dos formatos, paralaje contrastado independientemente, rotación/traslación y profundidad inválida. Fuente completa intacta. Una captura privada real de 1152×648 produce un PPM idéntico byte por byte a la ruta anterior con iguales parámetros. Ese test sube un archivo y lee la salida; no es presentación del juego vivo en OpenXR.

Pasan 111 pruebas VR. Esta fase solo cambia componentes de prueba/presentación VR; el renderer nativo conserva el checkpoint ya comprobado. Falta unir propietario reutilizable, poses actuales de OpenXR, swapchain real y composición de menús/HUD. No se ha abierto el visor ni publicado imágenes o archivos del juego.


The [projection metadata builder](vr-gpu-projection-metadata.md) separately checks that a future OpenXR layer uses the GPU pair's production poses/FOV and complete eye rectangles. Castellano: comprobado por separado que la capa futura conserve poses/FOV de producción y rectángulos completos.
