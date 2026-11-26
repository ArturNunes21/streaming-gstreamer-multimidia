# Streaming multimídia com GStreamer

Trabalho em grupo de **Sistemas Multimídia (CMP 1103)**.

| Atividade | Código | Documentação |
|---|---|---|
| **Vídeo** H.264 em MPEG-TS/UDP, com interfaces GTK 3 | `servidor.c`, `cliente.c`, `midia.c` | [RELATORIO_VIDEO.md](RELATORIO_VIDEO.md) |
| **Áudio** RTP/UDP em linha de comando (atividade anterior, base deste trabalho) | `transmissor.c`, `receptor.c` | [README_AUDIO.md](README_AUDIO.md), [registro](REGISTRO_EXPERIMENTOS_AUDIO.md), [conclusão](CONCLUSAO_AUDIO.md) |

Baseado no repositório
[ViniciusZoccoli/streaming-gstreamer-audio](https://github.com/ViniciusZoccoli/streaming-gstreamer-audio);
o histórico de commits original foi mantido.

## 1. Dependências (Windows com MSYS2)

1. Instale o [MSYS2](https://www.msys2.org) na pasta padrão `C:\msys64`
   (ou `winget install MSYS2.MSYS2`).
2. No terminal **MSYS2 UCRT64**, atualize com `pacman -Syu` e instale:

   ```bash
   p=mingw-w64-ucrt-x86_64
   pacman -S --needed $p-gcc $p-pkgconf $p-gtk3 $p-gstreamer $p-gst-plugins-base \
     $p-gst-plugins-good $p-gst-plugins-bad $p-gst-plugins-ugly $p-gst-libav $p-gst-plugin-gtk
   ```

| Pacote | Fornece (entre outros) |
|---|---|
| `gst-plugins-ugly` | `x264enc` |
| `gst-libav` | `avdec_h264` |
| `gst-plugin-gtk` | `gtksink` (vídeo dentro da janela) |
| `gst-plugins-bad` | `h264parse`, `mpegtsmux`, `fpsdisplaysink`, `netsim` |

> Se você também tem o instalador oficial do GStreamer (MSVC), não misture os dois: as
> bibliotecas precisam ser compatíveis com o compilador. O `compilar.ps1` já prioriza o MSYS2.

Linux (não testado): instale o GCC, o GTK 3, o GStreamer com os plugins equivalentes e os
pacotes de desenvolvimento.

## 2. Compilação

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
```

Terminal MSYS2 UCRT64 ou Linux (instale o `make` se necessário):

```bash
make
```

## 3. Execução das interfaces

No PowerShell:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
.\cliente.exe
```

Em outro terminal, com o mesmo `$env:Path`:

```powershell
.\servidor.exe
```

1. No cliente, escolha a porta (padrão 5000) e clique em **Iniciar**.
2. No servidor, informe o IP de destino (`127.0.0.1` no mesmo computador) e a mesma porta,
   ajuste a qualidade e clique em **Iniciar**.

A opção `--iniciar` faz qualquer um dos dois começar assim que abre. Para alterar os
parâmetros durante a execução, interrompa e inicie de novo.

**Entre dois computadores (não testado):** no servidor, use o IP do computador do cliente
(`ipconfig`). Os dois devem estar na mesma rede, e o firewall do cliente precisa permitir
UDP na porta escolhida.

## 4. Experimentos da Parte 1

```powershell
powershell -ExecutionPolicy Bypass -File .\experimentos.ps1
```

Leva cerca de 15 minutos, abre várias janelas e **sobrescreve** os CSVs em
`resultados/video/`. Deixe a tela desbloqueada e a porta 5000 livre. O teste com VLC é
ignorado se ele não estiver instalado.

## 5. Pipelines usadas pelas interfaces

```
Servidor:
videotestsrc is-live=true pattern=ball ! video/x-raw,width=W,height=H,framerate=F/1
  ! videoconvert ! video/x-raw,format=I420
  ! x264enc tune=zerolatency key-int-max=F bitrate=B speed-preset=P
  ! h264parse config-interval=-1 ! mpegtsmux alignment=7
  ! [netsim drop-probability=X]            (só com perda simulada)
  ! udpsink host=IP port=PORTA sync=false

Cliente:
udpsrc port=PORTA reuse=false caps="video/mpegts,systemstream=true,packetsize=188"
  ! queue ! tsdemux ! h264parse ! avdec_h264 ! videoconvert
  ! fpsdisplaysink sync=false video-sink=gtksink     (ou autovideosink)
```

O diagrama [`pipelines.drawio`](pipelines.drawio) se refere à atividade de áudio.

## 6. Problemas comuns

| Sintoma | Causa / solução |
|---|---|
| Programa não abre ou faltam DLLs | Rode `$env:Path = "C:\msys64\ucrt64\bin;$env:Path"` no mesmo terminal |
| `no element "x264enc"` / `"avdec_h264"` | Faltam `gst-plugins-ugly` / `gst-libav` |
| Vídeo abre em janela separada | Falta `gst-plugin-gtk`, ou a opção "Video nesta janela" está desmarcada |
| "Erro ao iniciar" no cliente | Porta ocupada (outro cliente ou VLC aberto) |
| "Aguardando pacotes" sem fim | IP ou porta diferentes, firewall, ou servidor parado |

## 7. Trabalho em grupo com Git

```bash
git clone https://github.com/ArturNunes21/streaming-gstreamer-multimidia.git
cd streaming-gstreamer-multimidia
git pull --rebase               # antes de começar e antes de enviar
git add <arquivos alterados>
git commit -m "Descrição da mudança"
git push
```

Confira seu nome e e-mail do Git com `git config user.name` e `git config user.email`.
