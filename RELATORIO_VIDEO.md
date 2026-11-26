# Diagnóstico e evolução de um sistema de streaming multimídia

Sistemas Multimídia (CMP 1103) — Prof. Esp. Thalles Bruno Santos

## Ambiente e método

| Item | Valor |
|---|---|
| Sistema | Windows 11 Home |
| Hardware | AMD Ryzen 5 5500 (12 processadores lógicos), 15,8 GB de RAM, Radeon RX 6650 XT |
| Software | GStreamer 1.28.9 e GTK 3.24.52 (MSYS2 UCRT64), VLC 3.0.24 |
| Rede | Somente localhost (127.0.0.1) |

As medições são automatizadas pelo script [`experimentos.ps1`](experimentos.ps1), que executa
`gst-launch-1.0 -v`. Os dados brutos estão em [`resultados/video/`](resultados/video/).

- **FPS:** média (`average`) informada pelo `fpsdisplaysink`; cada execução dura cerca de 20 s.
  A média inclui a partida da pipeline.
- **Descartados:** contador `dropped` do `fpsdisplaysink`. São quadros descartados pelo sink,
  não pacotes perdidos na rede.
- **CPU:** tempo de processador do processo, do 3º segundo ao fim, em porcentagem do total dos
  12 processadores lógicos. A escala é a do Gerenciador de Tarefas, mas a leitura foi feita por
  script. Não inclui o uso de GPU nem o de outros processos (como o compositor de janelas).
- **Fluidez:** avaliada pelas métricas acima. **Não foi feita observação visual contínua** das
  animações dos experimentos.
- Cada linha de FPS e CPU vem de **uma única execução**. Diferenças pequenas não são conclusivas.

**Sobre a "pipeline original".** O código-base deste repositório transmite **áudio** em RTP/UDP
(ver [README_AUDIO.md](README_AUDIO.md)) e não contém uma pipeline de vídeo MPEG-TS/UDP.
Por isso, adotamos como original a versão mínima abaixo, sem as propriedades do item 1.3.
Os resultados da comparação valem para esta referência.

```
videotestsrc is-live=true pattern=ball ! video/x-raw,width=640,height=360,framerate=25/1
  ! videoconvert ! video/x-raw,format=I420 ! x264enc bitrate=500 speed-preset=ultrafast
  ! h264parse ! mpegtsmux ! udpsink host=127.0.0.1 port=5000
```

## Parte 1.1 — Reprodução local sem compressão

| Variável alterada | Resolução | FPS configurado | FPS medido | Descartados | CPU (%) |
|---|---|---|---|---|---|
| referência | 640×360 | 25 | 24,88 | 1 | 0,6 |
| resolução | 320×240 | 25 | 24,93 | 1 | 0,3 |
| resolução | 1280×720 | 25 | 24,87 | 1 | 1,8 |
| taxa de quadros | 640×360 | 15 | 15,04 | 0 | 0,4 |
| taxa de quadros | 640×360 | 30 | 29,87 | 1 | 0,6 |

O FPS medido acompanhou o configurado em todos os casos, com no máximo um quadro descartado
por execução. A resolução maior aumentou o uso de CPU, sem afetar a taxa de quadros.

## Parte 1.2 — Com codificação e decodificação H.264

Todos os testes usam `tune=zerolatency`. Referência: 640×360, 25 FPS, 500 kbps, `ultrafast`.

| Variável alterada | Valor | FPS medido | Descartados | CPU (%) |
|---|---|---|---|---|
| referência | — | 24,73 | 1 | 1,2 |
| bitrate | 200 kbps | 24,78 | 1 | 1,3 |
| bitrate | 1500 kbps | 24,78 | 1 | 1,2 |
| speed-preset | fast | 24,78 | 1 | 1,9 |
| speed-preset | medium | 24,73 | 1 | 1,8 |
| framerate | 15 | 15,04 | 0 | 0,8 |
| framerate | 30 | 29,67 | 1 | 1,8 |
| resolução | 320×240 | 24,78 | 1 | 0,4 |
| resolução | 1280×720 | 24,72 | 1 | 5,8 |

