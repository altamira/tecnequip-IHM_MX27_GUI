/* TODO List

 1 - Não dar foco a campos de texto quando abrir uma nova aba, evitando ocultar a
aba sendo aberta
 2 - Verificar o código de login local pois o programa fechou sozinho
 3 - Executar o ntpdate no boot
 4 - Se o campo já está com o foco, não consigo abrir o teclado virtual

*/

#include "defines.h"
#include <gtk/gtk.h>
#include "GtkUtils.h"
#include <time.h>
#include <sys/time.h>

extern void AbrirLog   ();
extern void AbrirOper  ();
extern void AbrirConfig(unsigned int pos);

void Log(char *evento, int tipo);

struct MODBUS_Device mbdev;
struct strDB         mainDB;
extern int idUser; // Indica usuário que logou se for diferente de zero.

pthread_mutex_t mutex_ipcmq_rd   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ipcmq_wr   = PTHREAD_MUTEX_INITIALIZER;

// Funcoes para conexao ao SQL Server

struct strDB *sMSDB = NULL;

// Conversão entre UTF-8 para ISO-8859-1
char * MSSQL_UTF2ISO(char *data)
{
  unsigned char *in, *out;

  if(data == NULL)
    return NULL;

  // Usamos tanto a entrada como saida o endereco passado como parametro
  // Assim evitamos a necessidade de alocar e posteriormente liberar memoria
  // Como o formato ISO usa menos bytes que o UTF, nao existe o risco de estouro do buffer
  in = out = (unsigned char *)data;

  while(*in) {
    if (*in & 0x80) {
      *out    = (*in++ & 0x03)<<6;
      *out++ |= (*in++ & 0x3f);
    } else {
      *out++ = *in++;
    }
  }

  *out = 0;

  // Retornamos o proprio data para que se possa utilizar a funcao diretamente em chamadas
  // de outras funcoes. Ex.: printf("texto = %s\n", MSSQL_UTF2ISO(data));
  return data;
}

// Gera string de data no formato do SQL Server a partir de uma variavel time_t
char * MSSQL_DateFromTimeT(time_t t, char *data)
{
  struct tm *now;

  if(data == NULL)
    return NULL;

  now = localtime(&t);
  sprintf(data, "%4d-%02d-%02d %02d:%02d:%02d", 1900+now->tm_year, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);

  // Retornamos o proprio data para que se possa utilizar a funcao diretamente em chamadas
  // de outras funcoes. Ex.: printf("texto = %s\n", MSSQL_UTF2ISO(data));
  return data;
}

// Executa uma consulta SQL
int MSSQL_Execute(int nres, char *sql, unsigned int flags)
{
  char msg[200], *sql_sync;
  struct strDB *sDB;
  int ret;

  if(sql == NULL)
    return -1;

  sDB = MSSQL_Connect();
  if(sDB == NULL) {
    ret = -1;
  } else {
    ret = DB_Execute(sDB, nres, sql);
  }

  if(ret < 0) {
    if(flags & MSSQL_USE_SYNC) {
      sql_sync = (char *)malloc(strlen(sql)+100);
      sprintf(sql_sync, "insert into SyncTable (SyncSQL) values (\"%s\")", sql);

      if(DB_Execute(&mainDB, 3, sql_sync) < 0)
        strcpy(msg, "Erro executando consulta SQL. Sincronismo FALHOU!");
      else
        strcpy(msg, "Erro executando consulta SQL. Sincronismo OK");

      free(sql_sync);
    } else {
        strcpy(msg, "Erro executando consulta SQL. Sincronismo DESATIVADO!");
    }

    if(!(flags & MSSQL_DONT_REPORT))
      Log(msg, LOG_TIPO_SISTEMA);
  }

  return ret;
}

// Sincroniza o SQL Server com o MySQL
void MSSQL_Sync(void)
{
  char sql[500];
  unsigned int SyncDone = 0, SyncIncomplete = 0;

  // Carrega os comandos SQL aguardando para sincronizar
    DB_Execute(&mainDB, 2, "select SyncID, SyncSQL from SyncTable");

    // Loop entre todos os registros da tabela de sincronismo
    while(DB_GetNextRow(&mainDB, 2) > 0) {
      sql[strlen(sql)-1] = 0;
      strcat(sql, ") values (");

      if(!SyncDone) {
        Log("Iniciando Sincronismo com SQL Server", LOG_TIPO_SISTEMA);
        SyncDone = 1;
      }

      if(MSSQL_Execute(3, DB_GetData(&mainDB, 2, 1), MSSQL_DONT_SYNC) >= 0) {
        sprintf(sql, "delete from SyncTable where SyncID=%d", atoi(DB_GetData(&mainDB, 2, 0)));
        DB_Execute(&mainDB, 3, sql);
      } else {
        SyncIncomplete = 1;
        break;
      }
    }

  if(SyncDone) {
    if(SyncIncomplete)
      Log("Sincronismo com SQL Server falhou", LOG_TIPO_SISTEMA);
    else
      Log("Sincronismo com SQL Server finalizado com sucesso", LOG_TIPO_SISTEMA);
  }
}

struct strDB * MSSQL_Connect(void)
{
  int ret;
  static time_t timer = 0;

  if(sMSDB == NULL && time(NULL) > timer) {
    sMSDB = (struct strDB *)malloc(sizeof(struct strDB));
    DB_Clear(sMSDB);

    sMSDB->DriverID = "MSSQL";
    sMSDB->server   = "AltamiraSQLServer";
    sMSDB->user     = "scada";
    sMSDB->passwd   = "altamira@2012";
    sMSDB->nome_db  = "SCADA";

    ret = DB_Init(sMSDB);
    if(ret > 0) {
      MSSQL_Sync();
    } else {
      free(sMSDB);
      sMSDB = NULL;

      // Se ocorreu erro na conexao, aguarda 5 minutos para tentar novamente.
      timer = time(NULL) + 300;
    }
  }

  return sMSDB;
}

