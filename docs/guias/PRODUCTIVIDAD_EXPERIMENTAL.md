# Suite de productividad y navegador ToolChainOS

La rama incorpora aplicaciones ELF nativas para el escritorio. Se compilan junto
al resto de apps/*.c y se registran como módulos de Limine.

## Aplicaciones

- **Browser** (browser.elf): barra URL, búsqueda Google, desplazamiento y
  descarga real de páginas HTTPS. El kernel transmite la solicitud por TC/1/COM2
  y scripts/ai_bridge.py usa HTTPS con validación de certificados en el anfitrión.
  La respuesta se convierte a texto plano y se limita a una trama de 1 KiB.
- **Writer** (writer.elf): editor de documentos de texto UTF-8, con cursor,
  navegación, abrir el archivo con el que se lanzó y guardar con Ctrl+S en el FS.
- **Calc** (calc.elf): hoja de 8 × 6 celdas, lectura/escritura CSV y fórmulas
  básicas =A1+B2, =A1-B2 y =SUM(A1:B3).
- **Slides** (slides.elf): editor de cinco diapositivas de texto, con título y
  contenido por diapositiva; guarda en /presentacion.tcs.

## Navegación HTTPS

La pila de red del kernel sigue implementando RTL8139, Ethernet, ARP, ICMP y
DNS; todavía no implementa TCP/TLS dentro del propio kernel. Para que Browser
pueda abrir páginas sin fingir capacidades, se añadió un servicio web al puente
COM2 que ya se usa para la IA.

Desde Linux/WSL, en la raíz del repositorio:

~~~sh
make iso
python3 scripts/run_vm.py --web
~~~

Ese lanzador conecta COM2 a ai_bridge.py sin exigir Ollama. En el escritorio
abre browser.elf, escribe una URL (por ejemplo https://example.com) o una
búsqueda, y pulsa Enter. También se puede usar make run-web.

El puente solo acepta HTTPS público por el puerto 443, bloquea destinos locales
y privados, valida certificados TLS, sigue redirecciones HTTPS seguras y acepta
HTML/texto. No ejecuta JavaScript, CSS, extensiones, cookies, audio, vídeo ni
descarga imágenes. Por ahora muestra un extracto de texto de hasta 1 KiB; no es
Firefox/Chromium ni un navegador visual completo.

## Alcance ofimático

Writer guarda texto UTF-8 y Calc guarda CSV. Slides guarda un formato de texto
propio .tcs. El FS de esta versión tiene 64 nodos y un máximo de 1023 bytes
útiles por archivo. No se declara compatibilidad con DOCX, ODT, XLSX, ODS,
PPTX ni PDF. LibreOffice y ONLYOFFICE son suites de gran tamaño que dependen de
bibliotecas, formatos, servicios y subsistemas que ToolChainOS todavía no posee;
no se pueden copiar directamente a este kernel freestanding.

## Compilación y pruebas

~~~sh
make apps
make iso
make test-web-bridge
make run-web
~~~
