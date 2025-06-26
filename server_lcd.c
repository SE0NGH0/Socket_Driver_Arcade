// File: server.c
// 3개의 미니게임 결과를 LCD 및 LED로 출력하는 TCP 서버
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#define PORT        10000
#define MAX_CLIENTS 2
#define BUF_SIZE    128
#define PID_FILE    "server.pid"

typedef struct {
    int sockfd;
    int player_id;
} client_info_t;

typedef struct {
    int scores[MAX_CLIENTS];
    int current_round;
    pthread_mutex_t lock;
} game_state_t;

static game_state_t game = {{0}, 0, PTHREAD_MUTEX_INITIALIZER};
static client_info_t *clients[MAX_CLIENTS] = { NULL };

typedef struct {
    char buf[BUF_SIZE];
    struct timeval tv;
    int answered;
} response_t;

// 두 플레이어의 응답을 타임스탬프와 함께 받음
void recv_with_timestamp(client_info_t *c0, client_info_t *c1,
                         response_t *r0, response_t *r1) {
    fd_set rfds;
    int maxfd = ((c0->sockfd > c1->sockfd)? c0->sockfd : c1->sockfd) + 1;
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

// 가위바위보 게임
int play_rps_game(client_info_t *c0, client_info_t *c1) {
    const char *prompt = "RPS! rock/paper/scissors 입력:\n";
    const char *moves[] = {"rock","paper","scissors"};
    response_t r0, r1;
    char buf0[BUF_SIZE], buf1[BUF_SIZE];
    while (1) {
        send(c0->sockfd, prompt, strlen(prompt), 0);
        send(c1->sockfd, prompt, strlen(prompt), 0);
        r0.answered = r1.answered = 0;
        recv_with_timestamp(c0, c1, &r0, &r1);
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

// 연산 대전
int play_math_battle(client_info_t *c0, client_info_t *c1) {
    int a = rand()%10+1, b = rand()%10+1;
    char ops[] = "+-*/", op = ops[rand()%4];
    int res = (op=='+'?a+b:(op=='-'?a-b:(op=='*'?a*b:(b?a/b:0))));
    char msg[BUF_SIZE];
    snprintf(msg,sizeof(msg),"MATH %d %c %d\n",a,op,b);
    send(c0->sockfd,msg,strlen(msg),0);
    send(c1->sockfd,msg,strlen(msg),0);
    response_t r0={.answered=0}, r1={.answered=0};
    recv_with_timestamp(c0,c1,&r0,&r1);
    int ans0 = atoi(r0.buf), ans1 = atoi(r1.buf);
    int ok0=(ans0==res), ok1=(ans1==res);
    if(ok0&&!ok1) return c0->player_id;
    if(ok1&&!ok0) return c1->player_id;
    if(ok0&&ok1) {
        return (r0.tv.tv_sec<r1.tv.tv_sec ||
               (r0.tv.tv_sec==r1.tv.tv_sec && r0.tv.tv_usec<r1.tv.tv_usec))
               ? c0->player_id : c1->player_id;
    }
    return (r0.tv.tv_sec<r1.tv.tv_sec ||
           (r0.tv.tv_sec==r1.tv.tv_sec && r0.tv.tv_usec<r1.tv.tv_usec))
           ? c1->player_id : c0->player_id;
}

// 반응 속도 대전
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

// 클라이언트 핸들 쓰레드
void *handle_client(void *arg) {
    client_info_t *ci = arg;
    char m[BUF_SIZE];
    snprintf(m,sizeof(m),"[서버] Player %d 접속!\n",ci->player_id+1);
    send(ci->sockfd,m,strlen(m),0);
    while (1) {
        pthread_mutex_lock(&game.lock);
        if (clients[0] && clients[1]) { pthread_mutex_unlock(&game.lock); break; }
        pthread_mutex_unlock(&game.lock);
        usleep(100000);
    }
    return NULL;
}

// PID 파일 정리
void cleanup_pidfile() { remove(PID_FILE); }

// LED 출력 (misc 드라이버 사용)
static void set_leds(int wins) {
    int mask = (wins <= 0 ? 0 : (wins >= 3 ? 7 : ((1 << wins) - 1)));
    int fd = open("/dev/led_control", O_WRONLY);
    if(fd<0){ perror("open /dev/led_control"); return; }
    char mbuf[2]; int len = snprintf(mbuf,sizeof(mbuf),"%d",mask);
    write(fd, mbuf, len);
    close(fd);
}

// LCD 출력 (/dev/lcd1602)
static void write_lcd(const char *msg) {
    int fd = open("/dev/lcd1602", O_WRONLY);
    if(fd<0){ perror("open /dev/lcd1602"); return; }
    write(fd, msg, strlen(msg));
    close(fd);
}

int main() {
    // 이전 인스턴스 정리
    system("fuser -k 10000/tcp 2>/dev/null"); sleep(1);
    FILE *pf = fopen(PID_FILE,"r");
    if(pf) {
        int old; fscanf(pf,"%d",&old);
        if(kill(old,0)==0){ kill(old,SIGTERM); sleep(1); }
        fclose(pf); remove(PID_FILE);
    }
    pf = fopen(PID_FILE,"w");
    if(pf){ fprintf(pf,"%d\n",getpid()); fclose(pf); atexit(cleanup_pidfile); }

    srand(time(NULL));
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    int opt=1; setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr = {AF_INET, htons(PORT), INADDR_ANY};
    bind(server_fd,(struct sockaddr*)&addr,sizeof(addr));
    listen(server_fd,MAX_CLIENTS);
    printf("[서버] 포트 %d 대기중\n", PORT);

    // 클라이언트 연결
    pthread_t tid; int cnt=0;
    while(cnt<MAX_CLIENTS) {
        int cfd = accept(server_fd,NULL,NULL);
        client_info_t *ci = malloc(sizeof(*ci));
        ci->sockfd = cfd; ci->player_id = cnt;
        pthread_mutex_lock(&game.lock);
        clients[cnt++] = ci;
        pthread_mutex_unlock(&game.lock);
        pthread_create(&tid,NULL,handle_client,ci);
        pthread_detach(tid);
    }

    // 게임 라운드 진행
    int (*fns[3])(client_info_t*,client_info_t*) = {play_rps_game, play_math_battle, play_reaction_battle};
    while(game.current_round < 3) {
        int w = fns[game.current_round](clients[0], clients[1]);
        pthread_mutex_lock(&game.lock);
        game.scores[w]++;
        game.current_round++;
        pthread_mutex_unlock(&game.lock);
        for(int i=0;i<MAX_CLIENTS;i++){
            send(clients[i]->sockfd, i==w?"WIN\n":"LOSE\n", i==w?4:5, 0);
        }
    }

    // 최종 점수 요약 및 출력 준비
    int p1 = game.scores[0];
    int p2 = game.scores[1];
    char line1[17] = {0}, line2[17] = {0}, lcdbuf[33] = {0};
    snprintf(line1, sizeof(line1), "P1: %d win %d lose", p1, 3 - p1);
    int l1 = strlen(line1);
    if(l1 < 16) memset(line1 + l1, ' ', 16 - l1);
    snprintf(line2, sizeof(line2), "P2: %d win %d lose", p2, 3 - p2);
    int l2 = strlen(line2);
    if(l2 < 16) memset(line2 + l2, ' ', 16 - l2);
    memcpy(lcdbuf, line1, 16);
    memcpy(lcdbuf + 16, line2, 16);

    // LED 및 LCD 출력
    set_leds(p1);
    write_lcd(lcdbuf);

    // 클라이언트에 최종 결과 전송
    char summary[BUF_SIZE];
    snprintf(summary, sizeof(summary), "P1: %d win %d lose, P2: %d win %d lose\n",
             p1, 3-p1, p2, 3-p2);
    for(int i=0; i<MAX_CLIENTS; i++) {
        send(clients[i]->sockfd, summary, strlen(summary), 0);
        send(clients[i]->sockfd, "EXIT\n", 5, 0);
        close(clients[i]->sockfd);
        free(clients[i]);
    }
    close(server_fd);
    return 0;
}