char * MSSQL_GetData(int nres, unsigned int pos)
{
  struct strDB *sDB = MSSQL_Connect();
  char *data = sDB != NULL ? DB_GetData(sDB, nres, pos) : NULL;

  if(data != NULL) {
    unsigned char *in = (unsigned char *)data, *out, *dest;
    dest = out = (unsigned char *)malloc(strlen(data)*2 + 1);

    while (*in)
        if (*in<128) *out++=*in++;
        else *out++=0xc2+(*in>0xbf), *out++=(*in++&0x3f)+0x80;

    *out = 0;
    data = (char *)dest;
  }

  return data;
}

void MSSQL_Close(void)
{
  if(sMSDB != NULL) {
    DB_Close(sMSDB);
    free(sMSDB);
    sMSDB = NULL;
  }
}

// Variavel indicando que houve atividade
int atividade = 0;

gboolean cbBackLightTurnOn(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
#ifndef DEBUG_PC
  //comm_put(&(struct comm_msg){ COMM_FNC_BL, { 0x1 } });
#endif

  atividade++;

  return TRUE;
}

void cbEspera(GtkButton *button, gpointer user_data)
{
  gchar *nome;
  static uint32_t estado_espera = 0;
  uint32_t selecionado;

  atividade++;

  if(button == NULL) { // Chamado diretamente, limpa estado e desliga backlight
#ifndef DEBUG_PC
    //comm_put(&(struct comm_msg){ COMM_FNC_BL, { 0x0 } });
#endif
    estado_espera = 0;
  } else {
    cbBackLightTurnOn(NULL, NULL, NULL);

    nome = (gchar *)gtk_widget_get_name(GTK_WIDGET(button));
    selecionado = atoi(&nome[strlen(nome)-1]);

    if(selecionado == estado_espera+1) { // Selecionado botao seguinte
      estado_espera = selecionado;
      if(selecionado == 2) {
        estado_espera = 0;
        WorkAreaGoPrevious();
      }
    } else {
      estado_espera = 0;
    }
  }
}

gboolean tmrActivity(gpointer data)
{
  if(!atividade) {
    cbEspera(NULL, NULL); // Atualiza o modo de espera
    WorkAreaGoTo(NTB_ABA_ESPERA);
  } else {
    atividade = 0;
  }

  return TRUE;
}

#define LOG_ID_SISTEMA 1

// Função que salva um log no banco contendo usuário e data que gerou o evento.
void Log(char *evento, int tipo)
{
  char sql[200];
  unsigned int LogID = idUser;
  static unsigned int log_in_mssql = 0;

  if(!LogID)
    LogID = LOG_ID_SISTEMA;

  if((mainDB.status & DB_FLAGS_CONNECTED)) // Banco conectado
    {
    sprintf(sql, "insert into log (ID_Usuario, Tipo, Evento) values ('%d', '%d', '%s')", LogID, tipo, evento);
    DB_Execute(&mainDB, 3, sql);
    }

  if(!log_in_mssql) {
    log_in_mssql = 1;
    sprintf(sql, "insert into LOG_EVENTO (LINHA, MAQUINA, OPERADOR, TIPO, HISTORICO) values ('%s', '%s', '%d', '%d', '%s')",
        MAQ_LINHA, MAQ_MAQUINA, LogID, tipo, evento);
    MSSQL_Execute(3, sql, MSSQL_USE_SYNC); // Mesmo que dê erro temos que inserir para sincronizarmos depois.
    log_in_mssql = 0;
  }
}

int32_t ihm_connect(char *host, int16_t port)
{
        struct  hostent  *ptrh;  /* pointer to a host table entry       */
        struct  protoent *ptrp;  /* pointer to a protocol table entry   */
        struct  sockaddr_in sad; /* structure to hold an IP address     */
        int32_t sd;              /* socket descriptor                   */

        memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
        sad.sin_family = AF_INET;         /* set family to Internet     */

        /* Check command-line argument for protocol port and extract    */
        /* port number if one is specified.  Otherwise, use the default */
        /* port value given by constant PROTOPORT                       */

        if (port > 0)                   /* test for legal value         */
                sad.sin_port = htons((uint16_t)port);
        else {                          /* print error message and exit */
                fprintf(stderr,"Bad port number %d\n",port);
                return -1;
        }

        /* Convert host name to equivalent IP address and copy to sad. */

        ptrh = gethostbyname(host);
        if ( ((int8_t *)ptrh) == NULL ) {
                fprintf(stderr,"Invalid host: %s\n", host);
                return -1;
        }
        memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

        /* Map TCP transport protocol name to protocol number. */

        if ( ((ptrp = getprotobyname("tcp"))) == NULL) {
              fprintf(stderr, "Cannot map \"tcp\" to protocol number");
                return -1;
        }

        /* Create a socket. */

        sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
        if (sd < 0) {
                fprintf(stderr, "Socket creation failed\n");
                return -1;
        }

        /* Connect the socket to the specified server. */

        if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
                fprintf(stderr,"Connect failed\n");
                return -1;
        }

        return sd;
}

struct PortaSerial *ps;
int32_t tcp_socket = -1;