- **Custo da compressão:** na referência, a CPU passou de 0,6% para 1,2%, sem queda de FPS.
- **Resolução:** foi a variável de maior impacto na CPU (5,8% em 1280×720).
- **Preset:** `fast` e `medium` usaram mais CPU que `ultrafast`. A diferença entre `fast` e
  `medium` é pequena demais para ser interpretada com uma execução de cada.
- **Bitrate:** não houve efeito mensurável na CPU nem no FPS. O padrão `ball` é uma bola sobre
  fundo preto, muito fácil de comprimir; com vídeo real o resultado pode ser diferente.
- A qualidade de imagem não foi comparada entre os bitrates e presets.

## Parte 1.3 — Transmissão MPEG-TS/UDP

### Finalidade das propriedades

| Elemento e propriedade | Finalidade |
|---|---|
| `x264enc key-int-max=25` | Intervalo máximo entre quadros-chave: 25 quadros, ou 1 s a 25 FPS. O padrão do `x264enc` (0, automático) resulta em intervalos longos. Quem entra no meio da transmissão precisa esperar um quadro-chave. |
| `x264enc tune=zerolatency` | Desliga recursos que fazem o codificador reter quadros antes de produzir a saída (como *lookahead* e quadros B). |
| `h264parse config-interval=-1` | Reenvia SPS e PPS (configuração necessária ao decodificador) junto de cada quadro-chave. |
| `mpegtsmux alignment=7` | Agrupa 7 pacotes TS de 188 bytes por buffer, ou 1316 bytes, que cabem em um datagrama com MTU de 1500. |
| `udpsink sync=false` | Envia cada buffer assim que fica pronto, sem esperar o relógio da pipeline. Quem dita o ritmo é a fonte ao vivo (`is-live=true`). |

### FPS e CPU em localhost

Receptor: `udpsrc ! queue ! tsdemux ! h264parse ! avdec_h264 ! videoconvert ! fpsdisplaysink`.
Também alteramos a propriedade `sync` do sink do **receptor**.

| Transmissor | `sync` no receptor | FPS medido | Descartados | CPU transm. (%) | CPU recept. (%) |
|---|---|---|---|---|---|
| original | true | 24,01 | 0 | 0,4 | 0,2 |
| original | false | 25,06 | 0 | 0,4 | 0,2 |
| modificada | true | 24,13 | 0 | 0,5 | 0,3 |
| modificada | false | 25,06 | 0 | 0,4 | 0,2 |

Em regime contínuo, as duas pipelines entregaram a taxa configurada e nenhum quadro foi
descartado. A média um pouco menor com `sync=true` inclui a partida; este teste não permite
afirmar que há perda de fluidez.

Os valores de CPU são menores que os da Parte 1.2 mesmo com mais elementos. Não investigamos
a causa, por isso eles não devem ser comparados diretamente entre as duas partes.

### Tempo até o primeiro quadro

A diferença entre as pipelines apareceu **no início da reprodução**. Medimos o tempo até o
primeiro quadro decodificado, com 8 repetições por caso. O valor inclui a abertura do processo
`gst-launch-1.0` e a consulta ao log a cada 50 ms. Não é a latência fim a fim em regime contínuo.

- **Receptor antes:** receptor já aberto; tempo contado a partir do início do transmissor.
- **Receptor atrasado:** transmissor já rodando; tempo contado a partir do início do receptor,
  que entra em um momento sorteado.

Mediana em segundos (mínimo – máximo). Cada linha `+` altera **uma única** propriedade da original.
Com 8 amostras, a mediana informada é a superior das duas centrais.

| Transmissor | Receptor antes | Receptor atrasado |
|---|---|---|
| original | 0,64 (0,63 – 0,68) | **9,16** (5,63 – 9,57) |
| + `key-int-max=25` | 0,66 (0,63 – 0,68) | **1,07** (0,32 – 1,20) |
| + `tune=zerolatency` | **0,19** (0,19 – 0,22) | 8,79 (5,62 – 9,93) |
| + `config-interval=-1` | 0,63 (0,63 – 0,67) | 8,37 (5,19 – 9,88) |
| + `alignment=7` | 0,71 (0,66 – 0,77) | 8,32 (7,07 – 9,94) |
| + `sync=false` | 0,59 (0,57 – 0,63) | 9,55 (5,34 – 9,89) |
| modificada (todas) | **0,27** (0,25 – 0,28) | **0,82** (0,44 – 1,20) |

