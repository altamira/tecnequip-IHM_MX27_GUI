#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "defines.h"
#include "GtkUtils.h"

static const int FLAG_PROG_TAMANHO = 0x0200;
static const int FLAG_PROG_GRAVAR  = 0x0800;

static const int FLAG_CMD_MESA_SUBIR    = 0x0002;
static const int FLAG_CMD_MESA_DESCER   = 0x2000;
static const int FLAG_CMD_APLAN_SUBIR   = 0x4000;
static const int FLAG_CMD_APLAN_DESCER  = 0x8000;
static const int FLAG_CMD_DESBOB_LIGAR  = 0x0100;

static const int FLAG_PROG_PRENSA_SINGELO = 0x0001;
static const int FLAG_PROG_PRENSA_LIGAR   = 0x0002;
static const int FLAG_PROG_REPET_PROXIMO  = 0x0004;
static const int FLAG_PROG_REPET_ANTERIOR = 0x0008;
static const int FLAG_PROG_PASSO_PROXIMO  = 0x0010;
static const int FLAG_PROG_PASSO_ANTERIOR = 0x0020;
static const int FLAG_PROG_LER_ESTADO     = 0x0040;
static const int FLAG_PROG_LER_DADOS      = 0x0080;
static const int FLAG_PROG_FINALIZAR      = 0x0100;

static const int REG_PROG_INDICE     = 23;
static const int REG_PROG_AVANCO     = 24;
static const int REG_PROG_PORTAS     = 25;
static const int REG_PROG_REPETICOES = 18;
static const int REG_PROG_FLAGS      = 31;

static const int PROG_MASK_PRENSA    = 0x0001;
static const int PROG_MASK_FACA      = 0x0002;

#define MAX_PROG_PASSOS 10
#define MAX_PROG_TEXTO  51 // Tamanho do campo no BD + 1 para final

static const int NTB_CADPROG_LISTA    = 0;
static const int NTB_CADPROG_PROGRAMA = 1;
static const int NTB_CADPROG_PASSOS   = 2;

// Definicoes para cadastro de programas

typedef struct {
  int  Avanco;
  int  Portas;
  int  Repeticoes;
  int  LigarPrensa;
  int  PedirAvanco;
  char TextoAvanco[MAX_PROG_TEXTO + 1];
  int  PedirRepeticoes;
  char TextoRepeticoes[MAX_PROG_TEXTO + 1];
} tProgPassoData;

typedef struct {
  unsigned int   ID;
  char           Nome[MAX_PROG_TEXTO + 1];
  int            ModoSingelo;
  int            Quantidade;
  int            PedirQuantidade;
  int            QtdPassos;
  tProgPassoData Passos[MAX_PROG_PASSOS];
} tProgData;

void ProgCarregarDados(int forcar)
{
  char *arrayFieldNames[][4] = {
      { "lblExecProgPasso", "lblExecProgAvanco", "lblExecProgRepeticao", "lblExecProgPortas" },
  };

  static int ultProgPassoAtual = -1, ultProgRepetAtual = -1;
  int ProgPassoAtual, ProgRepetAtual;

  // Carrega os indices do passo atual
  MaqGravarRegistrador(REG_PROG_FLAGS, MaqLerRegistrador(REG_PROG_FLAGS, 0) | FLAG_PROG_LER_ESTADO);
  while(MaqLerRegistrador(REG_PROG_FLAGS, 0) & FLAG_PROG_LER_ESTADO);

  ProgPassoAtual = MaqLerRegistrador(REG_PROG_INDICE    , 0);
  ProgRepetAtual = MaqLerRegistrador(REG_PROG_REPETICOES, 0);

  if(forcar || ultProgPassoAtual != ProgPassoAtual || ultProgRepetAtual != ProgRepetAtual) {
    char tmp[100];
    char **FieldNames = arrayFieldNames[0];

    // Carrega os dados do passo atual
    MaqGravarRegistrador(REG_PROG_FLAGS, MaqLerRegistrador(REG_PROG_FLAGS, 0) | FLAG_PROG_LER_DADOS);
    while(MaqLerRegistrador(REG_PROG_FLAGS, 0) & FLAG_PROG_LER_DADOS);

    int ProgQtdPassos = MaqLerRegistrador(REG_PROG_INDICE    , 0);
    int ProgQtdRepet  = MaqLerRegistrador(REG_PROG_REPETICOES, 0);
    int ProgAvanco    = MaqLerRegistrador(REG_PROG_AVANCO    , 0);
    int ProgPortas    = MaqLerRegistrador(REG_PROG_PORTAS    , 0);

    sprintf(tmp, "%d de %d", ProgPassoAtual + 1, ProgQtdPassos);
    GravarValorWidget(FieldNames[0], tmp);

    sprintf(tmp, "%d", ProgAvanco / 100);
    GravarValorWidget(FieldNames[1], tmp);

    sprintf(tmp, "%d de %d", ProgRepetAtual + 1, ProgQtdRepet);
    GravarValorWidget(FieldNames[2], tmp);

    sprintf(tmp, "%c %c %c", (ProgPortas & 0x01) ? 'X' : '-',
        (ProgPortas & 0x02) ? 'X' : '-', (ProgPortas & 0x04) ? 'X' : '-');
    GravarValorWidget(FieldNames[3], tmp);

    ultProgPassoAtual = ProgPassoAtual;
    ultProgRepetAtual = ProgRepetAtual;
  }
}

void AbrirProgParam(void);

tProgData ProgPrensa_Carregar(unsigned int numProg)
{
  int i;
  char sql[300];
  tProgData ProgData;

  sprintf(sql, "select * from ProgPrensaPassos where ProgPrensaID='%d' order by ID", numProg);
  DB_Execute(&mainDB, 0, sql);

  for(i = 0; DB_GetNextRow(&mainDB, 0) > 0 && i < MAX_PROG_PASSOS; i++) {
    ProgData.Passos[i].Avanco          = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "ProgPrensaAvanco")));
    ProgData.Passos[i].Portas          = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "ProgPrensaPortas")));
    ProgData.Passos[i].Repeticoes      = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "Repeticoes"      )));
    ProgData.Passos[i].LigarPrensa     = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "LigarPrensa"     )));
    ProgData.Passos[i].PedirAvanco     = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "PedirAvanco"     )));
    ProgData.Passos[i].PedirRepeticoes = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "PedirRepeticoes" )));

    strcpy(ProgData.Passos[i].TextoAvanco    , DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "TextoAvanco"     )));
    strcpy(ProgData.Passos[i].TextoRepeticoes, DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "TextoRepeticoes")));
  }

  ProgData.QtdPassos = i;

  sprintf(sql, "select * from ProgPrensa where ID=%d", numProg);
  DB_Execute(&mainDB, 0, sql);
  DB_GetNextRow(&mainDB, 0);

  ProgData.ID = numProg;
  ProgData.ModoSingelo     = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "ModoSingelo")));
  ProgData.PedirQuantidade = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "PedirNumCiclos")));
  ProgData.Quantidade      = atoi(DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "NumCiclos")));

  strcpy(ProgData.Nome, DB_GetData(&mainDB, 0, DB_GetFieldNumber(&mainDB, 0, "Nome")));

  return ProgData;
}