MODBUS_HANDLER_TX(IHM_MB_TX)
{
  struct timeval tv;
  static struct timeval tv_last;
  static uint32_t primeiro = 1;

  uint32_t i, tent = 50;
  int32_t resp, wait_usec, wait, opts;

#ifdef DEBUG_PC_NOETH
  return 0;
#endif

  if(primeiro) {
    primeiro = 0;
    gettimeofday(&tv_last, NULL);
  }

  gettimeofday(&tv, NULL);
  wait_usec =  tv.tv_usec - tv_last.tv_usec;
  wait      = (tv.tv_sec  - tv_last.tv_sec) * 1000000;

  if(wait_usec < 0)
    wait += 1000000;
  wait += wait_usec;

//  if(wait < 250000 && wait > 0)
//    usleep(250000 - wait);

  gettimeofday(&tv, NULL);
  printf("\n%3d.%04ld - MB Send: ", (int)tv.tv_sec, (long)tv.tv_usec);
  for(i=0; i<size; i++)
    printf("%02x ", data[i]);
  printf("\n");

#ifndef DEBUG_PC
  tcp_socket = ihm_connect("192.168.2.237", 502);
#else
  tcp_socket = ihm_connect("192.168.0.172", 502);
#endif
  if(tcp_socket >= 0) {
    // Configura socket para o modo non-blocking e retorna se houver erro.
    opts = fcntl(tcp_socket,F_GETFL);
    if (opts < 0) {
        return 0;
    }
    if (fcntl(tcp_socket, F_SETFL, opts | O_NONBLOCK) < 0) {
        return 0;
    }
  } else {
        return 0;
  }

// Envia a mensagem pela ethernet
  send(tcp_socket, data, size, 0);

  while((resp=recv(tcp_socket, data, MODBUS_BUFFER_SIZE, 0))<=0 && tent--) {
    usleep(10000);
  }

  if(resp<=0) {
    size = 0;
    gettimeofday(&tv, NULL);
    printf("%3d.%04ld - Tempo para resposta esgotado...\n", (int)tv.tv_sec, (long)tv.tv_usec);
  } else {
    size = resp;
    gettimeofday(&tv, NULL);
    printf("%3d.%04ld - Retorno de MB Send: ", (int)tv.tv_sec, (long)tv.tv_usec);
    for(i=0; i<size; i++)
      printf("%02x ", data[i]);
    printf("\n");
  }

  close(tcp_socket);

  tv_last = tv;

  return size;
}

// Objeto que contem toda a interface GTK
GtkBuilder *builder;

// Variavel indicando que a tela de desligamento esta ativada
uint32_t OnPowerDown = 0;

extern key_t fd_rd;
extern key_t fd_wr;

void IPCMQ_Main_Enviar(struct strIPCMQ_Message *msg)
{
  pthread_mutex_lock(&mutex_ipcmq_wr);
  msgsnd(fd_wr, msg, IPCMQ_MESSAGE_SIZE, 0);
  pthread_mutex_unlock(&mutex_ipcmq_wr);
}

int IPCMQ_Main_Receber(struct strIPCMQ_Message *msg, int tipo)
{
  int ret;

  pthread_mutex_lock(&mutex_ipcmq_rd);
  ret = msgrcv(fd_rd, msg, IPCMQ_MESSAGE_SIZE, tipo, IPC_NOWAIT);
  pthread_mutex_unlock(&mutex_ipcmq_rd);

  return ret;
}

void IPCMQ_Threads_Enviar(struct strIPCMQ_Message *msg)
{
  pthread_mutex_lock(&mutex_ipcmq_rd);
  msgsnd(fd_rd, msg, IPCMQ_MESSAGE_SIZE, 0);
  pthread_mutex_unlock(&mutex_ipcmq_rd);
}

int IPCMQ_Threads_Receber(struct strIPCMQ_Message *msg)
{
  int ret;

  pthread_mutex_lock(&mutex_ipcmq_wr);
  ret = msgrcv(fd_wr, msg, IPCMQ_MESSAGE_SIZE, 0, IPC_NOWAIT);
  pthread_mutex_unlock(&mutex_ipcmq_wr);

  return ret;
}

// Funções para configuração do estado da máquina
unsigned int CurrentStatus = MAQ_STATUS_INDETERMINADO;
time_t LastStatusChange = 0;

void SetMaqStatus(unsigned int NewStatus)
{
  char data[100], sql[500];
  static time_t t = 0;

  printf("Configurando status. CurrentStatus=%d, NewStatus=%d\n", CurrentStatus, NewStatus);

  LastStatusChange = time(NULL);

  // Se não houve mudança, retorna.
  if(CurrentStatus == NewStatus)
    return;

  // Se t for zero, indica que ainda não foi carregado seu valor nenhuma vez, carrega agora.
  if(!t)
    t = time(NULL);

  // Checa o novo estado e toma as providências necessárias
  switch(NewStatus) {
  default: // Estado invalido! Configurando como indeterminado
    NewStatus = MAQ_STATUS_INDETERMINADO;
    /* no break */

  case MAQ_STATUS_INDETERMINADO:
    t = time(NULL);
    break;

  case MAQ_STATUS_PARADA:
    break;

  case MAQ_STATUS_SETUP:
    break;

  case MAQ_STATUS_MANUAL:
    break;

  case MAQ_STATUS_PRODUZINDO:
    break;

  case MAQ_STATUS_MANUTENCAO:
    break;
  }

  // Registra o estado da máquina no sistema
  if(NewStatus != MAQ_STATUS_INDETERMINADO) {
    // Se o estado atual não for indeterminado, estamos saindo de um estado válido.
    // Assim devemos considerar que o momento de transição é agora.
    // Se o estado atual é indeterminado, a transição de estado aconteceu quando entramos
    // em estado indeterminado e portanto não devemos ler a hora neste momento.
    if(CurrentStatus != MAQ_STATUS_INDETERMINADO)
      t = time(NULL);

    // Gera string com a data/hora do evento
    MSSQL_DateFromTimeT(t, data);

    // Insere o novo estado no banco e registra um log de evento
    sprintf(sql, "update LOG_STATUS set DATA_FINAL='%s' where ID=(select MAX(ID) from LOG_STATUS where LINHA='%s' and MAQUINA='%s')",
        data, MAQ_LINHA, MAQ_MAQUINA);
    MSSQL_Execute(0, sql, MSSQL_USE_SYNC);

    sprintf(sql, "insert into LOG_STATUS (LINHA, MAQUINA, DATA_INICIAL, CODIGO) values ('%s', '%s', '%s', '%d')",
        MAQ_LINHA, MAQ_MAQUINA, data, NewStatus);
    MSSQL_Execute(0, sql, MSSQL_USE_SYNC);

    sprintf(sql, "Status alterado para %d", NewStatus);
    Log(sql, LOG_TIPO_SISTEMA);

    MSSQL_Close();
  } else if(WorkAreaGet() == NTB_ABA_HOME || WorkAreaGet() == NTB_ABA_TAREFA) {
    WorkAreaGoTo(NTB_ABA_INDETERMINADO); // Atingiu timeout em tela home ou tarefa, podemos mudar para a tela de definição de parada
  }

  // Atualiza o estado atual para o novo estado
  CurrentStatus = NewStatus;
}