Em todos os casos houve vídeo nas 8 repetições.

- **`key-int-max=25`** explica quase todo o ganho de quem entra atrasado: a espera caiu de até
  cerca de 10 s para até 1,2 s. Isso é compatível com um quadro-chave a cada 250 quadros
  (10 s a 25 FPS) na original.
- **`tune=zerolatency`** reduziu a partida com o receptor já aberto de 0,64 s para 0,19 s.
- **`config-interval=-1`, `alignment=7` e `sync=false`** não mostraram efeito que se destaque da
  variação das medidas, neste teste local. Isso não prova que sejam inúteis em outras redes ou
  com outros receptores.

### VLC como receptor

O VLC foi executado sem janela, abrindo `udp://@:5000` por 15 s, com o transmissor iniciado
3 s antes. O filtro `scene` salvou uma imagem a cada 25 quadros. Foi feita uma execução por pipeline.

| Transmissor | Imagens salvas em 15 s |
|---|---|
| original | 9 |
| modificada | 15 |

O VLC reproduziu os dois fluxos. O menor número de imagens na original é compatível com a
espera pelo primeiro quadro-chave. A reprodução pela interface do VLC, com o cache de rede
padrão, não foi avaliada visualmente.

![Quadro recebido pelo VLC](docs/vlc_quadro_recebido.png)

### Perda simulada (`netsim`)

Teste extra com o mesmo código das interfaces, receptor em janela separada, uma execução de
10 s por caso.

| Perda configurada | Quadros codificados | Datagramas recebidos | Quadros decodificados |
|---|---|---|---|
| 0% | 266 | 201 | 263 |
| 5% | 249 | 182 | 211 |

Com 5% de perda de buffers, deixaram de ser decodificados cerca de 15% dos quadros (38 de 249),
contra cerca de 1% (3 de 266) sem perda. MPEG-TS/UDP não tem retransmissão nem reordenação.
Os defeitos visuais não foram avaliados.

Há menos datagramas que quadros. Uma hipótese é que, com `alignment=7`, quadros pequenos
dividam o mesmo datagrama. Não medimos se isso gera entrega em rajadas.

## Parte 1.4 — Registro dos resultados

| Experimento | FPS | CPU | Fluidez / observações |
|---|---|---|---|
| Local sem compressão | 24,88 | 0,6% | Taxa mantida; 1 quadro descartado |
| Local com H.264 | 24,73 | 1,2% | Taxa mantida; 1 quadro descartado |
| UDP via localhost (original, `sync=false` no receptor) | 25,06 | 0,4% transm. / 0,2% recept. | Taxa mantida; quem entra atrasado fica até cerca de 10 s sem vídeo |
| UDP entre dispositivos* | indisponível | indisponível | **Não realizado** |
| Pipeline modificada (`sync=false` no receptor) | 25,06 | 0,4% transm. / 0,2% recept. | Taxa mantida; quem entra atrasado vê vídeo em até 1,2 s |

\* Opcional segundo o enunciado.

### Em qual etapa ocorreu a degradação

| Sintoma | O que observamos |
|---|---|
| **Baixa fluidez** | Não reproduzida nesta máquina: nenhum cenário sem perda simulada ficou abaixo da taxa configurada de forma relevante. |
| **Atraso para começar** | Na codificação: sem `zerolatency`, a partida subiu de cerca de 0,2 s para 0,6 s. |
| **Ausência temporária de vídeo** | Na entrada do receptor no fluxo: sem quadros-chave frequentes, a tela ficou sem imagem por até cerca de 10 s. Pode parecer que a transmissão não funciona. |

**O que melhorou:** `key-int-max=25` (entrada no fluxo) e `tune=zerolatency` (partida).
Não houve ganho mensurável de FPS ou CPU em localhost.

### Hipóteses sem confirmação

- A baixa fluidez da atividade anterior pode ter vindo de um computador com menos CPU, de
  outra fonte de vídeo ou de parâmetros diferentes dos usados aqui.
