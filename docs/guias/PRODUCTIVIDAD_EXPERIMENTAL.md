# Suite de productividad experimental

Esta rama agrega tres aplicaciones ELF que se compilan automáticamente porque el
Makefile utiliza `wildcard apps/*.c`:

- **Writer** (`writer.elf`): editor ligero de texto plano. Escribe en la ventana
  y pulsa Ctrl+S para guardar en `/nota.txt`. Está sujeto al límite de tamaño del FS.
- **Calc** (`calc.elf`): calculadora de una lista numérica separada por comas.
  Pulsa Enter para obtener cantidad, suma y promedio entero.
- **Browser** (`browser.elf`): pantalla experimental que informa el estado real
  de la red. No descarga páginas todavía.

## Límites explícitos

No se anuncia compatibilidad DOCX, ODT, XLSX u ODS. El sistema de archivos expuesto
por la ABI actual es textual y tiene un límite pequeño por archivo. Tampoco se
anuncia navegación web funcional: el kernel tiene funciones Ethernet/ARP/ICMP/DNS,
pero todavía no expone sockets TCP ni un cliente HTTP. La siguiente fase debe
implementar transporte TCP, sockets y HTTP antes de convertir Browser en navegador.

## Compilación y ejecución

Desde la raíz del proyecto:

```sh
make apps
make iso
make run
```

Las aplicaciones se incluyen automáticamente en `build/toolchain-os.iso`.
