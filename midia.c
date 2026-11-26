#include "midia.h"

#include <gio/gio.h>

struct Midia {
    GstElement *pipeline;
    GstElement *fpssink;  /* so no receptor */
    GstElement *gtksink;  /* so no receptor com video incorporado */
    guint watch_id;
    MidiaCallback cb;
    gpointer dados;
    /* Contadores escritos pelas threads de streaming e lidos pela interface. */
    gint pacotes;
    gint quadros;
};

gboolean midia_validar_ip(const char *texto) {
    if (texto == NULL) return FALSE;
    GInetAddress *end = g_inet_address_new_from_string(texto);
    if (end == NULL) return FALSE;
    g_object_unref(end);
    return TRUE;
}

gboolean midia_elemento_disponivel(const char *nome) {
    GstElementFactory *f = gst_element_factory_find(nome);
    if (f == NULL) return FALSE;
    gst_object_unref(f);
    return TRUE;
}

static void avisar(Midia *m, MidiaEvento evento, const char *texto) {
    if (m->cb) m->cb(evento, texto, m->dados);
}

/* Mensagens do GstBus, entregues no loop principal. O callback da interface
 * NAO deve chamar midia_destruir() diretamente daqui; use g_idle_add(). */
static gboolean ao_receber_mensagem(GstBus *bus, GstMessage *msg,
                                    gpointer dados) {
    (void) bus;
    Midia *m = dados;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_WARNING: {
            gboolean eh_erro = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR;
            GError *err = NULL;
            gchar *detalhes = NULL;
            if (eh_erro)
                gst_message_parse_error(msg, &err, &detalhes);
            else
                gst_message_parse_warning(msg, &err, &detalhes);
            g_printerr("%s de %s: %s\n  detalhes: %s\n",
                       eh_erro ? "ERRO" : "AVISO",
                       GST_OBJECT_NAME(msg->src), err->message,
                       detalhes ? detalhes : "nenhum");
            avisar(m, eh_erro ? MIDIA_EVENTO_ERRO : MIDIA_EVENTO_AVISO,
                   err->message);
            g_error_free(err);
            g_free(detalhes);
            break;
        }
        case GST_MESSAGE_EOS:
            avisar(m, MIDIA_EVENTO_FIM, "Fim de fluxo (EOS)");
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(m->pipeline)) {
                GstState anterior, novo;
                gst_message_parse_state_changed(msg, &anterior, &novo, NULL);
                avisar(m, MIDIA_EVENTO_ESTADO,
                       gst_element_state_get_name(novo));
            }
            break;
        default:
            break;
    }
    return G_SOURCE_CONTINUE;
}

static GstPadProbeReturn contar_buffer(GstPad *pad, GstPadProbeInfo *info,
                                       gpointer contador) {
    (void) pad;
    (void) info;
    g_atomic_int_inc((gint *) contador);
    return GST_PAD_PROBE_OK;
}

static void instalar_contador(Midia *m, const char *elemento, gint *contador) {
    GstElement *e = gst_bin_get_by_name(GST_BIN(m->pipeline), elemento);
    GstPad *pad = gst_element_get_static_pad(e, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, contar_buffer, contador,
                      NULL);
    gst_object_unref(pad);
    gst_object_unref(e);
}

static Midia *criar_base(const char *descricao, MidiaCallback cb,
                         gpointer dados, GError **erro) {
    GError *erro_parse = NULL;
    g_print("Pipeline: %s\n", descricao);

    GstElement *pipeline = gst_parse_launch_full(
        descricao, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &erro_parse);
    if (pipeline == NULL) {
        g_propagate_error(erro, erro_parse);
        return NULL;
    }
    g_clear_error(&erro_parse);

    Midia *m = g_new0(Midia, 1);
    m->pipeline = pipeline;
    m->cb = cb;
    m->dados = dados;

    GstBus *bus = gst_element_get_bus(pipeline);
    m->watch_id = gst_bus_add_watch(bus, ao_receber_mensagem, m);
    gst_object_unref(bus);
    return m;
}

static void configurar(Midia *m, const char *elemento, const char *primeira,
                       ...) {
    GstElement *e = gst_bin_get_by_name(GST_BIN(m->pipeline), elemento);
    va_list args;
    va_start(args, primeira);
    g_object_set_valist(G_OBJECT(e), primeira, args);
    va_end(args);
    gst_object_unref(e);
}

