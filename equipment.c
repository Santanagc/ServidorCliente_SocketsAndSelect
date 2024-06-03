#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_EQUIPS 15

#define BUFFER_SIZE 1024

// Funcao que retorna erro ao ser passado o numero incorreto de argumentos
void usage(int argc, char **argv)
{
  printf("usage: %s <address> <port>\n", argv[0]);
  printf("example: %s 127.0.0.0 25252\n", argv[0]);
  exit(EXIT_FAILURE);
}

// Funcao que retorna erro para problemas com o socket
void logexit(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

// Funcao que manipula os ERROR()'s
int handleError(char *errorMsg)
{
  if (strcmp(errorMsg, "11(04)") == 0)
  {
    printf("Equipment limit exceeded\n");
    return -1;
  }
  else
  {
    return 0;
  }
}

int main(int argc, char **argv)
{
  int listID[MAX_EQUIPS];
  for (int i = 0; i < MAX_EQUIPS; i++)
  {
    listID[i] = 0;
  }
  int myID = 0;
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);

  // Teste dos argumentos Endereco e Porta
  if (argc != 3)
  {
    usage(argc, argv);
  }

  // Armazenamento do endereco e porta do socket
  char *serverIP = argv[1];
  in_port_t serverPort = atoi(argv[2]);

  // Criacao do socket e teste para condicao de erro, exemplo, equipment == -1
  int equipment = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (equipment < 0)
  {
    logexit("socket() failed\n");
  }

  // Definindo e inicializando os conjuntos de descritores de arquivos do equipamento
  fd_set equipmentSocket;
  int max = equipment;
  FD_ZERO(&equipmentSocket);
  FD_SET(STDIN_FILENO, &equipmentSocket);
  FD_SET(equipment, &equipmentSocket);

  // Criando a estrutura do endereco do servidor
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr)); // Zerando a struct
  serverAddr.sin_family = AF_INET;            // Definindo a familia do endereco como IPV4

  // Convertendo o endereco para string
  int returnVal = inet_pton(AF_INET, serverIP, &serverAddr.sin_addr.s_addr); // Conversao de endereco IPV4 para string
  if (returnVal == 0)
  {
    logexit("invalid address\n");
  }
  else if (returnVal < 0)
  {
    logexit("conversion of IPV4 failed\n");
  }
  serverAddr.sin_port = htons(serverPort); // Armazenando a porta na struct

  // Estabelecendo a conexao com o servidor e testando a confirmação do REQ_ADD
  if (connect(equipment, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
  {
    logexit("connect() failed\n");
  }
  else
  {
    ssize_t connectCheck = send(equipment, "05", strlen("05"), 0);
    if (connectCheck != strlen("05"))
    {
      logexit("send() failed\n");
    }

    ssize_t connectRcvd = recv(equipment, buffer, BUFFER_SIZE, 0);
    if (connectRcvd < 0)
    {
      logexit("recv() failed\n");
    }
    else if (handleError(buffer) < 0)
    {
      close(equipment);
      memset(buffer, 0, BUFFER_SIZE);
      exit(EXIT_FAILURE);
    }
    else
    {
      // Realizando o RES_ADD para o novo equipamento
      char *RES_ADD = strstr(buffer, "07");    // Pega o ID da mensagem RES_ADD
      int IDEquip = atoi(strchr(buffer, ' ')); // Pega o ID do equipamento

      if (RES_ADD != NULL)
      {
        myID = IDEquip;
        for (int i = 0; i < MAX_EQUIPS; i++)
        {
          if (listID[i] == 0)
          {
            listID[i] = myID;
            printf("New ID: %d\n", myID);
            break;
          }
        }
        memset(buffer, 0, BUFFER_SIZE);
      }
      else
      {
        logexit("RES_ADD failed");
      }
      memset(buffer, 0, BUFFER_SIZE);
    }
  }

  // Loop para a comunicação entre cliente e servidor
  while (1)
  {
    FD_ZERO(&equipmentSocket);
    FD_SET(STDIN_FILENO, &equipmentSocket);
    FD_SET(equipment, &equipmentSocket);

    // Verifica se há dados para leitura
    if (select(max + 1, &equipmentSocket, NULL, NULL, NULL) <= 0)
    {
      logexit("select() failed\n");
    }

    // Verifica se há dados para leitura vindos do socket do equipamento
    if (FD_ISSET(equipment, &equipmentSocket))
    {
      ssize_t confirmationRcvd = recv(equipment, buffer, BUFFER_SIZE, 0);
      if (confirmationRcvd < 0)
      {
        logexit("recv() failed\n");
      }
      else if (confirmationRcvd == 0)
      {
        // printf("connection closed by the server\n");
        close(equipment);
        memset(buffer, 0, BUFFER_SIZE);
        break;
      }
      else
      {
        // Seção de tratamento das mensagens de controle e erros
        char *RES_ADD = strstr(buffer, "07");       // Pega o ID da mensagem RES_ADD
        char *RES_LIST = strstr(buffer, "08");      // Pega o ID da mensagem RES_LIST
        char *REQ_REM = strstr(buffer, "06");       // Pega o ID da mensagem REQ_REM
        char *REQ_INF = strstr(buffer, "09");       // Pega o ID da mensagem REQ_INF
        char *RES_INF = strstr(buffer, "code: 10"); // Pega o ID da mensagem RES_INF
        char *REM_OK = strstr(buffer, "OK(01)");    // Pega o OK do REQ_REM
        char *ERROR_01 = strstr(buffer, "11(01)");  // Pega o erro 01
        char *ERROR_02 = strstr(buffer, "11(02)");  // Pega o erro 02
        char *ERROR_03 = strstr(buffer, "11(03)");  // Pega o erro 03

        if (RES_ADD != NULL) // RES_ADD
        {
          int IDEquip = atoi(strchr(buffer, ' ')); // Pega o ID do equipamento
          for (int i = 0; i < MAX_EQUIPS; i++)
          {
            if (listID[i] == 0)
            {
              listID[i] = IDEquip;
              printf("Equipment %d added\n", IDEquip);
              memset(buffer, 0, BUFFER_SIZE);
              break;
            }
          }
          memset(buffer, 0, BUFFER_SIZE);
        }
        // Unica forma que encontrei de tratar o RES_LIST corretamente foi tratar o caso de cada ID separadamente
        else if (RES_LIST != NULL) // RES_LIST
        {
          char *ID1 = strstr(buffer, " 1");
          char *ID2 = strstr(buffer, " 2");
          char *ID3 = strstr(buffer, " 3");
          char *ID4 = strstr(buffer, " 4");
          char *ID5 = strstr(buffer, " 5");
          char *ID6 = strstr(buffer, " 6");
          char *ID7 = strstr(buffer, " 7");
          char *ID8 = strstr(buffer, " 8");
          char *ID9 = strstr(buffer, " 9");
          char *ID10 = strstr(buffer, " 10");
          char *ID11 = strstr(buffer, " 11");
          char *ID12 = strstr(buffer, " 12");
          char *ID13 = strstr(buffer, " 13");
          char *ID14 = strstr(buffer, " 14");
          char *ID15 = strstr(buffer, " 15");

          for (int i = 0; i <= MAX_EQUIPS; i++)
          {
            if (ID1 != NULL && listID[i] == 0 && listID[i] != 1)
            {
              listID[i] = 1;
              break;
            }
            else if (ID2 != NULL && listID[i] == 0 && listID[i] != 2)
            {
              listID[i] = 2;
              break;
            }
            else if (ID3 != NULL && listID[i] == 0 && listID[i] != 3)
            {
              listID[i] = 3;
              break;
            }
            else if (ID4 != NULL && listID[i] == 0 && listID[i] != 4)
            {
              listID[i] = 4;
              break;
            }
            else if (ID5 != NULL && listID[i] == 0 && listID[i] != 5)
            {
              listID[i] = 5;
              break;
            }
            else if (ID6 != NULL && listID[i] == 0 && listID[i] != 6)
            {
              listID[i] = 6;
              break;
            }
            else if (ID7 != NULL && listID[i] == 0 && listID[i] != 7)
            {
              listID[i] = 7;
              break;
            }
            else if (ID8 != NULL && listID[i] == 0 && listID[i] != 8)
            {
              listID[i] = 8;
              break;
            }
            else if (ID9 != NULL && listID[i] == 0 && listID[i] != 9)
            {
              listID[i] = 9;
              break;
            }
            else if (ID10 != NULL && listID[i] == 0 && listID[i] != 10)
            {
              listID[i] = 10;
              break;
            }
            else if (ID11 != NULL && listID[i] == 0 && listID[i] != 11)
            {
              listID[i] = 11;
              break;
            }
            else if (ID12 != NULL && listID[i] == 0 && listID[i] != 12)
            {
              listID[i] = 12;
              break;
            }
            else if (ID13 != NULL && listID[i] == 0 && listID[i] != 13)
            {
              listID[i] = 13;
              break;
            }
            else if (ID14 != NULL && listID[i] == 0 && listID[i] != 14)
            {
              listID[i] = 14;
              break;
            }
            else if (ID15 != NULL && listID[i] == 0 && listID[i] != 15)
            {
              listID[i] = 15;
              break;
            }
          }
          memset(buffer, 0, BUFFER_SIZE);
        }
        else if (REQ_REM != NULL) // REQ_REM
        {
          int IDEquip = atoi(strchr(buffer, ' ')); // Pega o ID do equipamento
          for (int i = 0; i < MAX_EQUIPS; i++)
          {
            if (listID[i] == IDEquip && IDEquip != myID)
            {
              printf("Equipment %d removed\n", listID[i]);
              listID[i] = 0;
              break;
            }
          }
          memset(buffer, 0, BUFFER_SIZE);
          // break;
        }
        else if (REM_OK != NULL) // OK(01), confirmação do REQ_REM
        {
          // Finalizando a conexão
          memset(buffer, 0, BUFFER_SIZE);
          close(equipment);
          printf("Sucessful removal\n");
          exit(EXIT_SUCCESS);
          break;
        }
        else if (ERROR_01 != NULL) // ERROR(01)
        {
          printf("Equipment not found\n");
          memset(buffer, 0, BUFFER_SIZE);
        }
        else if (ERROR_02 != NULL) // ERROR(02)
        {
          printf("Source equipment not found\n");
          memset(buffer, 0, BUFFER_SIZE);
        }
        else if (ERROR_03 != NULL) // ERROR(03)
        {
          printf("Target equipment not found\n");
          memset(buffer, 0, BUFFER_SIZE);
        }
        else if (REQ_INF != NULL) // REQ_INF
        {
          char *getOrigem = strstr(buffer, "orig: ");
          char *getDestino = strstr(buffer, "dest: ");
          int IDOrigem = atoi(strchr(getOrigem, ' '));   // Pega o ID do equipamento de origem
          int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino

          printf("Equipment %d requested information\n", IDOrigem);

          int random = (rand() % 10);

          sprintf(buffer, "code: 10 orig: %d dest: %d payload: %d", IDOrigem, IDDestino, random); // Coloquei code: antes do código da mensagem para não dar conflito com o equipamento de numero 10
          send(equipment, buffer, strlen(buffer), 0);                                             // Envio RES_INF
          memset(buffer, 0, BUFFER_SIZE);
        }
        else if (RES_INF != NULL) // RES_INF
        {
          char *getDestino = strstr(buffer, "dest: ");
          char *getPayload = strstr(buffer, "payload: ");
          int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino
          int payload = atoi(strchr(getPayload, ' '));   // Pega o payload

          printf("Value from %d: %d\n", IDDestino, payload);
          memset(buffer, 0, BUFFER_SIZE);
        }
        memset(buffer, 0, BUFFER_SIZE);
      }
      memset(buffer, 0, BUFFER_SIZE);
    }

    //  Verifica se há dados para leitura vindos do terminal(stdin) para envio ao servidor
    if (FD_ISSET(STDIN_FILENO, &equipmentSocket))
    {
      memset(buffer, 0, BUFFER_SIZE);
      fgets(buffer, sizeof(buffer), stdin);
      buffer[strcspn(buffer, "\n")] = '\0'; // Removendo o '\n' da string digitada no terminal

      char *requestInfo = strstr(buffer, "./request information from");

      if (strcmp(buffer, "./close connection") == 0) // Condição de close pelo equipamento
      {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "06 %d", myID);
        ssize_t msgVerify = send(equipment, buffer, strlen(buffer), 0); // REQ_REM
        if (msgVerify != strlen(buffer))
        {
          logexit("send() failed\n");
        }
      }
      else if (strcmp(buffer, "./list equipment") == 0) // Condição de listagem dos equipamentos conectados no cluster
      {
        printf("Equipments on cluster:\n");
        for (int i = 0; i < MAX_EQUIPS; i++)
        {
          if (listID[i] != 0)
          {
            printf("Equipment %d\n", listID[i]);
          }
        }
      }
      else if (requestInfo != NULL) // Condição de request de informação a outro equipamento
      {
        char *getID = strstr(buffer, "from ");
        int IDRequested = atoi(strchr(getID, ' '));

        sprintf(buffer, "09 orig: %d dest: %d", myID, IDRequested);
        ssize_t msgVerify = send(equipment, buffer, strlen(buffer), 0); // REQ_REM
        if (msgVerify != strlen(buffer))
        {
          logexit("send() failed\n");
        }
      }
      else // Envio de qualquer outra mensagem digitada pelo equipamento ao servidor
      // Não foi pedido, mas acho uma boa implementação apenas para visualização de que o servidor recebe qualquer outra mensagem digitada pelo equipamento
      {
        ssize_t msgVerify = send(equipment, buffer, strlen(buffer), 0);
        if (msgVerify != strlen(buffer))
        {
          logexit("send() failed\n");
        }
      }
    }
  }
}