void ProgPrensa_Enviar(tProgData ProgData)
{
  int i;

  MaqGravarRegistrador(REG_PROG_INDICE, ProgData.QtdPassos);
  MaqConfigFlags(MaqLerFlags() | FLAG_PROG_TAMANHO);
  while(MaqLerFlags() & FLAG_PROG_TAMANHO);

  for(i = 0; i < ProgData.QtdPassos; i++) {
    // Espera terminar escrita anterior
    while(MaqLerFlags() & FLAG_PROG_GRAVAR);

    // Grava a nova posicao
    MaqGravarRegistrador(REG_PROG_INDICE, i);
    MaqGravarRegistrador(REG_PROG_AVANCO    ,  ProgData.Passos[i].Avanco);
    MaqGravarRegistrador(REG_PROG_PORTAS    ,  ProgData.Passos[i].Portas);
    MaqGravarRegistrador(REG_PROG_REPETICOES,  ProgData.Passos[i].Repeticoes);
    MaqGravarRegistrador(REG_PROG_FLAGS     , (ProgData.Passos[i].LigarPrensa > 0) ? FLAG_PROG_PRENSA_LIGAR : 0);
    MaqConfigFlags(MaqLerFlags() | FLAG_PROG_GRAVAR);
  }

  MaqGravarRegistrador(REG_PROG_FLAGS, (ProgData.ModoSingelo > 0) ? FLAG_PROG_PRENSA_SINGELO : 0);
  MaqConfigProdQtd(ProgData.Quantidade);
}

void CarregaComboProgramas(void)
{
  GtkWidget *obj = GTK_WIDGET(gtk_builder_get_object(builder, "cmbPrensaProg"));

// Carregamento dos programas cadastrados no MySQL no ComboBox.
  DB_Execute(&mainDB, 0, "select Nome from ProgPrensa order by ID");
  CarregaCombo(&mainDB, GTK_COMBO_BOX(obj),0, NULL);
}

int ProgPrensa_Init(void)
{
  CarregaComboProgramas();

  return 1;
}

// Logica da tela principal: Callback de botoes e demais funcoes

void cbProgPrensa_MesaSubir(GtkButton *button, gpointer user_data)
{
  atividade++;
  SetMaqStatus(MAQ_STATUS_MANUAL);
  MaqConfigFlags(MaqLerFlags() | FLAG_CMD_MESA_SUBIR);
}

void cbProgPrensa_MesaDescer(GtkButton *button, gpointer user_data)
{
  atividade++;
  SetMaqStatus(MAQ_STATUS_MANUAL);
  MaqConfigFlags(MaqLerFlags() | FLAG_CMD_MESA_DESCER);
}

void cbProgPrensa_MesaParar(GtkButton *button, gpointer user_data)
{
  MaqConfigFlags(MaqLerFlags() & (~(FLAG_CMD_MESA_DESCER | FLAG_CMD_MESA_SUBIR)));
}

void cbProgPrensa_AplanSubir(GtkButton *button, gpointer user_data)
{
  atividade++;
  SetMaqStatus(MAQ_STATUS_MANUAL);
  MaqConfigFlags(MaqLerFlags() | FLAG_CMD_APLAN_SUBIR);
}

void cbProgPrensa_AplanDescer(GtkButton *button, gpointer user_data)
{
  atividade++;
  SetMaqStatus(MAQ_STATUS_MANUAL);
  MaqConfigFlags(MaqLerFlags() | FLAG_CMD_APLAN_DESCER);
}

void cbProgPrensa_AplanParar(GtkButton *button, gpointer user_data)
{
  MaqConfigFlags(MaqLerFlags() & (~(FLAG_CMD_APLAN_DESCER | FLAG_CMD_APLAN_SUBIR)));
}

void cbProgPrensa_Parar(GtkButton *button, gpointer user_data)
{
  atividade++;
  printf("Saindo do modo automatico\n");
  MaqConfigModo(MAQ_MODO_MANUAL);

  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblExecProgMsg")), "Terminando...");
  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

void cbProgPrensa_Finalizar(GtkButton *button, gpointer user_data)
{
  atividade++;

  MaqGravarRegistrador(REG_PROG_FLAGS, MaqLerRegistrador(REG_PROG_FLAGS, 0) | FLAG_PROG_FINALIZAR);

  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblExecProgMsg")), "Terminando...");
  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

void cbProgPrensa_Produzir(GtkButton *button, gpointer user_data)
{
  if(!GetUserPerm(PERM_ACESSO_OPER))
    {
    MessageBox("Sem permissão para operar a máquina!");
    return;
    }

  CarregaComboProgramas();
  AbrirProgParam();
  WorkAreaGoTo(NTB_ABA_PROG_PARAM);
}

void cbDesbob(GtkToggleButton *togglebutton, gpointer user_data)
{
  if(gtk_toggle_button_get_active(togglebutton)) {
      MaqConfigFlags(MaqLerFlags() |   FLAG_CMD_DESBOB_LIGAR );
    } else {
      MaqConfigFlags(MaqLerFlags() & (~FLAG_CMD_DESBOB_LIGAR));
    }
}

void cbExecProgPassoProx(GtkButton *button, gpointer user_data)
{
  uint16_t flags = MaqLerRegistrador(REG_PROG_FLAGS, 0);
  MaqGravarRegistrador(REG_PROG_FLAGS, flags | FLAG_PROG_PASSO_PROXIMO);
}

void cbExecProgPassoAnterior(GtkButton *button, gpointer user_data)
{
  uint16_t flags = MaqLerRegistrador(REG_PROG_FLAGS, 0);
  MaqGravarRegistrador(REG_PROG_FLAGS, flags | FLAG_PROG_PASSO_ANTERIOR);
}

void cbExecProgRepetProx(GtkButton *button, gpointer user_data)
{
  uint16_t flags = MaqLerRegistrador(REG_PROG_FLAGS, 0);
  MaqGravarRegistrador(REG_PROG_FLAGS, flags | FLAG_PROG_REPET_PROXIMO);
}

void cbExecProgRepetAnterior(GtkButton *button, gpointer user_data)
{
  uint16_t flags = MaqLerRegistrador(REG_PROG_FLAGS, 0);
  MaqGravarRegistrador(REG_PROG_FLAGS, flags | FLAG_PROG_REPET_ANTERIOR);
}

/*** Logica para montar a janela de parametros do programa ***/

// Estrutura para implementar a lista ligada com os itens
typedef struct strObjList {
  int id;
  GtkWidget *obj;

  struct strObjList *next;
} tObjList;

// Estrutura passada como parametro para as funcoes de callback
// Contem os objetos e respectivos IDs para atualizacao do banco de dados
typedef struct {
  int id;
  GtkWidget *qtd;

  tObjList *lista_avanco;
  tObjList *lista_repeticoes;
} tProgParam;

tProgParam *ProgParam = NULL;

// Timer de producao
gboolean tmrExecProg(gpointer data)
{
  static int qtd = 0, iniciando = 1, progID = 0;
  char tmp[30], sql[300];
  int qtdprod;
  static GtkLabel *lblSaldo = NULL;

  if(iniciando) {
    iniciando = 0;
    ProgCarregarDados(TRUE);
    progID = *(int *)(data);

    sprintf(sql, "select NumCiclos, Nome from ProgPrensa where ID=%d", progID);
    DB_Execute(&mainDB, 0, sql);
    DB_GetNextRow(&mainDB, 0);

    qtd = atoi(DB_GetData(&mainDB, 0, 0));

    lblSaldo = GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaExecSaldo"));

    if(qtd > 0) {
      sprintf(tmp, "%d", qtd);
    } else {
      strcpy(tmp, "Sem Limite");
      gtk_label_set_text(lblSaldo, tmp);
    }

    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaExecTotal")), tmp);

    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblExecProgMsg")), DB_GetData(&mainDB, 0, 1));
  }

  qtdprod = MaqLerProdQtd(); // Retorna quantidade de pecas restantes.

  if((MaqLerEstado() & MAQ_MODO_MASK) == MAQ_MODO_MANUAL) {
    iniciando = 1;
    MaqConfigModo(MAQ_MODO_MANUAL);

    // Configura o estado da máquina
    SetMaqStatus(MAQ_STATUS_MANUAL);

    if(progID > 0 && qtdprod >= 0) {
      // Executa a instrução SQL necessária para atualizar a quantidade restante.
      sprintf(sql, "update ProgPrensa set NumCiclos=%d where ID=%d", qtdprod, progID);
      DB_Execute(&mainDB, 0, sql);
    }

    qtd    = 0;
    progID = 0;

    WorkAreaGoTo(MaqConfigCurrent->AbaHome);
    return FALSE;
  } else {
    if(qtd > 0) {
      sprintf(tmp, "%d", qtdprod);
      gtk_label_set_text(lblSaldo, tmp);
    }

    ProgCarregarDados(FALSE);
  }

  return TRUE;
}

void IniciarProducao(int id)
{
  ProgPrensa_Enviar(ProgPrensa_Carregar(id));

  WorkAreaGoTo(NTB_ABA_PRENSA_PROD);

  printf("Entrando no modo automatico\n");
  MaqConfigModo(MAQ_MODO_AUTO);

  tmrExecProg((gpointer)&id);
  g_timeout_add(1000, tmrExecProg, NULL);

  // Configura o estado da máquina
  SetMaqStatus(MAQ_STATUS_PRODUZINDO);

  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnPrensaExecParar"  )), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnExecProgFinalizar")), TRUE);
}

