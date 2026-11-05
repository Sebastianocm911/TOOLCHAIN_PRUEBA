# Aplicaciones LECTOR y NET

## Registro e integracion

`apps/lector.c` y `apps/net.c` se compilan automaticamente porque el Makefile
construye `$(wildcard apps/*.c)`. `limine.conf` registra sus ELF como modulos; la
ventana Aplicaciones muestra los modulos disponibles mediante `elf_count()`.

Desde la shell:

- `LECTOR /ruta/archivo.txt` abre un documento de texto en el lector.
- `LECTOR` abre la ayuda del lector.
- `NET` abre las herramientas graficas de red.
- `NET PING 10.0.2.2` conserva el alias de diagnostico de consola.
- `RED`, `RED ARP`, `RED PING <ip>`, `RED DNS <nombre>` y `RED INTERNET`
  siguen siendo los comandos de diagnostico del kernel.

## Alcance real de LECTOR en V3.1

El lector inicial abre texto UTF-8 (`.txt` y `.md`) mediante `host_api.read_file`.
No interpreta documentos `.docx` nativamente. El FS actual usa `FS_FILE_MAX_SIZE`
= 1024, expone contenido como `char *` y guarda texto; un `.docx` es un paquete
ZIP de partes XML y normalmente usa compresion DEFLATE. Soporte DOCX real exige,
como minimo, una API de lectura binaria, una estrategia de almacenamiento para
archivos mayores de 1023 bytes, un lector ZIP/DEFLATE y extraccion segura de
`word/document.xml` antes de interpretar sus parrafos y tablas OOXML.

## Alcance real de NET en V3.1

La app es un panel grafico que invoca los servicios ya implementados por el
kernel. El driver `src/net.c` implementa RTL8139, Ethernet, ARP, IPv4/ICMP ping y
UDP/DNS con parametros de red de usuario QEMU (`10.0.2.15/24`, puerta
`10.0.2.2`, DNS `10.0.2.3`). No hay TCP, DHCP, TLS ni HTTP; por tanto esta
version es un comprobador de conectividad, no un navegador web. Un navegador
requiere completar TCP primero y luego HTTP (y HTTPS requiere TLS).

## Validacion

```bash
make apps
make iso
make test-gui
```

En QEMU, ejecutar `ELF` para comprobar los cuatro modulos, despues:

```text
LECTOR /etc/motd
NET
RED PING 10.0.2.2
RED DNS example.com
```

La red QEMU ya se agrega en `scripts/qemu_config.py` con
`-netdev user,id=net0 -device rtl8139,netdev=net0`.
