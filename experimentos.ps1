# Experimentos da Parte 1 (video). Abre janelas de video: deixe a tela desbloqueada.
# Uso: powershell -ExecutionPolicy Bypass -File .\experimentos.ps1
# Gera os CSVs em resultados\video\ (separador ';').
param(
    [string]$Bin = "C:\msys64\ucrt64\bin",
    [string]$Vlc = "C:\Program Files\VideoLAN\VLC\vlc.exe",
    [int]$Segundos = 20,    # duracao de cada medicao de FPS/CPU
    [int]$Repeticoes = 8    # repeticoes das medidas de tempo ate o 1o quadro
)
$ErrorActionPreference = "Stop"
$env:Path = "$Bin;$env:Path"
$gst = "$Bin\gst-launch-1.0.exe"
$saida = Join-Path $PSScriptRoot "resultados\video"
$tmp = Join-Path $env:TEMP "experimentos_gst"
New-Item -ItemType Directory -Force $saida, $tmp | Out-Null
$nucleos = [Environment]::ProcessorCount
$script:seq = 0

function Start-Gst([string]$pipeline) {
    $script:seq++
    $log = Join-Path $tmp "gst_$($script:seq).log"
    $p = Start-Process $gst -ArgumentList "-v $pipeline" -PassThru -NoNewWindow `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $null = $p.Handle
    [pscustomobject]@{ Proc = $p; Log = $log }
}
function Stop-Gst($g) {
    if ($g -and -not $g.Proc.HasExited) { $g.Proc.Kill(); $g.Proc.WaitForExit() }
}
function Read-Log($g) {
    $fs = [IO.File]::Open($g.Log, 'Open', 'Read', 'ReadWrite')
    try { (New-Object IO.StreamReader $fs).ReadToEnd() } finally { $fs.Dispose() }
}
# Ultima linha de estatisticas publicada pelo fpsdisplaysink.
function Get-Fps($g) {
    $m = [regex]::Matches((Read-Log $g),
        'last-message = rendered: (\d+), dropped: (\d+), (?:current: [\d.]+, average: ([\d.]+)|fps: [\d.]+, drop rate: [\d.]+)')
    $media = $m | Where-Object { $_.Groups[3].Success } | Select-Object -Last 1
    if (-not $media) { return [pscustomobject]@{ Fps = "indisponivel"; Render = 0; Desc = 0 } }
    $ultimo = $m[$m.Count - 1]
    [pscustomobject]@{ Fps = $media.Groups[3].Value; Render = [int]$ultimo.Groups[1].Value
                       Desc = [int]$ultimo.Groups[2].Value }
}
# CPU media do processo entre 3 s e o fim, em % do total da maquina
# (mesma escala do Gerenciador de Tarefas).
function Measure-Cpu($lista, [int]$duracao) {
    Start-Sleep -Seconds 3
    $ini = @($lista | ForEach-Object { $_.Proc.Refresh(); $_.Proc.TotalProcessorTime.TotalSeconds })
    $relogio = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Seconds ($duracao - 3)
    $t = $relogio.Elapsed.TotalSeconds
    for ($i = 0; $i -lt $lista.Count; $i++) {
        $lista[$i].Proc.Refresh()
        [math]::Round(100 * ($lista[$i].Proc.TotalProcessorTime.TotalSeconds - $ini[$i]) / ($t * $nucleos), 1)
    }
}
function Get-Caps($w, $h, $f) { "video/x-raw,width=$w,height=$h,framerate=$f/1" }
$exibir = "fpsdisplaysink text-overlay=true"

# ---------- 1.1 e 1.2: reproducao local ----------
function Test-Local($grupo, $variavel, $w, $h, $f, $bitrate, $preset) {
    $p = "videotestsrc is-live=true pattern=ball ! $(Get-Caps $w $h $f) ! videoconvert ! "
    if ($bitrate) {
        $p += "x264enc tune=zerolatency speed-preset=$preset bitrate=$bitrate ! h264parse ! avdec_h264 ! videoconvert ! "
    }
    $g = Start-Gst ($p + $exibir)
    $cpu = Measure-Cpu @($g) $Segundos
    $r = Get-Fps $g
    Stop-Gst $g
    $linha = [pscustomobject]@{ grupo = $grupo; variavel = $variavel; resolucao = "${w}x$h"
        fps_configurado = $f; bitrate_kbps = $bitrate; preset = $preset
        fps_medido = $r.Fps; renderizados = $r.Render; descartados = $r.Desc; cpu_pct = $cpu }
    Write-Host ($linha | Format-Table -HideTableHeaders | Out-String).Trim()
    $linha
}
Write-Host "== 1.1 / 1.2: local =="
$local = @(
    Test-Local "sem compressao" "referencia" 640 360 25 $null $null
    Test-Local "sem compressao" "resolucao" 320 240 25 $null $null
    Test-Local "sem compressao" "resolucao" 1280 720 25 $null $null
    Test-Local "sem compressao" "framerate" 640 360 15 $null $null
    Test-Local "sem compressao" "framerate" 640 360 30 $null $null
    Test-Local "H.264" "referencia" 640 360 25 500 ultrafast
    Test-Local "H.264" "bitrate" 640 360 25 200 ultrafast
    Test-Local "H.264" "bitrate" 640 360 25 1500 ultrafast
    Test-Local "H.264" "preset" 640 360 25 500 fast
    Test-Local "H.264" "preset" 640 360 25 500 medium
    Test-Local "H.264" "framerate" 640 360 15 500 ultrafast
    Test-Local "H.264" "framerate" 640 360 30 500 ultrafast
    Test-Local "H.264" "resolucao" 320 240 25 500 ultrafast
    Test-Local "H.264" "resolucao" 1280 720 25 500 ultrafast
)
$local | Export-Csv "$saida\local.csv" -NoTypeInformation -Delimiter ';' -Encoding UTF8

# ---------- 1.3: MPEG-TS/UDP em localhost ----------
$fonte = "videotestsrc is-live=true pattern=ball ! $(Get-Caps 640 360 25) ! videoconvert ! video/x-raw,format=I420"
function Get-Transmissor($enc, $parse, $mux, $sink) {
    "$fonte ! x264enc bitrate=500 speed-preset=ultrafast $enc ! h264parse $parse ! mpegtsmux $mux ! udpsink host=127.0.0.1 port=5000 $sink"
}
$variantes = [ordered]@{
    "original"            = Get-Transmissor "" "" "" ""
    "+key-int-max=25"     = Get-Transmissor "key-int-max=25" "" "" ""
    "+tune=zerolatency"   = Get-Transmissor "tune=zerolatency" "" "" ""
    "+config-interval=-1" = Get-Transmissor "" "config-interval=-1" "" ""
    "+alignment=7"        = Get-Transmissor "" "" "alignment=7" ""
    "+sync=false"         = Get-Transmissor "" "" "" "sync=false"
    "modificada"          = Get-Transmissor "key-int-max=25 tune=zerolatency" "config-interval=-1" "alignment=7" "sync=false"
}
$entrada = 'udpsrc port=5000 caps="video/mpegts,systemstream=(boolean)true,packetsize=(int)188" ! queue ! tsdemux ! h264parse ! avdec_h264 ! videoconvert'

Write-Host "== 1.3: FPS e CPU via localhost =="
$udp = foreach ($nome in "original", "modificada") {
    foreach ($sync in "true", "false") {
        $rx = Start-Gst "$entrada ! $exibir sync=$sync"
        Start-Sleep 2
        $tx = Start-Gst $variantes[$nome]
        $cpu = @(Measure-Cpu @($tx, $rx) $Segundos)
        $r = Get-Fps $rx
        Stop-Gst $tx; Stop-Gst $rx
        $linha = [pscustomobject]@{ transmissor = $nome; sync_receptor = $sync; fps_medido = $r.Fps
            renderizados = $r.Render; descartados = $r.Desc
            cpu_transmissor_pct = $cpu[0]; cpu_receptor_pct = $cpu[1] }
        Write-Host ($linha | Format-Table -HideTableHeaders | Out-String).Trim()
        $linha
    }
}
$udp | Export-Csv "$saida\udp_localhost.csv" -NoTypeInformation -Delimiter ';' -Encoding UTF8

# ---------- 1.3: tempo ate o primeiro quadro ----------
# O identity imprime uma linha por quadro decodificado; medimos quando surge a 1a.
$marcado = "$entrada ! identity name=marca silent=false ! fakesink sync=false"
function Wait-Quadro($g, $relogio, [int]$limite = 20) {
    while ($relogio.Elapsed.TotalSeconds -lt $limite) {
        if ((Read-Log $g) -match 'GstIdentity:marca: last-message = chain') {
            return [math]::Round($relogio.Elapsed.TotalSeconds, 2)
        }
        Start-Sleep -Milliseconds 50
    }
    return $null
}
function Get-Resumo($variante, $cenario, $tempos) {
    $ok = @($tempos | Where-Object { $_ -ne $null } | Sort-Object)
    $linha = [pscustomobject]@{ variante = $variante; cenario = $cenario
        com_video = "$($ok.Count)/$($tempos.Count)"
        minimo_s = $(if ($ok) { $ok[0] }); mediana_s = $(if ($ok) { $ok[[math]::Floor($ok.Count / 2)] })
        maximo_s = $(if ($ok) { $ok[-1] }) }
    Write-Host ($linha | Format-Table -HideTableHeaders | Out-String).Trim()
    $linha
}
Write-Host "== 1.3: tempo ate o primeiro quadro =="
$sorteio = New-Object Random 42
$tempos = foreach ($nome in $variantes.Keys) {
    # a) receptor ja esperando: tempo a partir do inicio do transmissor
    $a = @(foreach ($i in 1..$Repeticoes) {
        $rx = Start-Gst $marcado; Start-Sleep 2
        $relogio = [Diagnostics.Stopwatch]::StartNew()
        $tx = Start-Gst $variantes[$nome]
        , (Wait-Quadro $rx $relogio)
        Stop-Gst $tx; Stop-Gst $rx
    })
    Get-Resumo $nome "receptor antes" $a
    # b) receptor entra com a transmissao em andamento
    $tx = Start-Gst $variantes[$nome]; Start-Sleep 3
    $b = @(foreach ($i in 1..$Repeticoes) {
        Start-Sleep -Milliseconds $sorteio.Next(0, 3000)
        $relogio = [Diagnostics.Stopwatch]::StartNew()
        $rx = Start-Gst $marcado
        , (Wait-Quadro $rx $relogio)
        Stop-Gst $rx
    })
    Stop-Gst $tx
    Get-Resumo $nome "receptor atrasado" $b
}
$tempos | Export-Csv "$saida\primeiro_quadro.csv" -NoTypeInformation -Delimiter ';' -Encoding UTF8

# ---------- 1.3: VLC como receptor ----------
if (Test-Path $Vlc) {
    Write-Host "== 1.3: VLC =="
    $vlcRes = foreach ($nome in "original", "modificada") {
        $pasta = Join-Path $tmp "vlc_$nome"
        if (Test-Path $pasta) { Remove-Item $pasta -Recurse -Force }
        New-Item -ItemType Directory $pasta | Out-Null
        $tx = Start-Gst $variantes[$nome]; Start-Sleep 3
        # Sem janela: o filtro "scene" salva um PNG a cada 25 quadros decodificados.
        $v = Start-Process $Vlc -PassThru -ArgumentList ("-I dummy --dummy-quiet --avcodec-hw=none --vout=dummy " +
            "--video-filter=scene --scene-format=png --scene-ratio=25 --scene-prefix=q --scene-path=`"$pasta`" " +
            "--run-time=15 udp://@:5000 vlc://quit")
        if (-not $v.WaitForExit(40000)) { $v.Kill() }
        Stop-Gst $tx
        $n = @(Get-ChildItem $pasta -Filter *.png).Count
        Write-Host "VLC / $nome : $n imagens salvas em 15 s"
        [pscustomobject]@{ transmissor = $nome; imagens_salvas_em_15s = $n }
    }
    $vlcRes | Export-Csv "$saida\vlc.csv" -NoTypeInformation -Delimiter ';' -Encoding UTF8
} else { Write-Host "VLC nao encontrado: teste ignorado." }
Write-Host "Concluido. CSVs em $saida"