void SaindoProgParam(void)
{
  GtkContainer *container = GTK_CONTAINER(gtk_builder_get_object(builder, "tblProgParam"));
  GList *start, *lst = gtk_container_get_children(container);

  start = lst;
  while(lst) {
    gtk_container_remove(container, GTK_WIDGET(lst->data));
    lst = lst->next;
  }

  g_list_free(start);

  if(ProgParam != NULL) {
    while(ProgParam->lista_avanco != NULL) {
      tObjList *next = ProgParam->lista_avanco->next;
      free(ProgParam->lista_avanco);
      ProgParam->lista_avanco = next;
    }

    while(ProgParam->lista_repeticoes != NULL) {
      tObjList *next = ProgParam->lista_repeticoes->next;
      free(ProgParam->lista_repeticoes);
      ProgParam->lista_repeticoes = next;
    }

    free(ProgParam);
    ProgParam = NULL;
  }
}

void cbProgParamCancelar(GtkButton *button, gpointer user_data)
{
  WorkAreaGoTo(MaqConfigCurrent->AbaHome);
  SaindoProgParam();
}

void cbProgParamProduzir(GtkButton *button, gpointer user_data)
{
  int val;
  char sql[200];

  if(ProgParam != NULL) {
    tObjList   *item      = ProgParam->lista_avanco;

    while(item != NULL) {
      val = atoi(gtk_entry_get_text(GTK_ENTRY(item->obj))) * 100;
      if(val > 0) {
        sprintf(sql,"update ProgPrensaPassos set ProgPrensaAvanco=%d where ID=%d", val, item->id);
        DB_Execute(&mainDB, 0, sql);
      }

      item = item->next;
    }

    item = ProgParam->lista_repeticoes;

    while(item != NULL) {
      val = atoi(gtk_entry_get_text(GTK_ENTRY(item->obj)));
      if(val > 0) {
        sprintf(sql,"update ProgPrensaPassos set Repeticoes=%d where ID=%d", val, item->id);
        DB_Execute(&mainDB, 0, sql);
      }

      item = item->next;
    }

    if(ProgParam->qtd != NULL) {
      val = atoi(gtk_entry_get_text(GTK_ENTRY(ProgParam->qtd)));
      if(val >= 0) {
        sprintf(sql,"update ProgPrensa set NumCiclos=%d where ID=%d",
            val, ProgParam->id);
        DB_Execute(&mainDB, 0, sql);
      }
    }

    IniciarProducao(ProgParam->id);

    SaindoProgParam();
  }
}

