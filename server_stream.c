#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BLOCK_SIZE 4096  // 4KB씩 끊어서 전송

int main() {
    // 1. 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 서버 주소 설정
    struct sockaddr_in server_addr;
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
        ssize_t len = read(client_fd, buf, sizeof(buf));  // 요청 수신

        if (len <= 0) {
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

        int size = mb * 1024 * 1024;  // 바이트 단위 전환
        char block[BLOCK_SIZE];      // 블록 버퍼
        memset(block, 'A', BLOCK_SIZE);  // 더미 데이터 채우기

        // 클라이언트로 데이터 전송
        int sent = 0;
        while (sent < size) {
            int to_send = (size - sent < BLOCK_SIZE) ? (size - sent) : BLOCK_SIZE;
            int n = write(client_fd, block, to_send);
            if (n <= 0) break;
            sent += n;
        }

        printf("서버: 총 %d 바이트 전송 완료\n", sent);
    }

    close(client_fd);  // 연결 종료
    close(server_fd);  // 서버 소켓 종료
    return 0;
}
// 이 코드는 TCP 소켓 서버로, 클라이언트로부터 MB 단위의 요청을 받아 해당 크기의 더미 데이터를 전송합니다.
// 클라이언트가 "exit"을 보내면 서버는 연결을 종료합니다.
// 데이터는 4KB 블록 단위로 전송되며, 최대 100MB까지 지원합니다.
// 잘못된 요청에 대해서는 오류 메시지를 출력하고, 클라이언트와의 연결을 유지합니다.
// 서버는 클라이언트의 요청을 수신하고, 해당 크기의 더미 데이터를 전송합니다.
