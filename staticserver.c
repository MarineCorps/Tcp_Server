#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);
    printf("서버: 연결 대기 중...\n");

    int client_fd = accept(server_fd, NULL, NULL);
    printf("서버: 클라이언트 연결 수락\n");

    char buf[1024];
    while (1) {
        memset(buf, 0, sizeof(buf));
        ssize_t len = read(client_fd, buf, sizeof(buf));

        if (len <= 0) {
            printf("서버: 연결 종료됨\n");
            break;
        }

        printf("서버: 클라이언트 요청 수신 → %s\n", buf);

        if (strncmp(buf, "exit", 4) == 0)
            break;

        int mb = atoi(buf);
        if (mb <= 0 || mb > 100) {
            printf("서버: 잘못된 요청 크기\n");
            continue;
        }

        int size = mb * 1024 * 1024;
        char *dummy = malloc(size);
        memset(dummy, 'A', size);  // 더미 데이터 채우기

        int sent = 0;
        while (sent < size) {
            int n = write(client_fd, dummy + sent, size - sent);
            if (n <= 0) break;
            sent += n;
        }
        printf("서버: %d 바이트 전송 완료\n", sent);
        free(dummy);
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