void cbProgPadraoIniciar(GtkButton *button, gpointer user_data)
{
  int qtd, avanco, passo, tamanho, usar_faca, modo_singelo = 0, porta_prensa = 0;

  avanco      = atoi(gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoAvanco" )))) * 10;
  passo       = atoi(gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoPasso"  )))) * 10;
  tamanho     = atoi(gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoTamanho")))) * 10;
  qtd         = atoi(gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoQtd"    ))));
  usar_faca   = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rbtProgPadraoComFaca")));

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rbtProgPadraoSingelo")))) {
    modo_singelo = 1;
  }

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rbtProgPadraoComPrensa")))) {
    porta_prensa = PROG_MASK_PRENSA;
  }

  if       (avanco  <= 0) {
    MessageBox("O avanço deve ser maior que zero!");
  } else if(usar_faca && passo   <= 0) {
    MessageBox("O passo deve ser maior que zero!");
  } else if(usar_faca && tamanho <= 0) {
    MessageBox("O tamanho deve ser maior que zero!");
  } else if(usar_faca && tamanho <  passo) {
    MessageBox("O tamanho deve ser maior que o passo!");
  } else if(usar_faca && (avanco % passo) != 0) {
    MessageBox("O avanço deve ser múltiplo do passo!");
  } else {
    char sql[300];
    int passos = tamanho / avanco, correcao_passo = 0;
    int passo_final = tamanho - (passos * avanco);

    // Primeiro devemos excluir o programa anterior
    DB_Execute(&mainDB, 0, "delete from ProgPrensaPassos where ProgPrensaID=1");

    if(usar_faca) {
      if(passo_final == 0) {
        passos--;
        passo_final = avanco;
      } else if((passo_final % passo) != 0) {
        passos--;
        correcao_passo = (passo_final % passo) / 2;
      }

      if(passos > 0) {
        sprintf(sql, "insert into ProgPrensaPassos (ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas, Repeticoes) values (1, %d, %d, %d)",
            avanco, porta_prensa, passos);
        DB_Execute(&mainDB, 0, sql);
      }

      if(passo_final == avanco || !(passo_final % passo)) {
        sprintf(sql, "insert into ProgPrensaPassos (ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas) values (1, %d, %d)",
            passo_final, porta_prensa | PROG_MASK_FACA);
        DB_Execute(&mainDB, 0, sql);
      } else {
        sprintf(sql, "insert into ProgPrensaPassos (ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas) values (1, %d, %d)",
            passo_final - correcao_passo, PROG_MASK_FACA);
        DB_Execute(&mainDB, 0, sql);

        sprintf(sql, "insert into ProgPrensaPassos (ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas) values (1, %d, %d)",
            avanco + correcao_passo, porta_prensa);
        DB_Execute(&mainDB, 0, sql);
      }
    } else {
      sprintf(sql, "insert into ProgPrensaPassos (ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas) values (1, %d, %d)",
          avanco, porta_prensa);
      DB_Execute(&mainDB, 0, sql);
    }

    sprintf(sql, "update ProgPrensa set NumCiclos=%d, ModoSingelo=%d where ID=1", qtd, modo_singelo);
    DB_Execute(&mainDB, 0, sql);

    IniciarProducao(1);
  }
}

void cbProgPadraoContinuar(GtkButton *button, gpointer user_data)
{
  IniciarProducao(1);
}

void cbProgPadraoCancelar(GtkButton *button, gpointer user_data)
{
  WorkAreaGoTo(MaqConfigCurrent->AbaHome);
}

void AbrirProgramaPadrao(void)
{
  char tmp[100];
  int modo_singelo, ciclos, avanco, passo = 0, tamanho = 0, usar_faca = 0, usar_prensa = 0;

  SaindoProgParam();

  DB_Execute(&mainDB, 0, "select NumCiclos, ModoSingelo from ProgPrensa where ID=1");
  DB_GetNextRow(&mainDB, 0);

  ciclos = atoi(DB_GetData(&mainDB, 0, 0));
  sprintf(tmp, "%d", ciclos);
  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoQtd")), tmp);

  modo_singelo = atoi(DB_GetData(&mainDB, 0, 1));

  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoPasso" )), "40");
  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoAvanco")), "0");

  DB_Execute(&mainDB, 0, "select ProgPrensaAvanco, ProgPrensaPortas, Repeticoes from ProgPrensaPassos where ProgPrensaID=1");
  while(DB_GetNextRow(&mainDB, 0) > 0) {
    avanco = atoi(DB_GetData(&mainDB, 0, 0)) / 10;
    tamanho += avanco * atoi(DB_GetData(&mainDB, 0, 2));

    if(atoi(DB_GetData(&mainDB, 0, 1)) & PROG_MASK_FACA) {
      usar_faca = 1;
    }

    if(atoi(DB_GetData(&mainDB, 0, 1)) & PROG_MASK_PRENSA) {
      usar_prensa = 1;
    }

    if(passo == 0) {
      // Se for o primeiro passo, carrega o avanco no campo correspondente...
      sprintf(tmp, "%d", avanco);
      gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoAvanco")), tmp);
    }

    passo++;
  }

  sprintf(tmp, "%d", tamanho);
  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entProgPadraoTamanho")), tmp);

  char *radio;
  if(usar_faca) {
    radio = "rbtProgPadraoComFaca";
  } else {
    radio = "rbtProgPadraoSemFaca";
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, radio)), TRUE);

  if(usar_prensa) {
    radio = "rbtProgPadraoComPrensa";
  } else {
    radio = "rbtProgPadraoSemPrensa";
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, radio)), TRUE);

  if(modo_singelo) {
    radio = "rbtProgPadraoSingelo";
  } else {
    radio = "rbtProgPadraoContinuo";
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, radio)), TRUE);

  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProg")), 1);
}

void AbrirProgParam(void)
{
  char sql[200], tmp[60];
  int id, num_ciclos, pedir_ciclos, tam = 0, i = 0, idx = 0;
  GtkWidget *tbl, *obj;
  GtkComboBox *combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "cmbPrensaProg"));

  // Primeiro chama a funcao que descarta os objetos atuais da tela de parametros
  SaindoProgParam();

  if(gtk_combo_box_get_active(combo) < 0) {
    return; // Sem item ativo.
  }

  sprintf(sql, "select ID, NumCiclos, PedirNumCiclos from ProgPrensa where LINHA='%s' and MAQUINA='%s' and nome='%s'",
      MAQ_LINHA, MAQ_MAQUINA, LerComboAtivo(combo));
  DB_Execute(&mainDB, 0, sql);
  DB_GetNextRow(&mainDB, 0);

  id           = atoi(DB_GetData(&mainDB, 0, 0));
  num_ciclos   = atoi(DB_GetData(&mainDB, 0, 1));
  pedir_ciclos = atoi(DB_GetData(&mainDB, 0, 2));

  if(id == 1) { // Condicao especial: Programa padrao!
    AbrirProgramaPadrao();
  } else {
    if(pedir_ciclos) {
      tam++;
    }

    tbl = GTK_WIDGET(gtk_builder_get_object(builder, "tblProgParam"));

    sprintf(sql, "select ID, TextoAvanco, ProgPrensaAvanco from ProgPrensaPassos where ProgPrensaID=%d and PedirAvanco=1 order by ID", id);
    DB_Execute(&mainDB, 0, sql);
    if(DB_GetNextRow(&mainDB, 0) <= 0) {
      idx++;
    }

    tam += DB_GetCount(&mainDB, 0);

    sprintf(sql, "select ID, TextoRepeticoes, Repeticoes from ProgPrensaPassos where ProgPrensaID=%d and PedirRepeticoes=1 order by ID", id);
    DB_Execute(&mainDB, 1, sql);
    if(DB_GetNextRow(&mainDB, 1) <= 0) {
      idx++;
    }

    tam += DB_GetCount(&mainDB, 1);

    // Ponteiro para uma lista de IDs para a futura inserção / alteração dos dados no banco.
    // A logica a seguir aloca o ponteiro para a estrutura, o qual deve ser desalocado pela
    // função de callback do sinal destroy da janela.
    ProgParam = (tProgParam *)malloc(sizeof(tProgParam));
    memset(ProgParam, 0, sizeof(tProgParam));

    ProgParam->id = id;

    if(tam > 0) {
      gtk_table_resize(GTK_TABLE(tbl), tam, 2);

      if(pedir_ciclos) {
        obj = gtk_label_new("Quantidade:");
        gtk_buildable_set_name(GTK_BUILDABLE(obj), "lblProgParamQtd");
        gtk_table_attach_defaults(GTK_TABLE(tbl), obj, 0, 1, 0, 1);

        sprintf(tmp, "%d", num_ciclos);
        obj = gtk_entry_new_with_max_length(10); // Tamanho de um int
        gtk_buildable_set_name(GTK_BUILDABLE(obj), "entProgParamQtd");
        gtk_entry_set_text(GTK_ENTRY(obj), tmp);
        g_signal_connect ((gpointer) obj, "focus-in-event", G_CALLBACK(cbFocusIn), NULL);

        ProgParam->qtd = obj;

        gtk_table_attach_defaults(GTK_TABLE(tbl), obj, 1, 2, 0, 1);

        i++;
      }

      tObjList **ObjList = &(ProgParam->lista_avanco);

      // Estranho mas esta certo! inicializa e checa idx mas incrementa i.
      // idx eh o indice da consulta sql. idx = 0 para avancos e idx = 1 para repeticoes
      // i eh o contador de controle para anexar na tabela do gtk
      // idx eh incrementado diretamente no codigo, quando acabam os registros da consulta
      for(idx = 0; idx < 2; i++) {
        char *id = DB_GetData(&mainDB, idx, 0);
        if(id != NULL) {
          *ObjList = (tObjList *)(malloc(sizeof(tObjList)));
          (*ObjList)->id = atoi(id);

          Asc2Utf((unsigned char *)DB_GetData(&mainDB, idx, 1), (unsigned char *)tmp);
          obj = gtk_label_new(tmp);

          sprintf(tmp, "lblProgParam%02d", i);
          gtk_buildable_set_name(GTK_BUILDABLE(obj), tmp);

          gtk_table_attach_defaults(GTK_TABLE(tbl), obj, 0, 1, i, i+1);

          sprintf(tmp, "%d", atoi(DB_GetData(&mainDB, idx, 2)) / (idx == 0 ? 100 : 1));

          obj = gtk_entry_new_with_max_length(10); // Tamanho de um int
          gtk_entry_set_text(GTK_ENTRY(obj), tmp);

          sprintf(tmp, "entProgParam%02d", i);
          gtk_buildable_set_name(GTK_BUILDABLE(obj), tmp);

          g_signal_connect ((gpointer) obj, "focus-in-event", G_CALLBACK(cbFocusIn), NULL);

          (*ObjList)->obj = obj;

          gtk_table_attach_defaults(GTK_TABLE(tbl), obj, 1, 2, i, i+1);

          (*ObjList)->next = NULL;
          ObjList = &(*ObjList)->next;

          DB_GetNextRow(&mainDB, idx);
        } else {
          ObjList = &(ProgParam->lista_repeticoes);
          idx++;
        }
//        if(DB_GetNextRow(&mainDB, idx) <= 0) {
//          ObjList = &(ProgParam->lista_repeticoes);
//          idx++;
//        }
      }

      gtk_widget_show_all(tbl);
    }

    gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProg")), 0);
  }
}

