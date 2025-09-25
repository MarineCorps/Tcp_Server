#include <stdio.h>          // printf() 사용을 위한 표준 입출력 헤더
#include <string.h>         // memset(), strncmp(), strlen() 등의 문자열 처리 함수
#include <unistd.h>         // read(), write(), close() 같은 시스템콜
#include <arpa/inet.h>      // sockaddr_in, htons(), inet_ntoa() 등 네트워크 함수

int main() {
    // 1. 소켓 생성 (IPv4, TCP)
    // AF_INET: IPv4 주소 체계
    // SOCK_STREAM: TCP (연결 지향, 스트림 기반)
    // 0: 기본 프로토콜 자동 선택 (TCP)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 서버 주소 설정
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;                // 주소 체계: IPv4
    server_addr.sin_port = htons(8888);              // 포트 번호 (host to network byte order 변환)
    server_addr.sin_addr.s_addr = INADDR_ANY;        // 모든 IP(인터페이스)에서 접근 허용 (0.0.0.0)

    // 3. 소켓에 IP 주소와 포트 번호를 바인딩 ->
    // bind() 함수는 소켓 디스크립터와 주소 정보를 연결
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    // 1. 소켓함수를 통해 배정받은 디스크립터번호 , 2.IP와 포트 정보가 담긴 구조체를 가리키는 포인터, 3. 구조체 크기

    // 4. 클라이언트 연결 대기 (백로그 큐 크기: 5)
    listen(server_fd, 5);
    printf("서버: 연결 대기 중...\n");

    // 5. 클라이언트 연결 수락 (blocking, 연결이 오기 전까지 멈춤)
    // client_fd는 새로 생성된 연결된 소켓 디스크립터
    int client_fd = accept(server_fd, NULL, NULL);
    printf("서버: 클라이언트 연결 수락\n");

    // 6. 수신 버퍼 선언
    char buf[1024];

    // 7. 클라이언트로부터 반복적으로 메시지 수신 및 응답
    while (1) {
        memset(buf, 0, sizeof(buf)); // 버퍼 초기화 (이전 메시지 흔적 제거)

        // 클라이언트로부터 메시지 수신 (read는 blocking 함수)
        ssize_t len = read(client_fd, buf, sizeof(buf));

        // 연결 종료 또는 오류 발생 시
        if (len <= 0) {
            printf("서버: 연결 종료됨\n");
            break;
        }

        // 수신한 메시지 출력
        printf("서버: 클라이언트로부터 메시지 수신 → %s\n", buf);

        // 클라이언트가 "exit"을 보냈는지 확인
        if (strncmp(buf, "exit", 4) == 0) {
            printf("서버: 클라이언트 종료 요청 감지\n");
            break;
        }

        // 응답 메시지 전송 ("메시지 받음")
        char response[] = "메시지 받음";
        write(client_fd, response, strlen(response));
    }

    // 8. 소켓 종료 (클라이언트 → 서버 순서로 닫기)
    close(client_fd);   // 연결 소켓 종료
    close(server_fd);   // 리슨 소켓 종료

    return 0;
}
