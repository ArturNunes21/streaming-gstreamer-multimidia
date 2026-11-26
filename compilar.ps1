$env:Path = "C:\msys64\ucrt64\bin;$env:Path"

$gst = (pkg-config --cflags --libs gstreamer-1.0) -split '\s+' | Where-Object { $_ }
$gtk = (pkg-config --cflags --libs gtk+-3.0 gstreamer-1.0) -split '\s+' | Where-Object { $_ }

gcc transmissor.c -o transmissor.exe @gst
gcc receptor.c -o receptor.exe @gst
gcc servidor.c -o servidor.exe @gtk
gcc cliente.c -o cliente.exe @gtk