/*** Fim da logica para montar a janela de parametros do programa ***/

void cbProgramaSelecionado(GtkComboBox *combobox, gpointer user_data)
{
  AbrirProgParam();
}

extern int idUser; // Indica usuário que logou se for diferente de zero.

void LoadIntoPixbuf(GdkPixbuf *pb, char *file, gint x, gint y, gdouble scale_x, gdouble scale_y, gint refpos);

struct strImgIHM {
  char *arq;
  int posx, posy;
  unsigned int scalex, scaley;
};

struct strTelaIHM {
  GdkPixbuf *pb;
  unsigned int offset;
  struct strCoordIHM *coord;
};

void cbManAplanAvancar(void)
{
}

void cbManAplanRecuar(void)
{
}

void cbManAplanAbrir(void)
{
}

void cbManAplanSubir(void)
{
}

void cbManAplanExpandir(void)
{
}

void cbManPrsLigar(void)
{
}

void cbManAplanRetrair(void)
{
}

void cbManAplanDescer(void)
{
}

void cbManAplanFechar(void)
{
}

struct strCoordIHM {
  unsigned int xpos, ypos, xsize, ysize;
  void (*fnc)();
  char *img;
} *CurrentLstCoord, lst_coord[2][2][2][10] = {
    // Posição 000 - Tampa fechada, extensão abaixada, prolongamento recuado.
    [0][0][0] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  70, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 165, 70, 55, cbManAplanSubir   , "images/cmd-aplan-ext-subir.png"    },
    {  45, 245, 70, 55, cbManAplanExpandir, "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 001 - Tampa fechada, extensão abaixada, prolongamento avançado.
    [0][0][1] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  70, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 165, 70, 55, cbManAplanSubir   , "images/cmd-aplan-ext-subir.png"    },
    {  45, 245, 70, 55, cbManAplanRetrair , "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 010 - Tampa fechada, extensão levantada, prolongamento recuado.
    [0][1][0] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  70, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 215, 70, 55, cbManAplanDescer  , "images/cmd-aplan-ext-descer.png"   },
    {   0, 175, 70, 55, cbManAplanExpandir, "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 011 - Tampa fechada, extensão levantada, prolongamento avançado.
    [0][1][1] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  70, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 215, 70, 55, cbManAplanDescer  , "images/cmd-aplan-ext-descer.png"   },
    {   0, 175, 70, 55, cbManAplanRetrair , "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 100 - Tampa aberta, extensão abaixada, prolongamento recuado.
    [1][0][0] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  40, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 315,  40, 70, 55, cbManAplanFechar  , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 165, 70, 55, cbManAplanSubir   , "images/cmd-aplan-ext-subir.png"    },
    {  45, 245, 70, 55, cbManAplanExpandir, "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 101 - Tampa aberta, extensão abaixada, prolongamento avançado.
    [1][0][1] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  40, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 315,  40, 70, 55, cbManAplanFechar  , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 165, 70, 55, cbManAplanSubir   , "images/cmd-aplan-ext-subir.png"    },
    {  45, 245, 70, 55, cbManAplanRetrair , "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 110 - Tampa aberta, extensão levantada, prolongamento recuado.
    [1][1][0] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  40, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 315,  40, 70, 55, cbManAplanFechar  , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 215, 70, 55, cbManAplanDescer  , "images/cmd-aplan-ext-descer.png"   },
    {   0, 175, 70, 55, cbManAplanExpandir, "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },

    // Posição 111 - Tampa aberta, extensão levantada, prolongamento avançado.
    [1][1][1] = {
    { 415,  85, 70, 55, cbManAplanAvancar , "images/cmd-aplan-perfil-avanca.png"},
    { 135,  85, 70, 55, cbManAplanRecuar  , "images/cmd-aplan-perfil-recua.png" },
    { 235,  40, 70, 55, cbManAplanAbrir   , "images/cmd-aplan-tampa-abrir.png"  },
    { 315,  40, 70, 55, cbManAplanFechar  , "images/cmd-aplan-tampa-abrir.png"  },
    { 110, 215, 70, 55, cbManAplanDescer  , "images/cmd-aplan-ext-descer.png"   },
    {   0, 175, 70, 55, cbManAplanRetrair , "images/cmd-aplan-ext-expandir.png" },
    { 582,   7, 70, 55, cbManPrsLigar     , "images/cmd-prensa-ligar.png"       },
    { 0, 0, 0, 0, NULL } },
};

void CriarTelaIHM(struct strTelaIHM *tela, unsigned int offset, struct strImgIHM *lst_img, struct strCoordIHM *lst_coord)
{
//  char arq[50];
//  static unsigned int n = 0;

#ifdef DEBUG_CARREGAR_TELA
  tela->offset = 0;
  tela->coord  = lst_coord;

  sprintf(arq, "images/maq.%d.png", n++);
  tela->pb = gdk_pixbuf_new_from_file(arq, NULL);
#else
  GdkPixbuf *pb;
//  GdkPixbuf *copy_pb;
  unsigned int i = 0;

  tela->offset = offset;
  tela->coord  = lst_coord;

  tela->pb = gdk_pixbuf_new_from_file_at_scale(lst_img[0].arq,
      lst_img[0].posx + 2*tela->offset, lst_img[0].posy + 2*tela->offset,
      FALSE, NULL);

// Carrega a nova imagem
  pb = gdk_pixbuf_new_from_file_at_scale(lst_img[0].arq,
      lst_img[0].posx, lst_img[0].posy,
      FALSE, NULL);

// Agrega as duas imagens e remove a referencia a nova imagem
  gdk_pixbuf_composite(pb, tela->pb, tela->offset, tela->offset,
      lst_img[0].posx, lst_img[0].posy, tela->offset, tela->offset, 1, 1,
      GDK_INTERP_BILINEAR, 255);
  g_object_unref(pb);

  while(lst_img[++i].arq != NULL) {
    LoadIntoPixbuf(tela->pb, lst_img[i].arq, lst_img[i].posx + tela->offset, lst_img[i].posy + tela->offset, lst_img[i].scalex, lst_img[i].scaley, LOADPB_REFPOS_DEFAULT);
  }

  for(i=0; tela->coord[i].img != NULL; i++) {
    LoadIntoPixbuf(tela->pb, tela->coord[i].img, tela->coord[i].xpos + tela->offset, tela->coord[i].ypos + tela->offset, 1, 1, LOADPB_REFPOS_UP | LOADPB_REFPOS_LEFT);
  }

  // Recorta pixbuf para o tamanho da tela e copia para um novo
  pb = gdk_pixbuf_new_subpixbuf(tela->pb, tela->offset, tela->offset, lst_img[0].posx, lst_img[0].posy);
//  copy_pb = gdk_pixbuf_copy(pb);

  // Remove referências dos pixbufs antigos e aponta a tela para o novo
//  g_object_unref(pb);
//  g_object_unref(tela->pb);
//  tela->pb = copy_pb;

//  sprintf(arq, "maq.%d.png", n++);
//  gdk_pixbuf_save(pb, arq, "png", NULL, NULL);
//  tela->pb = pb;
#endif
}