Midia *midia_criar_transmissor(const ConfigTransmissor *cfg, MidiaCallback cb,
                               gpointer dados, GError **erro) {
    /* netsim: mesmo recurso de simulacao de rede usado em transmissor.c. */
    gchar *simulacao;
    if (cfg->perda_pct > 0) {
        gchar prob[G_ASCII_DTOSTR_BUF_SIZE];
        g_ascii_dtostr(prob, sizeof prob, cfg->perda_pct / 100.0);
        simulacao = g_strdup_printf("netsim drop-probability=%s ! ", prob);
    } else {
        simulacao = g_strdup("");
    }

    /* Pipeline modificada da Parte 1. Os valores que vem da interface sao
     * aplicados com g_object_set(), nunca concatenados na descricao. */
    gchar *descricao = g_strdup_printf(
        "videotestsrc is-live=true pattern=ball ! capsfilter name=formato ! "
        "videoconvert ! video/x-raw,format=I420 ! "
        "x264enc name=codificador tune=zerolatency ! "
        "h264parse config-interval=-1 ! "
        "mpegtsmux alignment=7 ! %s"
        "udpsink name=saida sync=false",
        simulacao);
    g_free(simulacao);

    Midia *m = criar_base(descricao, cb, dados, erro);
    g_free(descricao);
    if (m == NULL) return NULL;

    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw", "width", G_TYPE_INT, cfg->largura, "height", G_TYPE_INT,
        cfg->altura, "framerate", GST_TYPE_FRACTION, cfg->fps, 1, NULL);
    configurar(m, "formato", "caps", caps, NULL);
    gst_caps_unref(caps);

    /* key-int-max = fps: um quadro-chave por segundo. */
    configurar(m, "codificador", "bitrate", (guint) cfg->bitrate_kbps,
               "key-int-max", (guint) cfg->fps, NULL);
    GstElement *cod = gst_bin_get_by_name(GST_BIN(m->pipeline), "codificador");
    gst_util_set_object_arg(G_OBJECT(cod), "speed-preset", cfg->speed_preset);
    gst_object_unref(cod);

    configurar(m, "saida", "host", cfg->host, "port", cfg->porta, NULL);

    instalar_contador(m, "codificador", &m->quadros);
    return m;
}

Midia *midia_criar_receptor(int porta, gboolean incorporar, MidiaCallback cb,
                            gpointer dados, GError **erro) {
    const char *descricao =
        "udpsrc name=entrada "
        "caps=\"video/mpegts,systemstream=(boolean)true,packetsize=(int)188\" ! "
        "queue ! tsdemux ! h264parse ! avdec_h264 ! "
        "videoconvert name=conversor ! "
        "fpsdisplaysink name=exibicao text-overlay=false signal-fps-measurements=false "
        "sync=false";

    Midia *m = criar_base(descricao, cb, dados, erro);
    if (m == NULL) return NULL;

    /* reuse=FALSE: porta ja ocupada vira erro visivel na interface. */
    configurar(m, "entrada", "port", porta, "reuse", FALSE,
               "buffer-size", 2 * 1024 * 1024, NULL);

    m->fpssink = gst_bin_get_by_name(GST_BIN(m->pipeline), "exibicao");

    GstElement *video = NULL;
    if (incorporar) {
        video = gst_element_factory_make("gtksink", NULL);
        if (video) m->gtksink = gst_object_ref(video);
    }
    if (video == NULL) video = gst_element_factory_make("autovideosink", NULL);
    if (video == NULL) {
        g_set_error_literal(erro, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                            "Nenhum elemento de saida de video disponivel");
        midia_destruir(m);
        return NULL;
    }
    g_object_set(m->fpssink, "video-sink", video, NULL);

    instalar_contador(m, "entrada", &m->pacotes);
    instalar_contador(m, "conversor", &m->quadros);
    return m;
}

gpointer midia_obter_widget(Midia *m) {
    gpointer widget = NULL;
    if (m && m->gtksink) g_object_get(m->gtksink, "widget", &widget, NULL);
    return widget;
}

gboolean midia_iniciar(Midia *m, GError **erro) {
    if (gst_element_set_state(m->pipeline, GST_STATE_PLAYING) !=
        GST_STATE_CHANGE_FAILURE)
        return TRUE;

    /* A causa detalhada da falha fica no bus. */
    GstBus *bus = gst_element_get_bus(m->pipeline);
    GstMessage *msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
    if (msg) {
        gst_message_parse_error(msg, erro, NULL);
        gst_message_unref(msg);
    } else {
        g_set_error_literal(erro, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "A pipeline nao conseguiu ir para PLAYING");
    }
    gst_object_unref(bus);
    return FALSE;
}

void midia_obter_estatisticas(Midia *m, MidiaEstatisticas *est) {
    est->pacotes = (guint) g_atomic_int_get(&m->pacotes);
    est->quadros = (guint) g_atomic_int_get(&m->quadros);
    est->renderizados = 0;
    est->descartados = 0;
    if (m->fpssink)
        g_object_get(m->fpssink, "frames-rendered", &est->renderizados,
                     "frames-dropped", &est->descartados, NULL);
}

void midia_destruir(Midia *m) {
    if (m == NULL) return;
    if (m->watch_id) g_source_remove(m->watch_id);
    gst_element_set_state(m->pipeline, GST_STATE_NULL);
    if (m->fpssink) gst_object_unref(m->fpssink);
    if (m->gtksink) gst_object_unref(m->gtksink);
    gst_object_unref(m->pipeline);
    g_free(m);
}
