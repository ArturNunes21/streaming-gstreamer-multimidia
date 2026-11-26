/* Interface grafica do receptor (cliente de midia) - GTK 3.
 * Uso: cliente [--iniciar]   (--iniciar comeca a receber ao abrir) */
#include <gtk/gtk.h>
#include <gst/gst.h>

#include "midia.h"

/* Segundos sem quadros novos ate considerar que o video parou. */
#define SEGUNDOS_SEM_VIDEO 2

typedef struct {
    GtkWidget *porta, *incorporar, *botao_iniciar, *botao_parar, *estado;
    GtkWidget *area_video, *aviso_video, *widget_video;
    Midia *midia;
    guint cronometro;
    MidiaEstatisticas anteriores;
    int segundos_sem_video;
    gboolean em_janela_separada;
} Cliente;

static void definir_estado(Cliente *c, const char *texto) {
    gtk_label_set_text(GTK_LABEL(c->estado), texto);
    g_print("[estado] %s\n", texto);
}

static void habilitar_controles(Cliente *c, gboolean recebendo) {
    gtk_widget_set_sensitive(c->porta, !recebendo);
    gtk_widget_set_sensitive(c->incorporar, !recebendo);
    gtk_widget_set_sensitive(c->botao_iniciar, !recebendo);
    gtk_widget_set_sensitive(c->botao_parar, recebendo);
}

static void encerrar(Cliente *c) {
    if (c->cronometro) {
        g_source_remove(c->cronometro);
        c->cronometro = 0;
    }
    midia_destruir(c->midia);
    c->midia = NULL;
}

/* Remove o video da janela e volta a mostrar o texto de espera. */
static void limpar_video(Cliente *c) {
    if (c->widget_video) {
        gtk_widget_destroy(c->widget_video);
        c->widget_video = NULL;
    }
    gtk_widget_show(c->aviso_video);
}

static gboolean encerrar_depois(gpointer dados) {
    Cliente *c = dados;
    encerrar(c);
    limpar_video(c);
    habilitar_controles(c, FALSE);
    return G_SOURCE_REMOVE;
}

/* O estado PLAYING nao garante que chegou video. Por isso o indicador e
 * baseado nos pacotes UDP recebidos e nos quadros realmente decodificados. */
static gboolean atualizar_estado(gpointer dados) {
    Cliente *c = dados;
    MidiaEstatisticas est;
    midia_obter_estatisticas(c->midia, &est);

    guint pacotes = est.pacotes - c->anteriores.pacotes;
    guint quadros = est.quadros - c->anteriores.quadros;
    c->anteriores = est;
    c->segundos_sem_video = quadros > 0 ? 0 : c->segundos_sem_video + 1;

    gchar *texto;
    if (est.pacotes == 0) {
        texto = g_strdup("Aguardando pacotes na porta (nenhum recebido)");
    } else if (est.quadros == 0) {
        texto = g_strdup_printf(
            "Pacotes chegando (%u), mas nenhum quadro decodificado ainda",
            est.pacotes);
    } else if (c->segundos_sem_video >= SEGUNDOS_SEM_VIDEO) {
        texto = g_strdup_printf(
            "Video interrompido: sem quadros novos ha %d s (pacotes/s: %u)",
            c->segundos_sem_video, pacotes);
    } else {
        texto = g_strdup_printf(
            "Recebendo video: %u quadros/s decodificados | exibidos %u | "
            "descartados pelo sink %u",
            quadros, est.renderizados, est.descartados);
    }
    definir_estado(c, texto);
    g_free(texto);
    return G_SOURCE_CONTINUE;
}

static void ao_evento(MidiaEvento evento, const char *texto, gpointer dados) {
    Cliente *c = dados;
    if (evento == MIDIA_EVENTO_ERRO || evento == MIDIA_EVENTO_FIM) {
        gchar *msg = g_strdup_printf(
            evento == MIDIA_EVENTO_ERRO ? "Erro: %s" : "%s", texto);
        definir_estado(c, msg);
        g_free(msg);
        if (c->cronometro) {
            g_source_remove(c->cronometro);
            c->cronometro = 0;
        }
        /* Nao destruir a pipeline de dentro do watch do proprio bus. */
        g_idle_add(encerrar_depois, c);
    } else if (evento == MIDIA_EVENTO_ESTADO) {
        g_print("[pipeline] %s\n", texto);
    }
}

