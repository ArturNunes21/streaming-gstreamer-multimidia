/* Interface grafica do transmissor (servidor de midia) - GTK 3.
 * Uso: servidor [--iniciar]   (--iniciar comeca a transmitir ao abrir) */
#include <gtk/gtk.h>
#include <gst/gst.h>

#include "midia.h"

typedef struct {
    GtkWidget *ip, *porta, *resolucao, *fps, *bitrate, *preset, *perda;
    GtkWidget *grade, *botao_iniciar, *botao_parar, *estado;
    Midia *midia;
    guint cronometro;
    guint quadros_anteriores;
    int fps_configurado;
} Servidor;

static void definir_estado(Servidor *s, const char *texto) {
    gtk_label_set_text(GTK_LABEL(s->estado), texto);
    g_print("[estado] %s\n", texto);
}

static void habilitar_controles(Servidor *s, gboolean transmitindo) {
    gtk_widget_set_sensitive(s->grade, !transmitindo);
    gtk_widget_set_sensitive(s->botao_iniciar, !transmitindo);
    gtk_widget_set_sensitive(s->botao_parar, transmitindo);
}

static void encerrar(Servidor *s) {
    if (s->cronometro) {
        g_source_remove(s->cronometro);
        s->cronometro = 0;
    }
    midia_destruir(s->midia);
    s->midia = NULL;
}

static gboolean encerrar_depois(gpointer dados) {
    Servidor *s = dados;
    encerrar(s);
    habilitar_controles(s, FALSE);
    return G_SOURCE_REMOVE;
}

static gboolean atualizar_estado(gpointer dados) {
    Servidor *s = dados;
    MidiaEstatisticas est;
    midia_obter_estatisticas(s->midia, &est);

    guint no_ultimo_segundo = est.quadros - s->quadros_anteriores;
    s->quadros_anteriores = est.quadros;

    gchar *texto = g_strdup_printf(
        "Transmitindo: %u quadros/s codificados (configurado: %d), total %u",
        no_ultimo_segundo, s->fps_configurado, est.quadros);
    definir_estado(s, texto);
    g_free(texto);
    return G_SOURCE_CONTINUE;
}

static void ao_evento(MidiaEvento evento, const char *texto, gpointer dados) {
    Servidor *s = dados;
    if (evento == MIDIA_EVENTO_ERRO) {
        gchar *msg = g_strdup_printf("Erro: %s", texto);
        definir_estado(s, msg);
        g_free(msg);
        if (s->cronometro) {
            g_source_remove(s->cronometro);
            s->cronometro = 0;
        }
        /* Nao destruir a pipeline de dentro do watch do proprio bus. */
        g_idle_add(encerrar_depois, s);
    } else if (evento == MIDIA_EVENTO_ESTADO) {
        g_print("[pipeline] %s\n", texto);
    }
}

static void iniciar_transmissao(GtkButton *botao, gpointer dados) {
    (void) botao;
    Servidor *s = dados;
    if (s->midia) return;

    /* Recuperar e validar os valores da interface. */
    gchar *ip = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(s->ip))));
    if (!midia_validar_ip(ip)) {
        definir_estado(s, "Endereco IP invalido (exemplo: 127.0.0.1)");
        gtk_widget_grab_focus(s->ip);
        g_free(ip);
        return;
    }

    ConfigTransmissor cfg = {0};
    cfg.host = ip;
    gtk_spin_button_update(GTK_SPIN_BUTTON(s->porta));
    cfg.porta = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->porta));

    const char *res = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->resolucao));
    const char *fps = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->fps));
    const char *preset = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->preset));
    if (!res || !fps || !preset ||
        sscanf(res, "%dx%d", &cfg.largura, &cfg.altura) != 2) {
        definir_estado(s, "Selecione resolucao, FPS e preset");
        g_free(ip);
        return;
    }
    cfg.fps = atoi(fps);
    cfg.speed_preset = preset;
    gtk_spin_button_update(GTK_SPIN_BUTTON(s->bitrate));
    cfg.bitrate_kbps =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->bitrate));
    gtk_spin_button_update(GTK_SPIN_BUTTON(s->perda));
    cfg.perda_pct = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->perda));

    if (cfg.perda_pct > 0 && !midia_elemento_disponivel("netsim")) {
        definir_estado(s, "netsim indisponivel: use perda simulada = 0");
        g_free(ip);
        return;
    }

    /* Configurar e iniciar a pipeline. */
    GError *erro = NULL;
    s->midia = midia_criar_transmissor(&cfg, ao_evento, s, &erro);
    g_free(ip);

    if (s->midia == NULL || !midia_iniciar(s->midia, &erro)) {
        gchar *msg = g_strdup_printf("Erro ao iniciar: %s",
                                     erro ? erro->message : "desconhecido");
        definir_estado(s, msg);
        g_free(msg);
        g_clear_error(&erro);
        encerrar(s);
        return;
    }

    s->fps_configurado = cfg.fps;
    s->quadros_anteriores = 0;
    s->cronometro = g_timeout_add(1000, atualizar_estado, s);
    habilitar_controles(s, TRUE);
    definir_estado(s, "Iniciando transmissao...");
}

