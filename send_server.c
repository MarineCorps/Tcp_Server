#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define BLOCK_SIZE 4096  // 4KB씩 끊어서 전송

int main() {
    // 1. 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 3. 소켓과 주소 바인딩
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 4. 클라이언트 연결 대기
    listen(server_fd, 5);
    printf("서버: 연결 대기 중...\n");

    // 5. 클라이언트 연결 수락
    int client_fd = accept(server_fd, NULL, NULL);
    printf("서버: 클라이언트 연결 수락\n");

    char buf[1024];  // 클라이언트 요청 저장 버퍼

    while (1) {
        memset(buf, 0, sizeof(buf));
        // 요청 수신 (read -> recv)
        ssize_t len = recv(client_fd, buf, sizeof(buf), 0);

        if (len <= 0) {
            if (len < 0) perror("recv 실패");
            printf("서버: 연결 종료됨\n");
            break;
        }

        printf("서버: 요청 수신 → %s\n", buf);

        // "exit" 입력 시 종료
        if (strncmp(buf, "exit", 4) == 0)
            break;

        // 숫자 해석 (MB 단위)
        int mb = atoi(buf);
        if (mb <= 0 || mb > 100) {
            printf("서버: 잘못된 요청\n");
            continue;
        }

        int size = mb * 1024 * 1024;   // 바이트 단위 전환
        char block[BLOCK_SIZE];        // 블록 버퍼
        memset(block, 'A', BLOCK_SIZE); // 더미 데이터 채우기

        // 클라이언트로 데이터 전송 (write -> send)
        int sent = 0;
        while (sent < size) {
            int to_send = (size - sent < BLOCK_SIZE) ? (size - sent) : BLOCK_SIZE;
            ssize_t n = send(client_fd, block, to_send, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) continue;
                perror("send 실패");
                break;
            }
            sent += (int)n;
        }

        printf("서버: 총 %d 바이트 전송 완료\n", sent);
    }

    close(client_fd);  // 연결 종료
    close(server_fd);  // 서버 소켓 종료
    return 0;
}