// Callback para definição do estado da máquina quando em modo indeterminado
void cbIndetMotivo(GtkButton *button, gpointer user_data)
{
  // Descobre o motivo selecionado pelo nome do botão
  const gchar *nome = gtk_buildable_get_name(GTK_BUILDABLE(button));
  unsigned int motivo = atoi(&nome[strlen(nome)-1]);

  // Configura o estado da máquina para o motivo selecionado
  SetMaqStatus(motivo);

  // Retorna para a tela anterior
  WorkAreaGoPrevious();
}

// Definições e Funções de controle da placa

#define BOARD_ADDR_GPIO_ENABLE 0x4800218C
#define BOARD_ADDR_GPIO_READ   0x49056038
#define BOARD_ADDR_GPIO_CLEAR  0x49054090
#define BOARD_ADDR_GPIO_SET    0x49054094

int Board_HasExternalPower(BoardStatus *bs)
{
  return bs->HasExternalPower;
}

void Board_GetAD(BoardStatus *bs, int channel)
{
#ifndef DEBUG_PC
  int   ret;
  float result;

  if(channel < 0 || bs == NULL || bs->par == NULL)
    return;

  memset(bs->par, 0, sizeof(struct twl4030_madc_user_parms));
  bs->par->channel = channel;

  ret    = ioctl(bs->dev, TWL4030_MADC_IOCX_ADC_RAW_READ, bs->par);
  result = ((unsigned int)bs->par->result) / 1024.f; // 10 bit ADC -> 1024

  if (ret == 0 && bs->par->status != -1) {
    switch(channel) {
    case BOARD_AD_VBAT:
      bs->BatteryVoltage  = result * 6.0;
      break;

    case BOARD_AD_VIN:
      bs->ExternalVoltage = result * 0.0;
      break;

    case BOARD_AD_TEMP:
      bs->Temperature     = result * 0.0;
      break;
    }
  }
#endif
}

void Board_Led(int TurnOn)
{
#ifndef DEBUG_PC
  io_write(TurnOn ? BOARD_ADDR_GPIO_CLEAR : BOARD_ADDR_GPIO_SET, 0x40000000);
#endif
}

void Board_GetPowerState(BoardStatus *bs)
{
  if(bs == NULL) return;

#ifndef DEBUG_PC
  int ps;

  // Leitura dos GPIOs
  ps = (io_read(BOARD_ADDR_GPIO_READ) >> 28)&7;

  // GPIO 158 indica se existe VIN. Nivel baixo = VIN OK
  bs->HasExternalPower = !((ps>>2)&1);

  // GPIOs 156 e 157 indicam estado do carregador.
  // 0 -> Precharge
  // 1 -> Bateria carregada
  // 2 -> Fast Charge
  // 3 -> Erro na bateria
  bs->BatteryState = ps&3;
  if(!bs->BatteryState) // Precharge
    bs->BatteryState = BOARD_BATT_CHARGING;
#else
  bs->HasExternalPower = 1;
  bs->BatteryState     = BOARD_BATT_FULL;
#endif
}

void Board_Update(BoardStatus *bs)
{
  Board_GetAD(bs, BOARD_AD_VIN );
  Board_GetAD(bs, BOARD_AD_VBAT);
  Board_GetAD(bs, BOARD_AD_TEMP);

  Board_GetPowerState(bs);
}

void Board_Init(BoardStatus *bs)
{
  if(bs == NULL)
    return;

  memset(bs, 0, sizeof(BoardStatus));

#ifndef DEBUG_PC
  bs->dev = open("/dev/twl4030-madc", O_RDWR | O_NONBLOCK);
  if(bs->dev >= 0) {
    bs->par = malloc(sizeof(struct twl4030_madc_user_parms));
  }

  io_write(BOARD_ADDR_GPIO_ENABLE  , io_read(BOARD_ADDR_GPIO_ENABLE  ) | 0x01000100);
  io_write(BOARD_ADDR_GPIO_ENABLE+4, io_read(BOARD_ADDR_GPIO_ENABLE+4) | 0x00000100);
#endif

  Board_Update(bs);
}

// Timers
gboolean tmrPowerDown(gpointer data)
{
  char msg[15];
  struct strIPCMQ_Message ipc_msg;
  BoardStatus *bs = (BoardStatus *)data;
  static uint32_t timeout=30;

  if(WorkAreaGet() != NTB_ABA_POWERDOWN) {
    timeout = 30;
  } else if(!timeout--) {
    gtk_main_quit();
  } else {
    sprintf(msg, "%d segundos", timeout);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPowerDownMsg")), msg);

    if(Board_HasExternalPower(bs)) {
      timeout = 30;
      WorkAreaGoPrevious();

      ipc_msg.fnc   = NULL;
      ipc_msg.res   = NULL;
      ipc_msg.mtype = IPCMQ_FNC_POWER;
      ipc_msg.data.power.status = 2;
      IPCMQ_Main_Enviar(&ipc_msg);
    }
  }

  return TRUE;
}

void cbPowerDownCancel(GtkButton *button, gpointer user_data)
{
  struct strIPCMQ_Message ipc_msg;

  ipc_msg.fnc   = NULL;
  ipc_msg.res   = NULL;
  ipc_msg.mtype = IPCMQ_FNC_POWER;
  ipc_msg.data.power.status = 1;
  IPCMQ_Main_Enviar(&ipc_msg);

  WorkAreaGoPrevious();
}