static void iniciar_recepcao(GtkButton *botao, gpointer dados) {
    (void) botao;
    Cliente *c = dados;
    if (c->midia) return;

    gtk_spin_button_update(GTK_SPIN_BUTTON(c->porta));
    int porta = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(c->porta));
    gboolean incorporar =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->incorporar));

    limpar_video(c);

    GError *erro = NULL;
    c->midia = midia_criar_receptor(porta, incorporar, ao_evento, c, &erro);
    if (c->midia) {
        GtkWidget *video = midia_obter_widget(c->midia);
        c->em_janela_separada = video == NULL;
        if (video) {
            c->widget_video = video;
            gtk_widget_hide(c->aviso_video);
            gtk_box_pack_start(GTK_BOX(c->area_video), video, TRUE, TRUE, 0);
            gtk_widget_show(video);
            g_object_unref(video); /* o container mantem a referencia */
        }
    }

    if (c->midia == NULL || !midia_iniciar(c->midia, &erro)) {
        gchar *msg = g_strdup_printf(
            "Erro ao iniciar (verifique se a porta %d esta livre): %s", porta,
            erro ? erro->message : "desconhecido");
        definir_estado(c, msg);
        g_free(msg);
        g_clear_error(&erro);
        encerrar(c);
        limpar_video(c);
        return;
    }

    c->anteriores = (MidiaEstatisticas){0};
    c->segundos_sem_video = 0;
    c->cronometro = g_timeout_add(1000, atualizar_estado, c);
    habilitar_controles(c, TRUE);
    definir_estado(c, incorporar && c->em_janela_separada
                          ? "gtksink indisponivel: video em janela separada. "
                            "Aguardando pacotes..."
                          : "Aguardando pacotes...");
}

static void interromper_recepcao(GtkButton *botao, gpointer dados) {
    (void) botao;
    Cliente *c = dados;
    encerrar(c);
    limpar_video(c);
    habilitar_controles(c, FALSE);
    definir_estado(c, "Recepcao interrompida");
}

static void ao_fechar(GtkWidget *janela, gpointer dados) {
    (void) janela;
    encerrar(dados);
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    Cliente c = {0};

    GtkWidget *janela = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(janela), "Cliente Multimidia");
    gtk_window_set_default_size(GTK_WINDOW(janela), 680, 520);
    gtk_container_set_border_width(GTK_CONTAINER(janela), 14);
    g_signal_connect(janela, "destroy", G_CALLBACK(ao_fechar), &c);

    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(janela), caixa);

    GtkWidget *barra = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(barra), gtk_label_new("Porta UDP"), FALSE, FALSE,
                       0);
    c.porta = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(c.porta), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(c.porta), 5000);
    gtk_box_pack_start(GTK_BOX(barra), c.porta, FALSE, FALSE, 0);
    c.incorporar = gtk_check_button_new_with_label("Video nesta janela");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c.incorporar), TRUE);
    gtk_box_pack_start(GTK_BOX(barra), c.incorporar, FALSE, FALSE, 0);
    c.botao_parar = gtk_button_new_with_label("Interromper");
    c.botao_iniciar = gtk_button_new_with_label("Iniciar");
    gtk_box_pack_end(GTK_BOX(barra), c.botao_parar, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(barra), c.botao_iniciar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), barra, FALSE, FALSE, 0);

    GtkWidget *moldura = gtk_frame_new(NULL);
    c.area_video = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(c.area_video, 640, 360);
    c.aviso_video = gtk_label_new("Area de reproducao");
    gtk_box_pack_start(GTK_BOX(c.area_video), c.aviso_video, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(moldura), c.area_video);
    gtk_box_pack_start(GTK_BOX(caixa), moldura, TRUE, TRUE, 0);

    c.estado = gtk_label_new("Parado");
    gtk_label_set_line_wrap(GTK_LABEL(c.estado), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(c.estado), 70);
    gtk_box_pack_start(GTK_BOX(caixa), c.estado, FALSE, FALSE, 0);

    g_signal_connect(c.botao_iniciar, "clicked", G_CALLBACK(iniciar_recepcao),
                     &c);
    g_signal_connect(c.botao_parar, "clicked", G_CALLBACK(interromper_recepcao),
                     &c);

    habilitar_controles(&c, FALSE);
    gtk_widget_show_all(janela);

    if (argc > 1 && g_strcmp0(argv[1], "--iniciar") == 0)
        iniciar_recepcao(NULL, &c);

    gtk_main();
    return 0;
}
