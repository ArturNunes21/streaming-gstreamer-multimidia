#include <gtk/gtk.h>
#include <gst/gst.h>

static GtkWidget *campo_porta, *botao_iniciar, *botao_parar, *label_estado;
static GtkWidget *area_video, *widget_video = NULL;
static GstElement *pipeline = NULL;
static GstElement *tela = NULL;
static guint id_bus = 0;
static guint id_timer = 0;
static guint quadros_anteriores = 0;

static void parar_pipeline(void) {
    if (pipeline) {
        if (id_bus) g_source_remove(id_bus);
        if (id_timer) g_source_remove(id_timer);
        id_bus = 0;
        id_timer = 0;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(tela);
        gst_object_unref(pipeline);
        pipeline = NULL;
    }
    if (widget_video) {
        gtk_widget_destroy(widget_video);
        widget_video = NULL;
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
    }
    return TRUE;
}

static gboolean atualizar_estado(gpointer data) {
    guint quadros = 0;
    g_object_get(tela, "frames-rendered", &quadros, NULL);

    if (quadros == 0)
        gtk_label_set_text(GTK_LABEL(label_estado), "Aguardando video...");
    else if (quadros > quadros_anteriores)
        gtk_label_set_text(GTK_LABEL(label_estado), "Recebendo video");
    else
        gtk_label_set_text(GTK_LABEL(label_estado), "Video interrompido");

    quadros_anteriores = quadros;
    return TRUE;
}

static void iniciar_recepcao(GtkButton *botao, gpointer dados) {
    int porta = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(campo_porta));

    GError *erro = NULL;
    pipeline = gst_parse_launch(
        "udpsrc name=entrada reuse=false "
        "caps=\"video/mpegts,systemstream=true,packetsize=188\" ! "
        "queue ! tsdemux ! h264parse ! avdec_h264 ! videoconvert ! "
        "fpsdisplaysink name=tela text-overlay=false sync=false", &erro);
    if (erro) {
        gtk_label_set_text(GTK_LABEL(label_estado), erro->message);
        g_error_free(erro);
        if (pipeline) gst_object_unref(pipeline);
        pipeline = NULL;
        return;
    }

    GstElement *entrada = gst_bin_get_by_name(GST_BIN(pipeline), "entrada");
    g_object_set(entrada, "port", porta, NULL);
    gst_object_unref(entrada);

    tela = gst_bin_get_by_name(GST_BIN(pipeline), "tela");

    GstElement *video = gst_element_factory_make("gtksink", NULL);
    if (video) {
        g_object_set(tela, "video-sink", video, NULL);
        g_object_get(video, "widget", &widget_video, NULL);
        gtk_box_pack_start(GTK_BOX(area_video), widget_video, TRUE, TRUE, 0);
        gtk_widget_show(widget_video);
        g_object_unref(widget_video);
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    id_bus = gst_bus_add_watch(bus, bus_call, NULL);
    gst_object_unref(bus);

    quadros_anteriores = 0;
    id_timer = g_timeout_add(1000, atualizar_estado, NULL);

    gtk_widget_set_sensitive(botao_iniciar, FALSE);
    gtk_widget_set_sensitive(botao_parar, TRUE);
    gtk_label_set_text(GTK_LABEL(label_estado), "Aguardando video...");

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        gtk_label_set_text(GTK_LABEL(label_estado), "Erro ao iniciar. A porta esta livre?");
        parar_pipeline();
    }
}

static void interromper_recepcao(GtkButton *botao, gpointer dados) {
    parar_pipeline();
    gtk_label_set_text(GTK_LABEL(label_estado), "Parado");
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    GtkWidget *janela = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(janela), "Cliente Multimidia");
    gtk_window_set_default_size(GTK_WINDOW(janela), 660, 520);
    gtk_container_set_border_width(GTK_CONTAINER(janela), 10);
    g_signal_connect(janela, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(janela), caixa);

    campo_porta = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(campo_porta), 5000);
    botao_iniciar = gtk_button_new_with_label("Iniciar");
    botao_parar = gtk_button_new_with_label("Interromper");
    label_estado = gtk_label_new("Parado");

    area_video = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(area_video, 640, 360);

    gtk_box_pack_start(GTK_BOX(caixa), gtk_label_new("Porta UDP"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), campo_porta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), botao_iniciar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), botao_parar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), area_video, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), label_estado, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(botao_parar, FALSE);

    g_signal_connect(botao_iniciar, "clicked", G_CALLBACK(iniciar_recepcao), NULL);
    g_signal_connect(botao_parar, "clicked", G_CALLBACK(interromper_recepcao), NULL);

    gtk_widget_show_all(janela);
    gtk_main();

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(tela);
        gst_object_unref(pipeline);
    }
    return 0;
}