void DesenharTelaIHM(GtkWidget *widget, struct strTelaIHM *tela)
{
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  CurrentLstCoord = tela->coord;

  GtkStyle *style = gtk_widget_get_style(widget);
  gdk_draw_pixbuf(gtk_widget_get_window(widget),
                  style->fg_gc[gtk_widget_get_state(widget)],
                  tela->pb,
                  tela->offset, tela->offset,
                  0, 0, allocation.width, allocation.height,
                  GDK_RGB_DITHER_NONE, 0, 0);
}
/*
gboolean cbMaquinaButtonPress(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  unsigned int i;

  if(event->type == GDK_BUTTON_PRESS && CurrentLstCoord != NULL){// && MaqPronta()) {
    for(i=0; CurrentLstCoord[i].fnc != NULL; i++) {
      if(event->x >= CurrentLstCoord[i].xpos &&
         event->y >= CurrentLstCoord[i].ypos &&
         event->x <= CurrentLstCoord[i].xpos + CurrentLstCoord[i].xsize &&
         event->y <= CurrentLstCoord[i].ypos + CurrentLstCoord[i].ysize) {
        (CurrentLstCoord[i].fnc)();
        break;
      }
    }
  } else if (event->type == GDK_BUTTON_RELEASE) {
    MaqAplanManual(MAQ_APLAN_PARAR);
  }

  return TRUE;
}
*/
gboolean cbDesenharPrensa(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  unsigned int tampa, ext, prol, offset = 200;
  static int first = 1, flags = 0;
  static struct strTelaIHM maq[2][2][2]; // 3 movimentos, 2 estados para cada um

  if(widget == NULL) {
    widget = GTK_WIDGET(gtk_builder_get_object(builder, "dwaPrensa"));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook1")), 1);
  }

  if(first) {
    first = 0;
    CurrentLstCoord = NULL;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    // Imagem 000 - Tampa fechada, extensão abaixada, prolongamento recuado.
    CriarTelaIHM(&maq[0][0][0], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-baixo.png"   , 148, 209, 1, 1 },
                              { "images/maq-aplan-ext-baixo.png"    , 156, 184, 1, 1 },
                              { "images/maq-aplan-tampa-fechada.png", 205, 136, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[0][0][0]
        );

    // Imagem 001 - Tampa fechada, extensão abaixada, prolongamento avançado.
    CriarTelaIHM(&maq[0][0][1], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-baixo.png"   , 108, 249, 1, 1 },
                              { "images/maq-aplan-ext-baixo.png"    , 156, 184, 1, 1 },
                              { "images/maq-aplan-tampa-fechada.png", 205, 136, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[0][0][1]
        );

    // Imagem 010 - Tampa fechada, extensão levantada, prolongamento recuado.
    CriarTelaIHM(&maq[0][1][0], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-cima.png"    , 113, 196, 1, 1 },
                              { "images/maq-aplan-ext-cima.png"     , 129, 186, 1, 1 },
                              { "images/maq-aplan-tampa-fechada.png", 205, 136, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[0][1][0]
        );

    // Imagem 011 - Tampa fechada, extensão levantada, prolongamento avançado.
    CriarTelaIHM(&maq[0][1][1], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-cima.png"    ,  73, 196, 1, 1 },
                              { "images/maq-aplan-ext-cima.png"     , 129, 186, 1, 1 },
                              { "images/maq-aplan-tampa-fechada.png", 205, 136, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[0][1][1]
        );

    // Imagem 100 - Tampa aberta, extensão abaixada, prolongamento recuado.
    CriarTelaIHM(&maq[1][0][0], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-baixo.png"   , 148, 209, 1, 1 },
                              { "images/maq-aplan-ext-baixo.png"    , 156, 184, 1, 1 },
                              { "images/maq-aplan-tampa-aberta.png" , 205, 106, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[1][0][0]
        );

    // Imagem 101 - Tampa aberta, extensão abaixada, prolongamento avançado.
    CriarTelaIHM(&maq[1][0][1], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-baixo.png"   , 108, 249, 1, 1 },
                              { "images/maq-aplan-ext-baixo.png"    , 156, 184, 1, 1 },
                              { "images/maq-aplan-tampa-aberta.png" , 205, 106, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[1][0][1]
        );

    // Imagem 110 - Tampa aberta, extensão levantada, prolongamento recuado.
    CriarTelaIHM(&maq[1][1][0], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-cima.png"    , 113, 196, 1, 1 },
                              { "images/maq-aplan-ext-cima.png"     , 129, 186, 1, 1 },
                              { "images/maq-aplan-tampa-aberta.png" , 205, 106, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[1][1][0]
        );

    // Imagem 111 - Tampa aberta, extensão levantada, prolongamento avançado.
    CriarTelaIHM(&maq[1][1][1], offset,
        (struct strImgIHM[]){ { "images/bg01.png", allocation.width, allocation.height, 1, 1 },
                              { "images/maq-aplan-corpo.png"        , 170, 153, 1, 1 },
                              { "images/maq-aplan-prol-cima.png"    ,  73, 196, 1, 1 },
                              { "images/maq-aplan-ext-cima.png"     , 129, 186, 1, 1 },
                              { "images/maq-aplan-tampa-aberta.png" , 205, 106, 1, 1 },
                              { "images/maq-prs-corpo.png"          , 520, -42, 1, 1 },
                              { "images/maq-prs-martelo.png"        , 563, -42, 1, 1 },
                              { "images/maq-prs-cobertura.png"      , 554, -52, 1, 1 },
                              { NULL, 0, 0, 0, 0 } },
        lst_coord[1][1][1]
        );
  }

  if(data != NULL)
    flags = *(unsigned int *)(data);

  if(idUser) {
    tampa = (flags >> 2) & 1;
    ext   = (flags >> 1) & 1;
    prol  = (flags >> 0) & 1;

    DesenharTelaIHM(widget, &maq[tampa][ext][prol]);
  }

  return TRUE;
}

// Atualizacao da tela da Prensa de Mezanino
void PrensaMezanino_Update(void)
{
  int current_flags;
  static int last_flags = -1;

  // Leitura das flags de estado da máquina para atualização da imagem na tela principal
  current_flags = (MaqLerEstado() >> 10) & 0x7;
  if(last_flags != current_flags) {
    last_flags = current_flags;
    printf("current_flags = %d\n", current_flags);
    cbDesenharPrensa(NULL, NULL, (gpointer)(&current_flags));
  }
}

/*** Controle da Tela de Criacao / Edicao de Programas ***/

tProgData ProgData;

static void RemovePasso(tProgData *data, int index)
{
  // Se ponteiro nulo ou indice invalido, retorna...
  if(data == NULL || index < 0 || index >= data->QtdPassos || data->QtdPassos == 1) return;

  memcpy(&(data->Passos[index]), &(data->Passos[index + 1]),
      sizeof(data->Passos[index]) * (data->QtdPassos - index - 1));

  data->QtdPassos--;
}

static void NovoPasso(tProgData *data, int index)
{
  // Se ponteiro nulo ou indice invalido, retorna...
  if(data == NULL || index < 0 || index > data->QtdPassos || data->QtdPassos >= MAX_PROG_PASSOS) return;

  int n;
  for(n = data->QtdPassos; n > index; n--) {
    memcpy(&(data->Passos[n]), &(data->Passos[n - 1]), sizeof(data->Passos[n]));
  }

  memset(&(data->Passos[index]), 0, sizeof(data->Passos[index]));

  data->Passos[index].LigarPrensa = 1;
  data->Passos[index].Repeticoes  = 1;

  data->QtdPassos++;
}

static tProgData NovoPrograma(void)
{
  tProgData novo;
  memset(&novo, 0, sizeof(tProgData));

  NovoPasso(&novo, 0);

  return novo;
}

void SalvaPassoPrograma(void)
{
  int index = atoi(LerValorWidget("lblPrensaProgPassoNum")) - 1;

  ProgData.Passos[index].Avanco     = (int)(atof(LerValorWidget("spbPrensaProgAvanco"    )) * 10);
  ProgData.Passos[index].Repeticoes =       atoi(LerValorWidget("spbPrensaProgRepeticoes"));

  ProgData.Passos[index].Portas  = atoi(LerValorWidget("ckbPrensaProgPorta1"));
  ProgData.Passos[index].Portas |= atoi(LerValorWidget("ckbPrensaProgPorta2")) << 1;
  ProgData.Passos[index].Portas |= atoi(LerValorWidget("ckbPrensaProgPorta3")) << 2;

  ProgData.Passos[index].PedirAvanco = atoi(LerValorWidget("rbtPrensaProgPedirAvancoSim"));

  strncpy(ProgData.Passos[index].TextoAvanco, LerValorWidget("entPrensaProgTextoAvanco"), MAX_PROG_TEXTO);
  ProgData.Passos[index].TextoAvanco[MAX_PROG_TEXTO] = 0;

  ProgData.Passos[index].PedirRepeticoes = atoi(LerValorWidget("rbtPrensaProgPedirRepSim"));

  strncpy(ProgData.Passos[index].TextoRepeticoes, LerValorWidget("entPrensaProgTextoRep"), MAX_PROG_TEXTO);
  ProgData.Passos[index].TextoRepeticoes[MAX_PROG_TEXTO] = 0;
}

void SalvaPrograma(void)
{
  int i;
  char sql[500];

  // Primeiro precisamos carregar os valores fornecidos pelo usuario para a estrutura do programa
  strncpy(ProgData.Nome, LerValorWidget("entPrensaProgNome"), MAX_PROG_TEXTO);
  ProgData.Nome[MAX_PROG_TEXTO] = 0;

  ProgData.ModoSingelo     = atoi(LerValorWidget("rbtPrensaProgModoSingelo"));
  ProgData.PedirQuantidade = atoi(LerValorWidget("rbtPrensaProgPedirQtdSim"));
  ProgData.Quantidade      = atoi(LerValorWidget("entPrensaProgQtd"        ));

  // Agora salva os dados no banco
  if(ProgData.ID == 0) { // Programa novo!
    sprintf(sql, "insert into ProgPrensa (LINHA, MAQUINA, Nome, ModoSingelo, NumCiclos, PedirNumCiclos) "
        "values ('%s', '%s', '%s', %d, %d, %d)",
        MAQ_LINHA, MAQ_MAQUINA, ProgData.Nome, ProgData.ModoSingelo, ProgData.Quantidade, ProgData.PedirQuantidade);
    DB_Execute(&mainDB, 0, sql);

    DB_Execute(&mainDB, 0, "select max(ID) from ProgPrensa");
    DB_GetNextRow(&mainDB, 0);

    ProgData.ID = atoi(DB_GetData(&mainDB, 0, 0));
  } else { // Programa existente
    sprintf(sql, "update ProgPrensa set Nome='%s', ModoSingelo=%d, NumCiclos=%d, PedirNumCiclos=%d where ID=%d",
        ProgData.Nome, ProgData.ModoSingelo, ProgData.Quantidade, ProgData.PedirQuantidade, ProgData.ID);
    DB_Execute(&mainDB, 0, sql);
  }

  // Exclui os passos atuais
  sprintf(sql, "delete from ProgPrensaPassos where ProgPrensaID=%d", ProgData.ID);
  DB_Execute(&mainDB, 0, sql);

  // Agora faz um loop entre os passos, salvando um a um...
  for(i = 0; i < ProgData.QtdPassos; i++) {
    sprintf(sql, "insert into ProgPrensaPassos(ProgPrensaID, ProgPrensaAvanco, ProgPrensaPortas, Repeticoes, "
        "PedirAvanco, TextoAvanco, PedirRepeticoes, TextoRepeticoes) values (%d, %d, %d, %d, %d, '%s', %d, '%s')",
        ProgData.ID, ProgData.Passos[i].Avanco, ProgData.Passos[i].Portas, ProgData.Passos[i].Repeticoes,
        ProgData.Passos[i].PedirAvanco    , ProgData.Passos[i].TextoAvanco,
        ProgData.Passos[i].PedirRepeticoes, ProgData.Passos[i].TextoRepeticoes);
    DB_Execute(&mainDB, 0, sql);
  }
}

void CarregaPassoPrograma(int index)
{
  char tmp[100];

  if(index >= ProgData.QtdPassos || index < 0) {
    return; // Passo inexistente!
  }

  sprintf(tmp, "%d de %d", index + 1, ProgData.QtdPassos);
  GravarValorWidget("lblPrensaProgPassoNum", tmp);

  sprintf(tmp, "%.1f", (float)(ProgData.Passos[index].Avanco)/10.0);
  GravarValorWidget("spbPrensaProgAvanco", tmp);

  sprintf(tmp, "%d", ProgData.Passos[index].Repeticoes);
  GravarValorWidget("spbPrensaProgRepeticoes", tmp);

  sprintf(tmp, "%d", (ProgData.Passos[index].Portas & 0x01) != 0);
  GravarValorWidget("ckbPrensaProgPorta1", tmp);

  sprintf(tmp, "%d", (ProgData.Passos[index].Portas & 0x02) != 0);
  GravarValorWidget("ckbPrensaProgPorta2", tmp);

  sprintf(tmp, "%d", (ProgData.Passos[index].Portas & 0x04) != 0);
  GravarValorWidget("ckbPrensaProgPorta3", tmp);

  GravarValorWidget(ProgData.Passos[index].PedirAvanco ?
      "rbtPrensaProgPedirAvancoSim" : "rbtPrensaProgPedirAvancoNao", "1");

  GravarValorWidget("entPrensaProgTextoAvanco", ProgData.Passos[index].TextoAvanco);

  GravarValorWidget(ProgData.Passos[index].PedirRepeticoes ?
      "rbtPrensaProgPedirRepSim" : "rbtPrensaProgPedirRepNao", "1");

  GravarValorWidget("entPrensaProgTextoRep", ProgData.Passos[index].TextoRepeticoes);

  // Atualiza o estados dos controles, nao permitindo acoes invalidas

  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnPrensaProgPassoAnt"      )),
      (index == 0                     ) ? FALSE : TRUE);

  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnPrensaProgPassoProx"     )),
      (index == ProgData.QtdPassos - 1) ? FALSE : TRUE);

  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnPrensaProgPassoAdicionar")),
      (ProgData.QtdPassos == MAX_PROG_PASSOS) ? FALSE : TRUE);

  gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "btnPrensaProgPassoRemover"  )),
      (ProgData.QtdPassos <= 1              ) ? FALSE : TRUE);
}

void CarregaPrograma(unsigned int ID)
{
  char *radio;
  char tmp[MAX_PROG_TEXTO + 1];
  ProgData = (ID == 0 ? NovoPrograma() : ProgPrensa_Carregar(ID));

  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entPrensaProgNome")), ProgData.Nome);

  sprintf(tmp, "%d", ProgData.Quantidade);
  gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entPrensaProgQtd" )), tmp);

  if(ProgData.PedirQuantidade != 0) {
    radio = "rbtPrensaProgPedirQtdSim";
  } else {
    radio = "rbtPrensaProgPedirQtdNao";
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, radio)), TRUE);

  if(ProgData.ModoSingelo != 0) {
    radio = "rbtPrensaProgModoSingelo";
  } else {
    radio = "rbtPrensaProgModoContinuo";
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, radio)), TRUE);
}

