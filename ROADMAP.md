# Roadmap — IMG Studio

## Version 1.0.0 (Completada)

- Motor binario GTA San Andreas IMG VER2 independiente en C++20 con lectura streaming y aritmetica verificada.
- Validacion estructural y de legibilidad de payloads con clasificacion por severidad (`Error`, `Warning`, `Info`) y exportacion JSON.
- Explorador de archivos con tabla virtualizada, ordenacion multi-columna, busqueda instantanea, filtros por tipo/extension, seleccion multiple y atajos de teclado.
- Operaciones completas sobre entradas: importar (con politicas explicitas de duplicados y arrastrar/soltar desde Windows Explorer), exportar seleccion o todo con proteccion de rutas, reemplazar, renombrar y eliminar.
- Reconstruccion segura por streaming a archivo temporal con verificacion SHA-256 y reemplazo atomico Win32.
- Respaldos automaticos pre-commit y manuales, historial JSON y restauracion segura verificada.
- Historial Undo/Redo para operaciones pendientes en memoria.
- Interfaz moderna oscura con fuentes tipograficas y cromado nativo oscuro de Windows 10/11.
- Suite de pruebas Catch2 ejecutada bajo Linux ASan/UBSan y Windows x64, mas paquete distribuible ZIP.

## Lineas futuras potenciales (Post-1.0)

- Previsualizacion integrada de texturas `.txd` y geometria `.dff`.
- Soporte de lectura opcional para archivos IMG VER1 (`.dir` + `.img`) de GTA III / Vice City con conversion controlada a VER2.
- Comparacion lado a lado entre dos archivos `.img`.
- Asociacion opcional de archivos `.img` en el instalador de Windows.
