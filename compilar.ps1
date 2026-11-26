# Compila os programas usando o GCC + GStreamer + GTK 3 do MSYS2 (UCRT64).
# Uso (no terminal PowerShell do VS Code, dentro desta pasta):
#   powershell -ExecutionPolicy Bypass -File .\compilar.ps1

$msys = "C:\msys64\ucrt64\bin"
if (-not (Test-Path "$msys\gcc.exe")) {
    Write-Host "ERRO: nao encontrei $msys\gcc.exe. Instale o MSYS2 e os pacotes do README." -ForegroundColor Red
    exit 1
}
# O MSYS2 vem primeiro no PATH para nao misturar com um GStreamer MSVC instalado.
$env:Path = "$msys;$env:Path"
$env:PKG_CONFIG_PATH = "C:\msys64\ucrt64\lib\pkgconfig"

function Build-Programa($saida, $fontes, $pacotes) {
    $flags = (& "$msys\pkg-config.exe" --cflags --libs @pacotes) -split '\s+' | Where-Object { $_ }
    & "$msys\gcc.exe" -Wall -Wextra @fontes -o $saida @flags
    if ($LASTEXITCODE -ne 0) { Write-Host "Falha ao compilar $saida" -ForegroundColor Red; exit 1 }
    Write-Host "OK: $saida" -ForegroundColor Green
}

Build-Programa "transmissor.exe" @("transmissor.c") @("gstreamer-1.0")
Build-Programa "receptor.exe" @("receptor.c") @("gstreamer-1.0")
Build-Programa "servidor.exe" @("servidor.c", "midia.c") @("gtk+-3.0", "gstreamer-1.0", "gio-2.0")
Build-Programa "cliente.exe" @("cliente.c", "midia.c") @("gtk+-3.0", "gstreamer-1.0", "gio-2.0")
