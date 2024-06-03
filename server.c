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

// Struct para armazenar a lista de conexões
typedef struct
{
  int ID;
  int socket;
} id_s;

id_s connectionList[MAX_EQUIPS];
int numEquips = 0;

// Funcao que retorna erro ao ser passado o numero incorreto de argumentos
void usage(int argc, char **argv)
{
  printf("usage: %s <server-address> <p2p port> <client-server port>\n", argv[0]);
  printf("example: %s 127.0.01 25252 31313\n", argv[0]);
  exit(EXIT_FAILURE);
}

// Funcao que retorna erro para problemas com o socket
void logexit(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

// Função que manipula o erro (04)
// Iria usar essa função para manipular os outros erros, mas acabou gerando alguns probleams, então deixei apenas para o 04
void handleError(char *errorMsg, int socket)
{
  if (strcmp(errorMsg, "11(04)") == 0)
  {
    send(socket, "11(04)", strlen("11(04)"), 0);
    // printf("maximum of 15 equipments connected reached\n");
    numEquips--;
    close(socket);
  }
}

int main(int argc, char **argv)
{
  // Teste dos argumentos Endereco e Porta
  if (argc != 4)
  {
    usage(argc, argv);
  }
  // else
  // {
  //   printf("server initialized correctly\n");
  // }

  // Armazenamento do endereco e das portas dos sockets, e o endereço do servidor
  in_port_t client_servPort = atoi(argv[3]);
  // in_port_t p2pPort = atoi(argv[2]);
  char *serverIP = argv[1];

  // Criacao do socket para servidor-cliente e teste para condicao de erro
  int client_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client_server < 0)
  {
    logexit("socket() failed\n");
  }

  // Definindo a reutilização do endereço para corrigir erro de "Adress already in use"
  int reuse = 1;
  if (setsockopt(client_server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    logexit("setsockopt() failed\n");
  }

  // Criando a estrutura do endereco do servidor
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));                                // Zerando a struct
  serverAddr.sin_family = AF_INET;                                           // Definindo a familia do endereco como IPV4
  int returnVal = inet_pton(AF_INET, serverIP, &serverAddr.sin_addr.s_addr); // Conversao de endereco IPV4 para string
  if (returnVal == 0)
  {
    logexit("invalid address\n");
  }
  else if (returnVal < 0)
  {
    logexit("conversion from IPV4 to string failed\n");
  }
  serverAddr.sin_port = htons(client_servPort);

  // Fazendo bind ao endereco local
  if (bind(client_server, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0)
  {
    logexit("bind() failed\n");
  }

  // Pondo o socket para escutar e esperar por tentativas de conexões, além de definir um limite máximo de 15 equipamentos na fila
  if (listen(client_server, MAX_EQUIPS) < 0)
  {
    logexit("listen() failed\n");
  }
  // else
  // {
  //   printf("waiting for connections on port %d\n", (int)client_servPort);
  // }

  // Inicializando a lista de equipamento conectados
  for (int i = 0; i < MAX_EQUIPS; ++i)
  {
    connectionList[i].ID = 0;
    connectionList[i].socket = 0;
  }

  // Definindo uma lista(ou conjunto) de equipamentos conectados
  fd_set sockets, socketsReady;
  int maxSocket;

  // Inicializando o conjunto dos equipamentos
  FD_ZERO(&sockets);
  FD_SET(client_server, &sockets);
  maxSocket = client_server;

  char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);

  // Mantendo o servidor ligado constantemente e realizando comunicação
  while (1)
  {
    socketsReady = sockets;
    struct sockaddr_in clientAddr;              // Definindo o endereco do equipamento
    socklen_t clntAddrLen = sizeof(clientAddr); // Definindo o tamanho da struct do endereco do equipamento

    if (select(maxSocket + 1, &socketsReady, NULL, NULL, NULL) < 0) // Testa para saber se há atividade nos sockets
    {
      logexit("select() failed\n");
    }

    for (int sockIt = 0; sockIt <= maxSocket; sockIt++)
    {
      if (FD_ISSET(sockIt, &socketsReady))
      {
        // Trata dados no socket principal cliente-servidor sendo escutado
        if (sockIt == client_server)
        {
          // Aceita uma nova conexão
          int client = accept(client_server, (struct sockaddr *)&clientAddr, &clntAddrLen);
          if (client < 0)
          {
            logexit("accept() failed\n");
          }
          else
          {
            numEquips++;
          }

          memset(buffer, 0, BUFFER_SIZE);
          ssize_t msgRcvd = recv(client, buffer, BUFFER_SIZE, 0); // Recebe o REQ_ADD("05") do equipamento para tentativa de conexão
          if (msgRcvd < 0)
          {
            logexit("recv() failed\n");
          }
          else if (msgRcvd == 0)
          {
            FD_CLR(client, &sockets);
            close(client);
            numEquips--;
            memset(buffer, 0, BUFFER_SIZE);
          }

          char *REQ_ADD = strstr(buffer, "05");       // Pega o ID da mensagem REQ_ADD
          char *REQ_REM = strstr(buffer, "06");       // Pega o ID da mensagem REQ_REM
          char *REQ_INF = strstr(buffer, "09");       // Pega o ID da mensagem REQ_INF
          char *RES_INF = strstr(buffer, "code: 10"); // Pega o ID da mensagem RES_INF

          if (REQ_ADD != NULL) // Trata o REQ_ADD
          {
            if (numEquips <= MAX_EQUIPS)
            {
              for (int i = 0; i <= numEquips; i++)
              {
                if (connectionList[i].socket == 0) // Enviando para o novo equipamento o seu ID
                {
                  connectionList[i].ID = i + 1;
                  connectionList[i].socket = client;
                  sprintf(buffer, "07 %d", connectionList[i].ID);
                  send(client, buffer, strlen(buffer), 0); // RES_ADD para novo equipamento
                  break;
                }
              }
              for (int i = 0; i < numEquips; i++)
              {
                if (connectionList[i].socket != client)
                {
                  send(connectionList[i].socket, buffer, strlen(buffer), 0); // RES_ADD para cluster
                }
              }
              printf("Equipment %d added\n", atoi(strchr(buffer, ' ')));
              memset(buffer, 0, BUFFER_SIZE);

              for (int i = 0; i < numEquips; i++)
              {
                sleep(0.8); // Esse sleep foi colocado apenas para gerar um delay, pois estava havendo um conflito entre RES_ADD e RES_LIST, onde o RES_LIST não era enviado em algumas ocasiões
                memset(buffer, 0, BUFFER_SIZE);
                if (connectionList[i].socket != client)
                {
                  memset(buffer, 0, BUFFER_SIZE);
                  sprintf(buffer, "08 %d\n", connectionList[i].ID);
                  send(client, buffer, strlen(buffer), 0); // RES_LIST
                  // break;
                }
              }

              FD_SET(client, &sockets);
              if (client > maxSocket)
              {
                maxSocket = client;
              }
              memset(buffer, 0, BUFFER_SIZE);
              // break;
            }
            else
            {
              handleError("11(04)", client);
              memset(buffer, 0, BUFFER_SIZE);
            }
          }
          else if (REQ_REM != NULL) // Trata o REQ_REM
          {
            int remove = 0;
            int IDEquip = atoi(strchr(buffer, ' ')); // Pega o ID do equipamento
            for (int i = 0; i < MAX_EQUIPS; i++)
            {
              if (IDEquip == connectionList[i].ID)
              {
                for (int j = 0; j < MAX_EQUIPS; j++)
                {
                  if (connectionList[j].socket != client && connectionList[j].socket != 0)
                  {
                    memset(buffer, 0, BUFFER_SIZE);
                    sprintf(buffer, "06 %d", connectionList[i].ID);
                    send(connectionList[j].socket, buffer, strlen(buffer), 0);
                  }
                }
                printf("Equipment %d removed", connectionList[i].ID);
                connectionList[i].ID = 0;
                connectionList[i].socket = 0;
                send(client, "12OK(01)", strlen("12OK(01)"), 0);
                // FD_CLR(client, &sockets);
                // close(client);
                // numEquips--;
                memset(buffer, 0, BUFFER_SIZE);
                remove = 1;
                break;
              }
              else if (remove == 1)
              {
                send(client, "11(01)", strlen("11(01)"), 0);
                memset(buffer, 0, BUFFER_SIZE);
                break;
              }
            }
          }

          else if (REQ_INF != NULL) // Trata o REQ_INF
          {
            int foundOrig = 0;
            int foundDest = 0;
            char *getOrigem = strstr(buffer, "orig: ");
            char *getDestino = strstr(buffer, "dest: ");
            int IDOrigem = atoi(strchr(getOrigem, ' '));   // Pega o ID do equipamento de origem
            int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino

            for (int i = 0; i < MAX_EQUIPS; i++)
            {
              if (IDOrigem == connectionList[i].ID)
              {
                foundOrig = 1;
                for (int j = 0; j < MAX_EQUIPS; j++)
                {
                  if (IDDestino == connectionList[j].ID)
                  {
                    foundDest = 1;
                    memset(buffer, 0, BUFFER_SIZE);
                    sprintf(buffer, "09 orig: %d dest: %d", IDOrigem, IDDestino);
                    send(connectionList[j].socket, buffer, strlen(buffer), 0);
                    break;
                  }
                }
              }
              if (foundDest == 1 && foundOrig == 1)
              {
                break;
              }
            }
            if (foundOrig == 0)
            {
              printf("Equipment %d not found\n", IDOrigem);
              // send(client, "11(02)", strlen("11(02)"), 0);
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDOrigem == connectionList[i].ID)
                {
                  send(connectionList[i].socket, "11(02)", strlen("11(02)"), 0);
                  break;
                }
              }
            }
            else if (foundOrig == 1 && foundDest == 0)
            {
              printf("Equipment %d not found\n", IDDestino);
              // send(client, "11(03)", strlen("11(03)"), 0);
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDOrigem == connectionList[i].ID)
                {
                  send(connectionList[i].socket, "11(03)", strlen("11(03)"), 0);
                  break;
                }
              }
            }
          }

          else if (RES_INF != NULL) // Trata o RES_INF
          {
            int foundOrig = 0;
            int foundDest = 0;
            char *getOrigem = strstr(buffer, "orig: ");
            char *getDestino = strstr(buffer, "dest: ");
            char *getPayload = strstr(buffer, "payload: ");
            int IDOrigem = atoi(strchr(getOrigem, ' '));   // Pega o ID do equipamento de origem
            int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino
            int payload = atoi(strchr(getPayload, ' '));   // Pega o payload

            for (int i = 0; i < MAX_EQUIPS; i++)
            {
              if (IDDestino == connectionList[i].ID)
              {
                foundDest = 1;
                for (int j = 0; j < MAX_EQUIPS; j++)
                {
                  if (IDOrigem == connectionList[j].ID)
                  {
                    foundOrig = 1;
                    memset(buffer, 0, BUFFER_SIZE);
                    sprintf(buffer, "code: 10 orig: %d dest: %d payload: %d", IDOrigem, IDDestino, payload);
                    send(connectionList[j].socket, buffer, strlen(buffer), 0);
                    break;
                  }
                }
                if (foundDest == 1 && foundOrig == 1)
                {
                  break;
                }
              }
            }
            if (foundDest == 0)
            {
              printf("Equipment %d not found\n", IDDestino);
              // send(sockIt, "11(02)", strlen("11(02)"), 0);
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDDestino == connectionList[i].ID)
                {
                  // send(client, "11(02)", strlen("11(02)"), 0);
                  send(connectionList[i].socket, "11(02)", strlen("11(02)"), 0);
                  break;
                }
              }
            }
            else if (foundDest == 1 && foundOrig == 0)
            {
              printf("Equipment %d not found\n", IDOrigem);
              // send(sockIt, "11(03)", strlen("11(03)"), 0);
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDOrigem == connectionList[i].ID)
                {
                  // send(client, "11(02)", strlen("11(02)"), 0);
                  send(connectionList[i].socket, "11(03)", strlen("11(03)"), 0);
                  break;
                }
              }
            }
          }
        }

        // Trata dados nos sockets adjacentes
        else
        {
          // Manipulando a comunicacao com o equipamento
          memset(buffer, 0, BUFFER_SIZE);
          ssize_t msgRcvd = recv(sockIt, buffer, BUFFER_SIZE, 0);
          if (msgRcvd < 0)
          {
            logexit("recv() failed");
          }
          else if (msgRcvd == 0)
          {
            // Remove o equipamento do conjunto e finaliza a conexao com o equipamento
            FD_CLR(sockIt, &sockets);
            close(sockIt);
            numEquips--;
            memset(buffer, 0, BUFFER_SIZE);
          }
          else
          {
            char *REQ_REM = strstr(buffer, "06");       // Pega o ID da mensagem REQ_REM
            char *REQ_INF = strstr(buffer, "09");       // Pega o ID da mensagem REQ_INF
            char *RES_INF = strstr(buffer, "code: 10"); // Pega o ID da mensagem RES_INF

            if (REQ_REM != NULL) // Trata o REQ_REM
            {
              int remove = 0;
              int IDEquip = atoi(strchr(buffer, ' ')); // Pega o ID do equipamento
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDEquip == connectionList[i].ID)
                {
                  for (int j = 0; j < MAX_EQUIPS; j++)
                  {
                    if (connectionList[j].socket != sockIt && connectionList[j].socket != 0)
                    {
                      memset(buffer, 0, BUFFER_SIZE);
                      sprintf(buffer, "06 %d", connectionList[i].ID);
                      send(connectionList[j].socket, buffer, strlen(buffer), 0);
                    }
                  }
                  printf("Equipment %d removed\n", connectionList[i].ID);
                  connectionList[i].ID = 0;
                  connectionList[i].socket = 0;
                  send(sockIt, "12OK(01)", strlen("12OK(01)"), 0);
                  memset(buffer, 0, BUFFER_SIZE);
                  remove = 1;
                  break;
                }
                else if (remove == 1)
                {
                  send(sockIt, "11(01)", strlen("11(01)"), 0);
                  memset(buffer, 0, BUFFER_SIZE);
                  break;
                }
              }
            }
            else if (REQ_INF != NULL) // Trata o REQ_INF
            {
              int foundOrig = 0;
              int foundDest = 0;
              char *getOrigem = strstr(buffer, "orig: ");
              char *getDestino = strstr(buffer, "dest: ");
              int IDOrigem = atoi(strchr(getOrigem, ' '));   // Pega o ID do equipamento de origem
              int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino
              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDOrigem == connectionList[i].ID)
                {
                  foundOrig = 1;
                  for (int j = 0; j < MAX_EQUIPS; j++)
                  {
                    if (IDDestino == connectionList[j].ID)
                    {
                      foundDest = 1;
                      memset(buffer, 0, BUFFER_SIZE);
                      sprintf(buffer, "09 orig: %d dest: %d", IDOrigem, IDDestino);
                      send(connectionList[j].socket, buffer, strlen(buffer), 0);
                      break;
                    }
                  }
                }
                if (foundDest == 1 && foundOrig == 1)
                {
                  break;
                }
              }
              if (foundOrig == 0)
              {
                printf("Equipment %d not found\n", IDOrigem);
                // send(sockIt, "11(02)", strlen("11(02)"), 0);
                for (int i = 0; i < MAX_EQUIPS; i++)
                {
                  if (IDOrigem == connectionList[i].ID)
                  {
                    send(connectionList[i].socket, "11(02)", strlen("11(02)"), 0);
                    break;
                  }
                }
              }
              else if (foundOrig == 1 && foundDest == 0)
              {
                printf("Equipment %d not found\n", IDDestino);
                // send(sockIt, "11(03)", strlen("11(03)"), 0);
                for (int i = 0; i < MAX_EQUIPS; i++)
                {
                  if (IDOrigem == connectionList[i].ID)
                  {
                    send(connectionList[i].socket, "11(03)", strlen("11(03)"), 0);
                    break;
                  }
                }
              }
            }

            else if (RES_INF != NULL) // Trata o RES_INF
            {
              int foundOrig = 0;
              int foundDest = 0;
              char *getOrigem = strstr(buffer, "orig: ");
              char *getDestino = strstr(buffer, "dest: ");
              char *getPayload = strstr(buffer, "payload: ");
              int IDOrigem = atoi(strchr(getOrigem, ' '));   // Pega o ID do equipamento de origem
              int IDDestino = atoi(strchr(getDestino, ' ')); // Pega o ID do equipamento de destino
              int payload = atoi(strchr(getPayload, ' '));   // Pega o payload

              for (int i = 0; i < MAX_EQUIPS; i++)
              {
                if (IDDestino == connectionList[i].ID)
                {
                  foundDest = 1;
                  for (int j = 0; j < MAX_EQUIPS; j++)
                  {
                    if (IDOrigem == connectionList[j].ID)
                    {
                      foundOrig = 1;
                      memset(buffer, 0, BUFFER_SIZE);
                      sprintf(buffer, "code: 10 orig: %d dest: %d payload: %d", IDOrigem, IDDestino, payload);
                      send(connectionList[j].socket, buffer, strlen(buffer), 0);
                      break;
                    }
                  }
                  if (foundDest == 1 && foundOrig == 1)
                  {
                    break;
                  }
                }
              }

              if (foundDest == 0)
              {
                printf("Equipment %d not found\n", IDDestino);
                // send(sockIt, "11(02)", strlen("11(02)"), 0);
                for (int i = 0; i < MAX_EQUIPS; i++)
                {
                  if (IDDestino == connectionList[i].ID)
                  {
                    // send(sockIt, "11(02)", strlen("11(02)"), 0);
                    send(connectionList[i].socket, "11(02)", strlen("11(02)"), 0);
                    break;
                  }
                }
                // break;
              }
              else if (foundDest == 1 && foundOrig == 0)
              {
                printf("Equipment %d not found\n", IDOrigem);
                // send(sockIt, "11(03)", strlen("11(03)"), 0);
                for (int i = 0; i < MAX_EQUIPS; i++)
                {
                  if (IDOrigem == connectionList[i].ID)
                  {
                    // send(sockIt, "11(02)", strlen("11(02)"), 0);
                    send(connectionList[i].socket, "11(03)", strlen("11(03)"), 0);
                    break;
                  }
                }
                // break;
              }
            }

            // Enviando confirmação de recebimento para o equipamento
            // Não necessária, implementada apenas para mostrar o recebimento caso algo diferente seja digitado, para mostrar a conexão
            else
            {
              for (int i = 0; i < numEquips; i++)
              {
                if (connectionList[i].socket == sockIt)
                {
                  // printf("message from equipement %d: %s\n", connectionList[i].ID, buffer);
                  memset(buffer, 0, BUFFER_SIZE);
                  break;
                }
              }
            }
            memset(buffer, 0, BUFFER_SIZE);
          }
        }
      }
    }
  }
}