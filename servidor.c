#include <gtk/gtk.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>

static GtkWidget *campo_ip, *campo_porta, *combo_resolucao, *combo_fps;
static GtkWidget *campo_bitrate, *botao_iniciar, *botao_parar, *label_estado;
static GstElement *pipeline = NULL;
static guint id_bus = 0;

static void parar_pipeline(void) {
    if (pipeline) {
        if (id_bus) g_source_remove(id_bus);
        id_bus = 0;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = NULL;
    }
    gtk_widget_set_sensitive(botao_iniciar, TRUE);
    gtk_widget_set_sensitive(botao_parar, FALSE);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err;
        gst_message_parse_error(msg, &err, NULL);
        g_printerr("ERRO: %s\n", err->message);
        gtk_label_set_text(GTK_LABEL(label_estado), err->message);
        g_error_free(err);
        parar_pipeline();
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_STATE_CHANGED &&
               GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
        GstState novo;
        gst_message_parse_state_changed(msg, NULL, &novo, NULL);
        if (novo == GST_STATE_PLAYING)
            gtk_label_set_text(GTK_LABEL(label_estado), "Transmitindo");
    }
    return TRUE;
}

static void iniciar_transmissao(GtkButton *botao, gpointer dados) {
    const char *ip = gtk_entry_get_text(GTK_ENTRY(campo_ip));
    int porta = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(campo_porta));
    int bitrate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(campo_bitrate));
    const char *resolucao = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_resolucao));
    int fps = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_fps)));
    int largura, altura;

    if (!g_hostname_is_ip_address(ip)) {
        gtk_label_set_text(GTK_LABEL(label_estado), "IP invalido");
        return;
    }
    sscanf(resolucao, "%dx%d", &largura, &altura);

    GError *erro = NULL;
    pipeline = gst_parse_launch(
        "videotestsrc is-live=true pattern=ball ! capsfilter name=filtro ! "
        "videoconvert ! video/x-raw,format=I420 ! "
        "x264enc name=encoder tune=zerolatency speed-preset=ultrafast ! "
        "h264parse config-interval=-1 ! mpegtsmux alignment=7 ! "
        "udpsink name=saida sync=false", &erro);
    if (erro) {
        gtk_label_set_text(GTK_LABEL(label_estado), erro->message);
        g_error_free(erro);
        if (pipeline) gst_object_unref(pipeline);
        pipeline = NULL;
        return;
    }

    GstElement *filtro = gst_bin_get_by_name(GST_BIN(pipeline), "filtro");
    GstElement *encoder = gst_bin_get_by_name(GST_BIN(pipeline), "encoder");
    GstElement *saida = gst_bin_get_by_name(GST_BIN(pipeline), "saida");
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
        "width", G_TYPE_INT, largura, "height", G_TYPE_INT, altura,
        "framerate", GST_TYPE_FRACTION, fps, 1, NULL);

    g_object_set(filtro, "caps", caps, NULL);
    g_object_set(encoder, "bitrate", bitrate, "key-int-max", fps, NULL);
    g_object_set(saida, "host", ip, "port", porta, NULL);

    gst_caps_unref(caps);
    gst_object_unref(filtro);
    gst_object_unref(encoder);
    gst_object_unref(saida);

    GstBus *bus = gst_element_get_bus(pipeline);
    id_bus = gst_bus_add_watch(bus, bus_call, NULL);
    gst_object_unref(bus);

    gtk_widget_set_sensitive(botao_iniciar, FALSE);
    gtk_widget_set_sensitive(botao_parar, TRUE);
    gtk_label_set_text(GTK_LABEL(label_estado), "Iniciando...");

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        gtk_label_set_text(GTK_LABEL(label_estado), "Erro ao iniciar a transmissao");
        parar_pipeline();
    }
}

static void interromper_transmissao(GtkButton *botao, gpointer dados) {
    parar_pipeline();
    gtk_label_set_text(GTK_LABEL(label_estado), "Parado");
}

static void adicionar_campo(GtkWidget *caixa, const char *texto, GtkWidget *campo) {
    gtk_box_pack_start(GTK_BOX(caixa), gtk_label_new(texto), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), campo, FALSE, FALSE, 0);
}

static GtkWidget *criar_combo(const char *a, const char *b, const char *c) {
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), a, a);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), b, b);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), c, c);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), b);
    return combo;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    GtkWidget *janela = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(janela), "Servidor Multimidia");
    gtk_window_set_default_size(GTK_WINDOW(janela), 400, 300);
    gtk_container_set_border_width(GTK_CONTAINER(janela), 10);
    g_signal_connect(janela, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(janela), caixa);

    campo_ip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(campo_ip), "127.0.0.1");

    campo_porta = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(campo_porta), 5000);

    combo_resolucao = criar_combo("320x240", "640x360", "1280x720");
    combo_fps = criar_combo("15", "25", "30");

    campo_bitrate = gtk_spin_button_new_with_range(100, 5000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(campo_bitrate), 500);

    adicionar_campo(caixa, "IP de destino", campo_ip);
    adicionar_campo(caixa, "Porta UDP", campo_porta);
    adicionar_campo(caixa, "Resolucao", combo_resolucao);
    adicionar_campo(caixa, "FPS", combo_fps);
    adicionar_campo(caixa, "Bitrate (kbps)", campo_bitrate);

    botao_iniciar = gtk_button_new_with_label("Iniciar");
    botao_parar = gtk_button_new_with_label("Interromper");
    label_estado = gtk_label_new("Parado");
    gtk_box_pack_start(GTK_BOX(caixa), botao_iniciar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), botao_parar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), label_estado, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(botao_parar, FALSE);

    g_signal_connect(botao_iniciar, "clicked", G_CALLBACK(iniciar_transmissao), NULL);
    g_signal_connect(botao_parar, "clicked", G_CALLBACK(interromper_transmissao), NULL);

    gtk_widget_show_all(janela);
    gtk_main();

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    return 0;
}