void IPC_Update(void)
{
  struct strIPCMQ_Message rcv;

#ifndef DEBUG_PC
  char *BattStatusDesc[] = { "Pré-Carga", "Carga Completa", "Carregando", "ERRO" };
  int i;
  char tmp[25];
  BoardStatus bs;
  GtkProgressBar *pgbVIN, *pgbVBAT;
  GtkImage       *imgBatt = NULL;
  GdkPixbuf      *pbBatt[6];
  GtkLabel       *lbl, *lblVINOK, *lblBatt;

  if(imgBatt == NULL) {
    pgbVIN   = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "pgbVIN"       ));
    pgbVBAT  = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "pgbVBAT"      ));
    imgBatt  = GTK_IMAGE       (gtk_builder_get_object(builder, "imgBateria"   ));
    lbl      = GTK_LABEL       (gtk_builder_get_object(builder, "lblTemp"      ));
    lblVINOK = GTK_LABEL       (gtk_builder_get_object(builder, "lblVINOK"     ));
    lblBatt  = GTK_LABEL       (gtk_builder_get_object(builder, "lblBattStatus"));

    pbBatt[0] = gdk_pixbuf_new_from_file("images/ihm-battery-error.png", NULL);
    for(i=1; i<5; i++) {
      sprintf(tmp, "images/ihm-battery-%d.png", i-1);
      pbBatt[i] = gdk_pixbuf_new_from_file(tmp, NULL);
    }
    pbBatt[i] = gdk_pixbuf_new_from_file("images/ihm-battery-full.png" , NULL);
  }
#endif

  while(IPCMQ_Main_Receber(&rcv, 0) >= 0) {
    switch(rcv.mtype) {
    case IPCMQ_FNC_TEXT:
      printf("Recebida mensagem de texto: %s\n", rcv.data.text);
      break;

    case IPCMQ_FNC_POWER:
      gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblPowerDownMsg")),
          "30 segundos");
      WorkAreaGoTo(NTB_ABA_POWERDOWN);

      break;

    case IPCMQ_FNC_BATT:
#ifndef DEBUG_PC
      if(rcv.data.batt_level < 0) {
        rcv.data.batt_level = 0;
      } else {
        rcv.data.batt_level++;
      }
      gtk_image_set_from_pixbuf(imgBatt, pbBatt[rcv.data.batt_level]);
#endif
      *(int *)rcv.res = 0;
      break;

    case IPCMQ_FNC_STATUS:
#ifndef DEBUG_PC
      bs = rcv.data.bs;

      sprintf(tmp, "%.01f °C", bs.Temperature);
      if(strcmp(tmp, gtk_label_get_text(lbl)))
        gtk_label_set_text(lbl, tmp);

      // Bateria com tensao inferior a 3V, sistema deve desligar!
      if(bs.BatteryVoltage < 3 && bs.ExternalVoltage < 8)
        gtk_main_quit();

      if((WorkAreaGet() == NTB_ABA_MANUT)) {
        gtk_progress_bar_set_fraction(pgbVIN , bs.ExternalVoltage/35);
        sprintf(tmp, "%.02f Volts", bs.ExternalVoltage);
        gtk_progress_bar_set_text(pgbVIN , tmp);

        gtk_progress_bar_set_fraction(pgbVBAT, bs.BatteryVoltage/6.0);
        sprintf(tmp, "%.02f Volts", bs.BatteryVoltage);
        gtk_progress_bar_set_text(pgbVBAT , tmp);

        gtk_label_set_text(lblBatt , BattStatusDesc[bs.BatteryState]);

        gtk_label_set_text(lblVINOK, bs.HasExternalPower ? "Sim" : "Não");
      }
#endif
      *(int *)rcv.res = 0;
      break;

    case IPCMQ_FNC_MODBUS:
      switch(rcv.data.modbus_reply.FunctionCode) {
      case MODBUS_FC_READ_DEVICE_IDENTIFICATION:
        printf("Identificador %d: %s\n", rcv.data.modbus_reply.reply.read_device_identification.object_id, rcv.data.modbus_reply.reply.read_device_identification.data);
        break;
      default:
        printf("Funcao desconhecida do modbus: %d\n", rcv.data.modbus_reply.FunctionCode);
        break;
      }

       if(rcv.fnc != NULL) {
        (*rcv.fnc)(&rcv.data, rcv.res);
      }

      break;

    default:
      printf("Mensagem de tipo desconhecido: %ld\n", rcv.mtype);
      break;
    }
  }
}