static void interromper_transmissao(GtkButton *botao, gpointer dados) {
    (void) botao;
    Servidor *s = dados;
    encerrar(s);
    habilitar_controles(s, FALSE);
    definir_estado(s, "Transmissao interrompida");
}

static void ao_fechar(GtkWidget *janela, gpointer dados) {
    (void) janela;
    encerrar(dados);
    gtk_main_quit();
}

static void adicionar_linha(GtkWidget *grade, int linha, const char *rotulo,
                            GtkWidget *controle) {
    GtkWidget *label = gtk_label_new(rotulo);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(controle, TRUE);
    gtk_grid_attach(GTK_GRID(grade), label, 0, linha, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), controle, 1, linha, 1, 1);
}

static GtkWidget *criar_combo(const char *const *itens, const char *padrao) {
    GtkWidget *combo = gtk_combo_box_text_new();
    for (int i = 0; itens[i]; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), itens[i], itens[i]);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), padrao);
    return combo;
}

static GtkWidget *criar_numero(int minimo, int maximo, int passo, int valor) {
    GtkWidget *spin = gtk_spin_button_new_with_range(minimo, maximo, passo);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), valor);
    return spin;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    static const char *resolucoes[] = {"320x240", "640x360", "1280x720", NULL};
    static const char *taxas[] = {"15", "25", "30", NULL};
    static const char *presets[] = {"ultrafast", "fast", "medium", NULL};

    Servidor s = {0};

    GtkWidget *janela = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(janela), "Servidor Multimidia");
    gtk_window_set_default_size(GTK_WINDOW(janela), 480, 360);
    gtk_container_set_border_width(GTK_CONTAINER(janela), 14);
    g_signal_connect(janela, "destroy", G_CALLBACK(ao_fechar), &s);

    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(janela), caixa);

    s.grade = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(s.grade), 8);
    gtk_grid_set_column_spacing(GTK_GRID(s.grade), 12);
    gtk_box_pack_start(GTK_BOX(caixa), s.grade, FALSE, FALSE, 0);

    s.ip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(s.ip), "127.0.0.1");
    s.porta = criar_numero(1, 65535, 1, 5000);
    s.resolucao = criar_combo(resolucoes, "640x360");
    s.fps = criar_combo(taxas, "25");
    s.bitrate = criar_numero(100, 8000, 100, 500);
    s.preset = criar_combo(presets, "ultrafast");
    s.perda = criar_numero(0, 50, 1, 0);

    adicionar_linha(s.grade, 0, "IP de destino", s.ip);
    adicionar_linha(s.grade, 1, "Porta UDP", s.porta);
    adicionar_linha(s.grade, 2, "Resolucao", s.resolucao);
    adicionar_linha(s.grade, 3, "Quadros por segundo", s.fps);
    adicionar_linha(s.grade, 4, "Bitrate (kbps)", s.bitrate);
    adicionar_linha(s.grade, 5, "Preset do x264", s.preset);
    adicionar_linha(s.grade, 6, "Perda simulada (%)", s.perda);

    GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(botoes), TRUE);
    s.botao_iniciar = gtk_button_new_with_label("Iniciar");
    s.botao_parar = gtk_button_new_with_label("Interromper");
    gtk_box_pack_start(GTK_BOX(botoes), s.botao_iniciar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(botoes), s.botao_parar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caixa), botoes, FALSE, FALSE, 0);

    s.estado = gtk_label_new("Parado");
    gtk_label_set_line_wrap(GTK_LABEL(s.estado), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(s.estado), 50);
    gtk_box_pack_start(GTK_BOX(caixa), s.estado, FALSE, FALSE, 0);

    g_signal_connect(s.botao_iniciar, "clicked",
                     G_CALLBACK(iniciar_transmissao), &s);
    g_signal_connect(s.botao_parar, "clicked",
                     G_CALLBACK(interromper_transmissao), &s);

    habilitar_controles(&s, FALSE);
    gtk_widget_show_all(janela);

    if (argc > 1 && g_strcmp0(argv[1], "--iniciar") == 0)
        iniciar_transmissao(NULL, &s);

    gtk_main();
    return 0;
}
