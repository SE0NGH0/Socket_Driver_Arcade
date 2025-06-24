#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define PORT 10000
#define MAX_CLIENTS 2
#define BUF_SIZE 128
#define PID_FILE "server.pid"

typedef struct {
    int sockfd;
    int player_id;
} client_info_t;

typedef struct {
    int scores[MAX_CLIENTS];
    int current_round;
    pthread_mutex_t lock;
} game_state_t;

static game_state_t game = {{0},0,PTHREAD_MUTEX_INITIALIZER};
static client_info_t *clients[MAX_CLIENTS] = {NULL};

typedef struct {
    char buf[BUF_SIZE];
    struct timeval tv;
    int answered;
} response_t;

void recv_with_timestamp(client_info_t *c0, client_info_t *c1,
                         response_t *r0, response_t *r1) {
    fd_set rfds;
    int maxfd = (c0->sockfd > c1->sockfd ? c0->sockfd : c1->sockfd) + 1;
    int count = 0;
    while (count < 2) {
        FD_ZERO(&rfds);
        FD_SET(c0->sockfd, &rfds);
        FD_SET(c1->sockfd, &rfds);
        select(maxfd, &rfds, NULL, NULL, NULL);
        if (FD_ISSET(c0->sockfd, &rfds) && !r0->answered) {
            gettimeofday(&r0->tv, NULL);
            int n = recv(c0->sockfd, r0->buf, BUF_SIZE-1, 0);
            r0->buf[(n>0?n:0)] = '\0'; r0->answered = 1; count++;
        }
        if (FD_ISSET(c1->sockfd, &rfds) && !r1->answered) {
            gettimeofday(&r1->tv, NULL);
            int n = recv(c1->sockfd, r1->buf, BUF_SIZE-1, 0);
            r1->buf[(n>0?n:0)] = '\0'; r1->answered = 1; count++;
        }
    }
}

int play_rps_game(client_info_t *c0, client_info_t *c1) {
    const char *prompt = "RPS! rock/paper/scissors 입력:\n";
    const char *moves[] = {"rock","paper","scissors"};
    response_t r0, r1;
    while (1) {
        send(c0->sockfd, prompt, strlen(prompt), 0);
        send(c1->sockfd, prompt, strlen(prompt), 0);
        r0.answered = r1.answered = 0;
        recv_with_timestamp(c0, c1, &r0, &r1);
        char buf0[BUF_SIZE], buf1[BUF_SIZE];
        strncpy(buf0, r0.buf, BUF_SIZE);
        strncpy(buf1, r1.buf, BUF_SIZE);
        buf0[strcspn(buf0, "\r\n")] = '\0';
        buf1[strcspn(buf1, "\r\n")] = '\0';
        int m0=-1, m1=-1;
        for(int i=0;i<3;i++){
            if(strcasecmp(buf0,moves[i])==0) m0=i;
            if(strcasecmp(buf1,moves[i])==0) m1=i;
        }
        if(m0<0||m1<0||m0==m1){
            send(c0->sockfd,"TIE\n",4,0);
            send(c1->sockfd,"TIE\n",4,0);
            continue;
        }
        return ((m0 - m1 + 3) % 3 == 1) ? c0->player_id : c1->player_id;
    }
}

int play_math_battle(client_info_t *c0, client_info_t *c1) {
    int a=rand()%10+1, b=rand()%10+1;
    char ops[]="+-*/", op=ops[rand()%4];
    int res = (op=='+'?a+b:(op=='-'?a-b:(op=='*'?a*b:(b?a/b:0))));
    char msg[BUF_SIZE];
    snprintf(msg,sizeof(msg),"MATH %d %c %d\n",a,op,b);
    send(c0->sockfd,msg,strlen(msg),0);
    send(c1->sockfd,msg,strlen(msg),0);

    response_t r0={.answered=0}, r1={.answered=0};
    recv_with_timestamp(c0,c1,&r0,&r1);
    int ans0=atoi(r0.buf), ans1=atoi(r1.buf);
    int ok0=(ans0==res), ok1=(ans1==res);
    if(ok0&&!ok1) return c0->player_id;
    if(ok1&&!ok0) return c1->player_id;
    if(ok0&&ok1){
        if(r0.tv.tv_sec<r1.tv.tv_sec ||
          (r0.tv.tv_sec==r1.tv.tv_sec && r0.tv.tv_usec<r1.tv.tv_usec))
            return c0->player_id;
        return c1->player_id;
    }
    // 둘 다 틀린 경우, 먼저 응답한 쪽이 패배
    if(r0.tv.tv_sec<r1.tv.tv_sec ||
      (r0.tv.tv_sec==r1.tv.tv_sec && r0.tv.tv_usec<r1.tv.tv_usec))
        return c1->player_id;
    return c0->player_id;
}