gboolean tmrGtkUpdate(gpointer data)
{
  time_t now;
  char tmp[40], *msg_error;
  uint32_t val, i;
  GtkWidget *wdg;
  struct tm *timeptr;
  static GtkLabel *lbl = NULL;
  static GdkPixbuf *pb_on = NULL, *pb_off = NULL;

  static int ciclos = 0, current_status = 0, last_status = 0;

  if(!OnPowerDown) {
    if(pb_on == NULL) { // Inicializa pixbufs
      pb_on  = gdk_pixbuf_new_from_file("images/ihm-status-on.png" , NULL);
      pb_off = gdk_pixbuf_new_from_file("images/ihm-status-off.png", NULL);
    }

    if(ciclos++ == 4) {
      ciclos = 0;

      // Leitura do estado dos CLPs, exibindo mensagem de erro caso houver
      current_status = MaqLerErros();
      if(last_status != current_status && current_status) { // houve mudanca e com erro
        MaqLiberar(0);
        msg_error = MaqStrErro(current_status);
        Log(msg_error, LOG_TIPO_ERRO);
        gtk_label_set_label(GTK_LABEL(gtk_builder_get_object(builder, "lblMensagens" )), msg_error);
        gtk_label_set_label(GTK_LABEL(gtk_builder_get_object(builder, "lblMessageBox")), msg_error);
        WorkAreaGoTo(NTB_ABA_MESSAGEBOX);
      } else if (last_status != current_status) {
        MaqLiberar(1);
        gtk_label_set_label(GTK_LABEL(gtk_builder_get_object(builder, "lblMensagens" )), MSG_SEM_ERRO);
      }
      last_status = current_status;

      // Se status não for indeterminado, parada ou produzindo e atingiu o tempo limite, entra em estado indeterminado
      if(CurrentStatus != MAQ_STATUS_INDETERMINADO && CurrentStatus != MAQ_STATUS_PARADA &&
         CurrentStatus != MAQ_STATUS_PRODUZINDO && (time(NULL) - LastStatusChange) > MAQ_IDLE_TIMEOUT) {
        SetMaqStatus(MAQ_STATUS_INDETERMINADO);
      }

      // Atualiza a hora da tela inicial
      if(lbl == NULL)
        lbl = GTK_LABEL(gtk_builder_get_object(builder, "lblHora"));

      now = time(NULL);
      timeptr = localtime(&now);

      sprintf(tmp, "%02d/%02d/%d, %.2d:%.2d",
          timeptr->tm_mday,
          timeptr->tm_mon + 1,
          1900 + timeptr->tm_year,
          timeptr->tm_hour,
          timeptr->tm_min);
      if(strcmp(tmp, gtk_label_get_text(lbl)))
        gtk_label_set_label(lbl, tmp);
    } else if(ciclos == 3) { // Divide as tarefas nos diversos ciclos para nao sobrecarregar
      if(WorkAreaGet() == NTB_ABA_MANUT) {
        val = MaqLerSaidas();
        for(i=0;;i++) { // Loop eterno, finaliza quando acabarem as saidas
          sprintf(tmp, "tglManutSai%02d", i);
          wdg = GTK_WIDGET(gtk_builder_get_object(builder, tmp));
          if(wdg == NULL) // Acabaram as saidas
            break; // Sai do loop

          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wdg), (val>>i)&1);
        }
      }
    } else if(ciclos == 2) { // Divide as tarefas nos diversos ciclos para nao sobrecarregar
      if(WorkAreaGet() == NTB_ABA_MANUT) {
        val = MaqLerEntradas();
        for(i=0;;i++) { // Loop eterno, finaliza quando acabarem as entradas
          sprintf(tmp, "imgStatusEnt%02d", i);
          wdg = GTK_WIDGET(gtk_builder_get_object(builder, tmp));
          if(wdg == NULL) // Acabaram as saidas
            break; // Sai do loop

          gtk_image_set_from_pixbuf(GTK_IMAGE(wdg), (val>>i)&1 ? pb_on : pb_off);
        }
      }
    }
  }

  IPC_Update();

  return TRUE;
}

/****************************************************************************
 * Funcoes que tratam os dados recebidos do Modbus
 ***************************************************************************/
void ReadID(void *dt, void *res)
{
  union uniIPCMQ_Data *data = (union uniIPCMQ_Data *)dt;

  printf("recebido: %s\n", data->modbus_reply.reply.read_device_identification.data);
}

/****************************************************************************
 * Callbacks gerais do GTK
 ***************************************************************************/

// Seleciona a aba correspondente ao botao clicado
void cbFunctionKey(GtkButton *button, gpointer user_data)
{
  uint32_t idx;
  const gchar *nome = gtk_buildable_get_name(GTK_BUILDABLE(button));

  idx = nome[strlen(nome)-1]-'0';
  WorkAreaGoTo(idx);

  switch(idx) {
  case NTB_ABA_CONFIG:
    AbrirConfig(0);
    break;

  case NTB_ABA_OPERAR:
    AbrirOper();
    break;

  case NTB_ABA_MANUAL:
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entOperarQtd")), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "entOperarTam")), "");
    break;

  case NTB_ABA_LOGS:
    AbrirLog();
    break;
  }
}

void LoadComboUsers(void)
{
  struct strDB *sDB = MSSQL_Connect();
  char *sql = "select USUARIO from OPERADOR where ID not in (select ID from OPERADOR where NOME='SISTEMA') order by ID";

  // Carregamento dos usuários cadastrados no SQL Server / MySQL no ComboBox.
  // Se não conectou no SQL Server, tenta no MySQL local.
  if(!(sDB && (sDB->status & DB_FLAGS_CONNECTED)))
    sDB = &mainDB;

  DB_Execute(sDB, 0, sql);
  CarregaCombo(sDB, GTK_COMBO_BOX(gtk_builder_get_object(builder, "cmbLoginUser")), 0, NULL);
}

void cbLogoff(GtkButton *button, gpointer user_data)
{
  Log("Saida do sistema", LOG_TIPO_SISTEMA);

  LoadComboUsers();

  // Grava zero em idUser para indicar que não há usuário logado
  idUser = 0;


  WorkAreaGoTo(NTB_ABA_LOGIN);
}

/****************************************************************************
 * Thread de comunicacao com o LPC2109 (Power Management) e CLPs (ModBus)
 ***************************************************************************/

