Trabalho de Sistemas Multimidia (CMP 1103)

servidor.c e cliente.c são a parte de vídeo com interface (relatório em RELATORIO_VIDEO.md).
transmissor.c e receptor.c são a parte de áudio (ver README_AUDIO.md).

Para rodar no Windows precisa do MSYS2. No terminal MSYS2 UCRT64, instalar:

    p=mingw-w64-ucrt-x86_64
    pacman -S --needed $p-gcc $p-pkgconf $p-gtk3 $p-gstreamer $p-gst-plugins-base $p-gst-plugins-good $p-gst-plugins-bad $p-gst-plugins-ugly $p-gst-libav $p-gst-plugin-gtk

Compilar (PowerShell):

    powershell -ExecutionPolicy Bypass -File .\compilar.ps1

Executar (PowerShell):

    $env:Path = "C:\msys64\ucrt64\bin;$env:Path"
    Start-Process .\cliente.exe
    .\servidor.exe

Clicar em Iniciar no cliente e depois no servidor, usando a mesma porta. No mesmo computador o IP é 127.0.0.1.
Em computadores diferentes, colocar o IP do cliente e liberar a porta UDP no firewall (não testamos).