int play_reaction_battle(client_info_t *c0, client_info_t *c1) {
    sleep(rand()%3+1);
    send(c0->sockfd,"REACT\n",6,0);
    send(c1->sockfd,"REACT\n",6,0);
    response_t r0={.answered=0}, r1={.answered=0};
    recv_with_timestamp(c0,c1,&r0,&r1);
    return (r0.tv.tv_sec<r1.tv.tv_sec ||
           (r0.tv.tv_sec==r1.tv.tv_sec && r0.tv.tv_usec<r1.tv.tv_usec))
           ? c0->player_id : c1->player_id;
}

void *handle_client(void *arg){
    client_info_t *ci = arg;
    char m[BUF_SIZE];
    snprintf(m,sizeof(m),"[서버] Player %d 접속!\n",ci->player_id+1);
    send(ci->sockfd,m,strlen(m),0);
    while(1){
        pthread_mutex_lock(&game.lock);
        if(clients[0] && clients[1]) { pthread_mutex_unlock(&game.lock); break; }
        pthread_mutex_unlock(&game.lock);
        usleep(100000);
    }
    return NULL;
}

void cleanup_pidfile(){ remove(PID_FILE); }

int main(){
    // 이전 인스턴스 정리
    system("fuser -k 10000/tcp 2>/dev/null"); sleep(1);
    FILE *pf = fopen(PID_FILE,"r");
    if(pf){
        int old; fscanf(pf,"%d",&old);
        if(kill(old,0)==0){ kill(old,SIGTERM); sleep(1); }
        fclose(pf); remove(PID_FILE);
    }
    pf = fopen(PID_FILE,"w");
    if(pf){ fprintf(pf,"%d\n",getpid()); fclose(pf); atexit(cleanup_pidfile); }

    srand(time(NULL));
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1; setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(server_fd,(struct sockaddr*)&addr,sizeof(addr));
    listen(server_fd,MAX_CLIENTS);
    printf("[서버] 포트 %d 대기중\n",PORT);

    pthread_t tid; int cnt=0;
    while(cnt<MAX_CLIENTS){
        int cfd = accept(server_fd,NULL,NULL);
        client_info_t *ci = malloc(sizeof(*ci));
        ci->sockfd=cfd; ci->player_id=cnt;
        pthread_mutex_lock(&game.lock);
        clients[cnt++] = ci;
        pthread_mutex_unlock(&game.lock);
        pthread_create(&tid,NULL,handle_client,ci);
        pthread_detach(tid);
    }

    int (*fns[3])(client_info_t*,client_info_t*) = {
        play_rps_game, play_math_battle, play_reaction_battle
    };
    while(game.current_round<3){
        int w = fns[game.current_round](clients[0],clients[1]);
        pthread_mutex_lock(&game.lock);
        game.scores[w]++; game.current_round++;
        pthread_mutex_unlock(&game.lock);
        for(int i=0;i<MAX_CLIENTS;i++){
            send(clients[i]->sockfd,
                 i==w?"WIN\n":"LOSE\n",
                 i==w?4:5,0);
        }
    }

    // → 플레이어1 승리수만큼 GPIO 제어
    {
        int p1_wins = game.scores[0];
        int mask = (p1_wins<=0?0:(p1_wins>=3?7:((1<<p1_wins)-1)));
        int pins[3] = {17,27,22};
        for(int i=0;i<3;i++){
            char cmd[64];
            if(mask & (1<<i))
                snprintf(cmd,sizeof(cmd),"raspi-gpio set %d op dh",pins[i]);
            else
                snprintf(cmd,sizeof(cmd),"raspi-gpio set %d op dl",pins[i]);
            system(cmd);
        }
    }

    // 최종 결과 전송 및 정리
    char summary[BUF_SIZE];
    snprintf(summary,sizeof(summary),
             "[종료] P1 %d승 %d패, P2 %d승 %d패\n",
             game.scores[0],3-game.scores[0],
             game.scores[1],3-game.scores[1]);
    for(int i=0;i<MAX_CLIENTS;i++){
        send(clients[i]->sockfd,summary,strlen(summary),0);
        send(clients[i]->sockfd,"EXIT\n",5,0);
        close(clients[i]->sockfd);
        free(clients[i]);
    }
    close(server_fd);
    return 0;
}