void * ihm_update(void *args)
{
#ifndef DEBUG_PC
  BoardStatus bs, *newbs =(BoardStatus *)args;
  uint32_t ciclos = 0;
  int32_t  batt_level, curr_batt_level = -2;
  int StayON = 0, waiting_bs_reply = 0, waiting_batt_reply = 0;
  uint32_t ChangedBS = 0, LedState = 0;

  bs = *newbs;
#endif

  struct strIPCMQ_Message ipc_msg;

  /****************************************************************************
   * Loop
   ***************************************************************************/
  while (1) {
    usleep(500);
    /*** Loop de checagem de mensagens vindas da CPU LPC2109 ***/
#ifndef DEBUG_PC
    Board_Update(newbs);

    if(newbs->Temperature      != bs.Temperature      ||
       newbs->ExternalVoltage  != bs.ExternalVoltage  ||
       newbs->BatteryVoltage   != bs.BatteryVoltage   ||
       newbs->HasExternalPower != bs.HasExternalPower ||
       newbs->BatteryState     != bs.BatteryState) {

      ChangedBS = 1;
      bs = *newbs;
    }

    if(ChangedBS && !waiting_bs_reply) {
      ChangedBS = 0;
      waiting_bs_reply = 1;

      ipc_msg.fnc   = NULL;
      ipc_msg.res   = (void *)&waiting_bs_reply;
      ipc_msg.mtype = IPCMQ_FNC_STATUS;
      ipc_msg.data.bs = bs;
      IPCMQ_Threads_Enviar(&ipc_msg);

      if((OnPowerDown || StayON) && Board_HasExternalPower(&bs)) {
        StayON      = 0;
        OnPowerDown = 0;
        printf("Sistema energizado\n");
      } else if(!OnPowerDown && !Board_HasExternalPower(&bs) && !StayON) {
        OnPowerDown = 1;
        printf("Sistema sem energia\n");

        MaqGravarConfig();

        ipc_msg.fnc   = NULL;
        ipc_msg.res   = NULL;
        ipc_msg.mtype = IPCMQ_FNC_POWER;
        ipc_msg.data.power.status = 0;
        IPCMQ_Threads_Enviar(&ipc_msg);
      }
    }

    /*** Loop para atualizacao da imagem da bateria ***/

    if(waiting_batt_reply) {
      ciclos = 0;
    } else if(ciclos++ > 300) {
      ciclos = 0;

      if(bs.BatteryState == BOARD_BATT_ERROR) {
        Board_Led(LedState == 1 || LedState == 3);
        if(LedState++ > 7)
          LedState = 0;
      } else {
        LedState = !LedState;
        Board_Led(LedState);
      }

      // Verifica o estado atual da bateria: carregando, cheia ou com erro.
      if(Board_HasExternalPower(&bs)) {
        if(bs.BatteryState == BOARD_BATT_ERROR) {
          batt_level = -1;
        } else if(bs.BatteryState == BOARD_BATT_FULL) {
          batt_level = 4;
        } else {
          batt_level = curr_batt_level-1; // Cicla entre as imagens
          if(batt_level < 0) {
            batt_level = 3;
          }
        }
      } else { // Sistema alimentado pela bateria, calcula seu nivel atual
        batt_level = (int)((4.0 - bs.BatteryVoltage)*1000);
        if(batt_level < 0) {
          batt_level = 0;
        } else if(batt_level >= 1000) {
          batt_level = 999;
        }
        batt_level /= 250; // Nivel entre 0 (cheia) e 3 (vazia).
      }
      if(curr_batt_level != batt_level) { // Alterado o nivel, atualiza imagem
        waiting_batt_reply = 1;
        curr_batt_level = batt_level;

        ipc_msg.fnc   = NULL;
        ipc_msg.res   = (void *)&waiting_batt_reply;
        ipc_msg.mtype = IPCMQ_FNC_BATT;
        ipc_msg.data.batt_level = curr_batt_level;
        IPCMQ_Threads_Enviar(&ipc_msg);
      }
    }
#endif

    /*** Fim do loop para atualizacao da imagem da bateria ***/

    // Loop de checagem de mensagens vindas da thread principal
    if(IPCMQ_Threads_Receber(&ipc_msg) >= 0) {
      switch(ipc_msg.mtype) {
      case IPCMQ_FNC_POWER:
        printf("Resposta: %d\n", ipc_msg.data.power.status);
#ifndef DEBUG_PC
        if(ipc_msg.data.power.status == 1) // Escolhido permanecer ligado
          StayON = 1;
#endif

        OnPowerDown = 0;

        break;

      case IPCMQ_FNC_TEXT:
        strcpy(ipc_msg.data.text, "resposta");
        IPCMQ_Threads_Enviar(&ipc_msg);
        break;

      case IPCMQ_FNC_MODBUS:
#ifndef DEBUG_PC
        if(Board_HasExternalPower(&bs)) {
#else
        if(1) {
#endif
          ipc_msg.data.modbus_reply = Modbus_RTU_Send(&mbdev, 0,
                                           ipc_msg.data.modbus_query.function_code,
                                           &ipc_msg.data.modbus_query.data);
        } else {
          ipc_msg.data.modbus_reply.ExceptionCode = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        }
        IPCMQ_Threads_Enviar(&ipc_msg);

        break;
      }
    }
  }

  return NULL;
}

void cbVirtualKeyboardCapsLock(GtkToggleButton *button, gpointer user_data);

uint32_t IHM_Init(int argc, char *argv[])
{
  uint32_t ret = 0;
  pthread_t tid;
  GSList *lst;
  GtkWidget *wnd;
  BoardStatus bs;
  char *campos_log   [] = { "Data", "Usuário", "Evento", "" };
  char *campos_tarefa[] = { "Número", "Cliente", "Pedido", "Modelo", "Total", "Produzidas", "Tamanho", "Data", "Comentários", "" };

  /* init threads */
  g_thread_init (NULL);
  gdk_threads_init ();

  gtk_init( &argc, &argv );

  //Carrega a interface a partir do arquivo glade
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "IHM.glade", NULL);
  wnd = GTK_WIDGET(gtk_builder_get_object(builder, "wndDesktop"));
  //Conecta Sinais aos Callbacks
  gtk_builder_connect_signals(builder, NULL);

  // Carrega os nomes dos widgets do gtkbuilder para poderem ser usados nos temas
  lst = gtk_builder_get_objects(builder);
  while(lst != NULL) {
    if(GTK_IS_WIDGET(lst->data))
      gtk_widget_set_name(GTK_WIDGET(lst->data), gtk_buildable_get_name(GTK_BUILDABLE(lst->data)));
    lst = lst->next;
 }

//  g_object_unref (G_OBJECT (builder));
  gtk_rc_parse("gtk.rc");

  // Configura TreeView da tela de Tarefas
  TV_Config(GTK_WIDGET(gtk_builder_get_object(builder, "tvwTarefas")), campos_tarefa,
    GTK_TREE_MODEL(gtk_list_store_new((sizeof(campos_tarefa)/sizeof(campos_tarefa[0]))-1,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING)));

  // Configura TreeView da tela de Logs
  TV_Config(GTK_WIDGET(gtk_builder_get_object(builder, "tvwLog")), campos_log,
      GTK_TREE_MODEL(gtk_list_store_new((sizeof(campos_log)/sizeof(campos_log[0]))-1,
          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING)));

  // Atualiza o estado inicial do CapsLock como desativado
  cbVirtualKeyboardCapsLock(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "tgbCapsLock")), NULL);

  // Configura a mensagem inicial da maquina
  gtk_label_set_label(GTK_LABEL(gtk_builder_get_object(builder, "lblMensagens" )), MSG_SEM_ERRO);

  // Cria filas de mensagens para comunicacao entre a thread ihm_update e o main
  fd_rd = msgget(IPC_PRIVATE, IPC_CREAT);
  if(fd_rd < 0) {
    printf("erro criando fila fd_rd (%d): %s\n", errno, strerror(errno));
    ret = 1;
    goto fim_fila_rd;
  }

  fd_wr = msgget(IPC_PRIVATE, IPC_CREAT);
  if(fd_wr < 0) {
    printf("erro criando fila fd_wr (%d): %s\n", errno, strerror(errno));
    ret = 2;
    goto fim_fila_wr;
  }