- A ausência de reprodução entre dispositivos pode envolver firewall, IP de destino errado,
  isolamento entre clientes no Wi-Fi ou perda de datagramas. Nada disso foi testado.
- Receptor incompatível com o transporte (por exemplo, esperar RTP e receber MPEG-TS) também
  impede a reprodução. Não foi reproduzido nos experimentos.
- O comportamento do `sync` e do `alignment` em uma rede real continua sem verificação.

## Parte 2 — Interfaces gráficas

### Organização

| Arquivo | Conteúdo |
|---|---|
| [`midia.c`](midia.c) / [`midia.h`](midia.h) | Criação, configuração, controle e liberação das pipelines |
| [`servidor.c`](servidor.c) | Interface GTK 3 do transmissor |
| [`cliente.c`](cliente.c) | Interface GTK 3 do receptor |

A aplicação não executa `gst-launch-1.0`. A estrutura das pipelines é criada com
`gst_parse_launch_full`, como no código-base. Os valores da interface são aplicados com
`g_object_set()` e caps, sem concatenar o texto digitado na descrição. O bloqueio em
`gst_bus_timed_pop_filtered` do código-base foi substituído por `gst_bus_add_watch()`.

O transmissor usa a pipeline modificada. O receptor usa `sync=false`: nos testes, as duas
opções exibiram o fluxo sem descartes, e escolhemos exibir sem esperar o relógio. A latência
das duas opções em regime contínuo não foi medida.

![Servidor transmitindo](docs/servidor_transmitindo.png)

![Cliente com o vídeo incorporado](docs/cliente_recebendo.png)

### Requisitos

| Requisito | Implementação |
|---|---|
| IP de destino e porta UDP | Campo de texto validado (`g_inet_address_new_from_string`) e `GtkSpinButton` de 1 a 65535 |
| Parâmetros de qualidade | Resolução, FPS, bitrate e preset (além de perda simulada) |
| Iniciar e Interromper | Nas duas interfaces; os controles são habilitados conforme o estado |
| Indicador de estado do servidor | Quadros codificados por segundo e total, além de erros |
| Indicador de estado do cliente | Baseado em datagramas recebidos e quadros decodificados, não apenas em `PLAYING` |
| Porta de recepção | `udpsrc` com `reuse=false`, para que porta ocupada gere erro |
| Liberação de recursos | `midia_destruir()` leva a pipeline a `NULL` ao interromper, em erro ou ao fechar |
| Desafio adicional | Vídeo na janela do cliente com `gtksink`; opção de janela separada com `autovideosink` |

Estados do cliente: aguardando pacotes, pacotes sem quadro decodificado, recebendo vídeo e
vídeo interrompido (2 s sem quadros novos). Como o UDP não avisa o fim da transmissão, o
cliente continua aberto aguardando.

O indicador mede quadros **decodificados**. Os contadores "exibidos" e "descartados" vêm do
`fpsdisplaysink`. O estado "pacotes sem quadro decodificado" está implementado, mas não foi
provocado nos testes.

### Testes realizados

Os cliques foram disparados por um programa de teste temporário, usando os botões reais das
interfaces. As capturas de tela vêm de uma execução com início automático (`--iniciar`).

| Cenário | Resultado |
|---|---|
| IP inválido (`999.1.1.1`) e campo vazio | Recusados com mensagem |
| IP com espaços nas pontas | Aceito após remover os espaços |
| Iniciar → Interromper → Iniciar | Funcionou no servidor e no cliente |
| Porta 5000 ocupada no cliente | Erro exibido; iniciou após liberar a porta |
| Cliente aberto antes do servidor | Estado "Aguardando pacotes" e depois "Recebendo vídeo" |
| Servidor fechado durante a recepção | Cliente indicou "Vídeo interrompido" |
| Vídeo incorporado | Recebido a cerca de 25 quadros/s, sem descartes; imagem conferida em captura de tela |
| Janela separada | Quadros decodificados e exibidos segundo os contadores |

Não testados: transmissão entre dois computadores, execução em Linux e outros valores de
resolução, FPS e preset pela interface.
