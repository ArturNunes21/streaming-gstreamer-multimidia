# Streaming multimídia com GStreamer

Trabalho de Sistemas Multimídia (CMP 1103).

- Vídeo com interface gráfica: `servidor.c` e `cliente.c` ([relatório](RELATORIO_VIDEO.md))
- Áudio (atividade anterior): `transmissor.c` e `receptor.c` ([instruções](README_AUDIO.md))

## Dependências (Windows)

Instale o [MSYS2](https://www.msys2.org) e, no terminal MSYS2 UCRT64, rode:

```bash
p=mingw-w64-ucrt-x86_64
pacman -S --needed $p-gcc $p-pkgconf $p-gtk3 $p-gstreamer $p-gst-plugins-base $p-gst-plugins-good $p-gst-plugins-bad $p-gst-plugins-ugly $p-gst-libav $p-gst-plugin-gtk
```

## Compilar

No PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
```

No terminal MSYS2 UCRT64:

```bash
gcc servidor.c -o servidor $(pkg-config --cflags --libs gtk+-3.0 gstreamer-1.0)
gcc cliente.c -o cliente $(pkg-config --cflags --libs gtk+-3.0 gstreamer-1.0)
```

## Executar

No PowerShell:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
Start-Process .\cliente.exe
.\servidor.exe
```

Clique em Iniciar no cliente e depois no servidor. A porta precisa ser a mesma. No mesmo
computador, use o IP 127.0.0.1. Em computadores diferentes, use o IP do cliente e libere a
porta UDP no firewall (ainda não testado).