#ifndef DEBUG_PC
  // Inicializa a placa
  Board_Init(&bs);
#endif

  // Inicializacao do ModBus
  mbdev.identification.Id = 0x02;
  mbdev.hl                = NULL;
  mbdev.hl_size           = 0;
  mbdev.mode              = MODBUS_MODE_TCP_MASTER;
  mbdev.TX                = IHM_MB_TX;

  if(!MaqLerConfig()) {
    printf("Erro carregando configuracao\n");
    ret = 7;
    goto fim_config;
  }

  // Limpa a estrutura do banco, zerando ponteiros, etc...
  DB_Clear(&mainDB);

  // Inicializa os drivers para acesso aos diferentes bancos
  DB_InitDrivers();

  // Carrega configuracoes do arquivo de configuracao e conecta ao banco
  if(!DB_LerConfig(&mainDB, DB_ARQ_CONFIG)) // Se ocorrer erro abrindo o arquivo, carrega defaults
    {
    mainDB.DriverID = "MySQL";
    mainDB.server   = "interno.tecnequip.com.br";
    mainDB.user     = "root";
    mainDB.passwd   = "y1cGH3WK20";
    mainDB.nome_db  = "cv_integrado";
    }

  WorkAreaGoTo(NTB_ABA_LOGIN);
  gtk_widget_show_all(wnd);

  // Iniciando os timers
  g_timeout_add(   500, tmrGtkUpdate, (gpointer)(&bs));
  g_timeout_add(  1000, tmrPowerDown, (gpointer)(&bs));
  g_timeout_add(300000, tmrActivity , NULL);

  pthread_create (&tid, NULL, ihm_update, (void *)(&bs));

  if(DB_Init(&mainDB)) { // Se inicializar o banco, entra no loop do GTK.
    // Carregamento no ComboBox dos usuários cadastrados no MySQL.
    LoadComboUsers();
  } else {
    GtkComboBox *cmb = GTK_COMBO_BOX(gtk_builder_get_object(builder, "cmbLoginUser"));

    MessageBox("Erro inicializando banco de dados");
    // Carregamento de usuário Master para acesso de emergência.
    CarregaItemCombo(cmb, "Master");
    gtk_combo_box_set_active(cmb, 0);
  }

  // Configura o estado inicial da máquina
  SetMaqStatus(MAQ_STATUS_PARADA);

  // Configura a máquina para modo manual.
  MaqConfigModo(MAQ_MODO_MANUAL);

  // Libera a máquina se sem erros.
  if(!MaqLerErros())
    MaqLiberar(1);

  gtk_main(); //Inicia o loop principal de eventos (GTK MainLoop)

  // Configura o estado final da máquina para PARADA pois ela está sendo desligada.
  SetMaqStatus(MAQ_STATUS_PARADA);

  DB_Close(&mainDB);

// A partir deste ponto iniciam os desligamentos. Em caso de erro na inicializacao, o programa
// salta para o label correspondente a etapa que falhou para desfazer o que ja havia sido feito

fim_config: // Encerrando por erro de configuracao
  MaqGravarConfig();

  msgctl(fd_wr, IPC_RMID, NULL);

fim_fila_wr: // Encerrando por falha ao criar fila de mensagens para escrita
  msgctl(fd_rd, IPC_RMID, NULL);

fim_fila_rd: // Encerrando por falha ao criar fila de mensagens para leitura
  pthread_mutex_destroy(&mutex_ipcmq_wr);
  pthread_mutex_destroy(&mutex_ipcmq_rd);

  return ret;
}

void TrataSinal(int sinal)
{
  char *lock[] = { "UNLOCKED", "LOCKED" };
  int rd_locked = 1, wr_locked = 1;

  if(!pthread_mutex_trylock(&mutex_ipcmq_rd)) {
    rd_locked = 0;
    pthread_mutex_unlock(&mutex_ipcmq_rd);
  }

  if(!pthread_mutex_trylock(&mutex_ipcmq_wr)) {
    wr_locked = 0;
    pthread_mutex_unlock(&mutex_ipcmq_wr);
  }

  printf("Estado dos mutex:\n\nRD = %s\nWR = %s\n\n",
      lock[rd_locked], lock[wr_locked]);

  printf("Recebido sinal %d, saindo!\n", sinal);
  gtk_main_quit();
}

//Inicia a aplicacao
int main(int argc, char *argv[])
{
  char tmp[10];
  GtkWidget *wdg;
  uint32_t ret;

  signal(SIGINT, TrataSinal);

  // Se ocorrer erro abrindo o programa, cria janela para avisar o usuario.
  if((ret = IHM_Init(argc, argv)) != 0) {
    printf("terminou com erro %d\n", ret);
    sprintf(tmp, "%02d", ret);

    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "lblInitErrorCode")), tmp);
    wdg = GTK_WIDGET(gtk_builder_get_object(builder, "wndInitError"));
    gtk_widget_show_all(wdg);

    gtk_main(); //Inicia o loop principal de eventos (GTK MainLoop)
  }

  return ret;
}
