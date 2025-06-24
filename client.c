#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 10000
#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    inet_pton(AF_INET, argv[1], &serv.sin_addr);
    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        exit(1);
    }
    printf("[클라이언트] 서버(%s:%d) 연결 성공\n", argv[1], PORT);

    FILE *fp = fdopen(sockfd, "r+");
    char buf[BUF_SIZE];

    while (fgets(buf, BUF_SIZE, fp)) {
        if (strncmp(buf, "WIN\n", 4) == 0) {
            printf("[결과] 승리!\n");
            continue;
        }
        if (strncmp(buf, "LOSE\n", 5) == 0) {
            printf("[결과] 패배.\n");
            continue;
        }
        if (strncmp(buf, "TIE\n", 4) == 0) {
            printf("[결과] 무승부! 다시 합니다...\n");
            continue;
        }
        if (strncmp(buf, "RPS", 3) == 0) {
            printf("[게임: 가위바위보] 선택을 입력하세요: ");
            fflush(stdout);
            char in[BUF_SIZE];
            if (!fgets(in, BUF_SIZE, stdin)) break;
            in[strcspn(in,"\n")] = '\0';
            fprintf(fp, "%s\n", in);
            fflush(fp);
            continue;
        }
        if (strncmp(buf, "MATH", 4) == 0) {
            printf("[게임: 연산] 문제: %s", buf+5);
            printf("정답: ");
            fflush(stdout);
            char in[BUF_SIZE];
            if (!fgets(in, BUF_SIZE, stdin)) break;
            in[strcspn(in,"\n")] = '\0';
            fprintf(fp, "%s\n", in);
            fflush(fp);
            continue;
        }
        if (strncmp(buf, "REACT\n", 6) == 0) {
            printf("[게임: 반응속도] NOW! 엔터 누르세요...\n");
            fflush(stdout);
            char d[BUF_SIZE];
            if (!fgets(d, BUF_SIZE, stdin)) break;
            fprintf(fp, "HIT\n");
            fflush(fp);
            continue;
        }
        if (strncmp(buf, "[종료]", 6) == 0) {
            printf("%s", buf);
            break;
        }
        fputs(buf, stdout);
    }

    fclose(fp);
    return 0;
}