void CarregaListaProgramas(GtkWidget *tvw)
{
  int i;
  const int tam = 4;
  char *valores[tam+1];

  valores[tam] = NULL;

  // Exclui os itens atuais
  TV_Limpar(tvw);

  // Carrega os Programas
  DB_Execute(&mainDB, 0,
      "select ID, Nome, case ModoSingelo when 0 then 'Nao' else 'Sim' end as Singelo, NumCiclos from ProgPrensa where ID!=1 order by Nome");

  while(DB_GetNextRow(&mainDB, 0)>0) {
    valores[0] = DB_GetData(&mainDB, 0, 0);
    for(i = 1; i<tam; i++) {
      valores[i] = DB_GetData(&mainDB, 0, i);
      if(valores[i] == NULL)
        valores[i] = "";
    }

    TV_Adicionar(tvw, valores);
  }

//  ConfigBotoesPrograma(GTK_WIDGET(tvw), FALSE);
}

void cbPrensaProgPassosAnt(GtkButton *button, gpointer user_data)
{
  SalvaPassoPrograma();

  // Remove 2 pois passo exibido parte de 1 mas indice parte de 0.
  CarregaPassoPrograma(atoi(gtk_label_get_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaProgPassoNum")))) - 2);
}

void cbPrensaProgPassosProx(GtkButton *button, gpointer user_data)
{
  SalvaPassoPrograma();

  // Nao incrementa pois passo exibido parte de 1 mas indice parte de 0.
  CarregaPassoPrograma(atoi(gtk_label_get_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaProgPassoNum")))));
}

void cbPrensaProgPassosAdicionar(GtkButton *button, gpointer user_data)
{
  if(ProgData.QtdPassos < MAX_PROG_PASSOS) {
    // Primeiro salva o passo atual
    SalvaPassoPrograma();

    // Apesar de o numero do passo na tela iniciar de 1, nao decrementa o index
    // pois queremos adicionar o novo passo a seguir da posicao atual.
    int index = atoi(gtk_label_get_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaProgPassoNum"))));

    NovoPasso(&ProgData, index);
    CarregaPassoPrograma(index);
  }
}

void cbPrensaProgPassosRemover(GtkButton *button, gpointer user_data)
{
  if(ProgData.QtdPassos > 1) {
    int index = atoi(gtk_label_get_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPrensaProgPassoNum")))) - 1;

    RemovePasso(&ProgData, index);

    if(index == ProgData.QtdPassos) {
      index--;
    }

    CarregaPassoPrograma(index);
  }
}

void cbPrensaProgPassosVoltar(GtkButton *button, gpointer user_data)
{
  SalvaPassoPrograma();

  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_PROGRAMA);
}

void cbPrensaProgAbrirPassos(GtkButton *button, gpointer user_data)
{
  CarregaPassoPrograma(0);
  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_PASSOS);
}

void cbPrensaProgSalvar(GtkButton *button, gpointer user_data)
{
  SalvaPrograma();
  CarregaListaProgramas(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")));
  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_LISTA);
}

void cbPrensaProgCancelar(GtkButton *button, gpointer user_data)
{
  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_LISTA);
}

void cbNovoPrograma(GtkButton *button, gpointer user_data)
{
  CarregaPrograma(0);

  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_PROGRAMA);
}

void cbEditarPrograma(GtkButton *button, gpointer user_data)
{
  char tmp[10];
  TV_GetSelected(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")), 0, tmp);
  CarregaPrograma((unsigned int)atoi(tmp));

  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_PROGRAMA);
}

void cbExcluirPrograma(GtkButton *button, gpointer user_data)
{
  int id;
  char tmp[MAX_PROG_TEXTO + 1];

  TV_GetSelected(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")), 0, tmp);
  id = atoi(tmp);

  TV_GetSelected(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")), 1, tmp);

  GtkWidget *dialog = gtk_message_dialog_new (NULL,
           GTK_DIALOG_DESTROY_WITH_PARENT,
           GTK_MESSAGE_QUESTION,
           GTK_BUTTONS_YES_NO,
           "Tem certeza que deseja excluir o programa '%s' ?",
           tmp);

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
    char sql[300];

    sprintf(sql, "delete from ProgPrensaPassos where ProgPrensaID=%d", id);
    DB_Execute(&mainDB, 0, sql);

    sprintf(sql, "delete from ProgPrensa where ID=%d", id);
    DB_Execute(&mainDB, 0, sql);

    CarregaListaProgramas(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")));
  }

  gtk_widget_destroy (dialog);
}

void cbPrensaProgVoltar(GtkButton *button, gpointer user_data)
{
  WorkAreaGoTo(MaqConfigCurrent->AbaHome);
  SaindoProgParam();
}

void cbPrensaProgAbrirListaProg(GtkButton *button, gpointer user_data)
{
  CarregaListaProgramas(GTK_WIDGET(gtk_builder_get_object(builder, "tvwPrensaProg")));
  gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "ntbPrensaProgCadastro")), NTB_CADPROG_LISTA);
  WorkAreaGoTo(NTB_ABA_PRENSA_CADPROG);
}

/*** Fim do Código de Controle da Tela de Criacao / Edicao de Programas ***/
