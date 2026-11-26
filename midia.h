/* Construcao e controle das pipelines de video (MPEG-TS/UDP).
 * Usado pelas duas interfaces graficas (servidor.c e cliente.c).
 *
 * Evolucao das funcoes de transmissor.c / receptor.c: em vez de bloquear em
 * gst_bus_timed_pop_filtered(), as mensagens do GstBus sao tratadas por
 * gst_bus_add_watch() no loop principal do GLib, que e o mesmo do GTK. */
#ifndef MIDIA_H
#define MIDIA_H

#include <gst/gst.h>

typedef struct {
    const char *host;         /* IP de destino */
    int porta;                /* porta UDP de destino */
    int largura;
    int altura;
    int fps;
    int bitrate_kbps;
    const char *speed_preset; /* ultrafast, fast, medium... */
    int perda_pct;            /* perda simulada com netsim (0 = desligada) */
} ConfigTransmissor;

typedef enum {
    MIDIA_EVENTO_ERRO,
    MIDIA_EVENTO_AVISO,
    MIDIA_EVENTO_FIM,         /* EOS */
    MIDIA_EVENTO_ESTADO       /* mudanca de estado da pipeline */
} MidiaEvento;

/* Chamada no loop principal do GLib, portanto pode mexer na interface. */
typedef void (*MidiaCallback)(MidiaEvento evento, const char *texto,
                              gpointer dados);

typedef struct {
    guint pacotes;      /* receptor: datagramas UDP que chegaram */
    guint quadros;      /* transmissor: codificados; receptor: decodificados */
    guint renderizados; /* receptor: quadros desenhados pelo sink */
    guint descartados;  /* receptor: quadros descartados pelo sink */
} MidiaEstatisticas;

typedef struct Midia Midia;

/* TRUE se o texto for um endereco IPv4 ou IPv6 valido. */
gboolean midia_validar_ip(const char *texto);

/* TRUE se os elementos do GStreamer necessarios estiverem instalados. */
gboolean midia_elemento_disponivel(const char *nome);

Midia *midia_criar_transmissor(const ConfigTransmissor *cfg, MidiaCallback cb,
                               gpointer dados, GError **erro);

/* incorporar = TRUE tenta usar gtksink (video dentro da janela); se o plugin
 * nao existir, usa autovideosink (janela separada). */
Midia *midia_criar_receptor(int porta, gboolean incorporar, MidiaCallback cb,
                            gpointer dados, GError **erro);

/* Widget GTK do gtksink (GtkWidget *), ou NULL se o video abrir em janela
 * separada. A referencia retornada pertence ao chamador (g_object_unref). */
gpointer midia_obter_widget(Midia *m);

gboolean midia_iniciar(Midia *m, GError **erro);

void midia_obter_estatisticas(Midia *m, MidiaEstatisticas *est);

/* Leva a pipeline para NULL e libera todos os recursos. Aceita NULL. */
void midia_destruir(Midia *m);

#endif
