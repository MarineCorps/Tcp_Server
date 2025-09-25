# Tcp_Server

# 2) 서버 코드 해부

## 2-1. 소켓 생성

```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

* 클라이언트와 동일. 리스닝 소켓(수신 전용 엔드포인트) 생성.

## 2-2. 주소 바인딩

```c
struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(8888);
server_addr.sin_addr.s_addr = INADDR_ANY;
bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
```

* **유저 공간:** 이 프로세스가 **포트 8888**의 수신을 맡겠다고 OS에 등록. `INADDR_ANY`는 모든 로컬 IP에서 수신.
* **커널 내부:** 포트-소켓 매핑 등록. (재시작 편의 위해 `SO_REUSEADDR` 권장)

## 2-3. 리슨

```c
listen(server_fd, 5);
```

* **커널 내부:**

  * 상태를 LISTEN으로 전이.
  * \*\*half-open 큐(SYN backlog)\*\*와 **완료 큐(accept backlog)** 운용 시작.

## 2-4. accept() — 데이터 소켓 생성

```c
int client_fd = accept(server_fd, NULL, NULL);
```

* **커널 내부:** 클라이언트와 핸드셰이크 완료된 연결 엔트리를 **완료 큐에서 꺼내** 새 소켓 FD로 반환.
* **유저 공간:** `client_fd`는 **그 클라이언트 전용** 송수신에만 사용(리스닝 소켓은 계속 대기).

## 2-5. 요청 수신(read) → 분기

```c
ssize_t len = read(client_fd, buf, sizeof(buf));
if (len <= 0) { ... }
if (strncmp(buf, "exit", 4) == 0) break;
int mb = atoi(buf);
```

* **유저 공간:** 요청 문자열을 한 번 읽고 판단.
* **현 코드 특성:** 요청이 매우 짧으므로 한 번의 read로 충분할 가능성이 크지만, **원칙적으로**는 줄바꿈(`\n`)까지 읽는 루프가 더 견고(부분 수신 대비).

## 2-6. 더미 데이터 전송 루프(write)

```c
char block[BLOCK_SIZE];
memset(block, 'A', BLOCK_SIZE);

int sent = 0, size = mb*1024*1024;
while (sent < size) {
    int to_send = (size - sent < BLOCK_SIZE) ? (size - sent) : BLOCK_SIZE;
    int n = write(client_fd, block, to_send);
    if (n <= 0) break;
    sent += n;
}
```

* **유저 공간:** **부분 전송**에 대비한 누적 루프가 정확. `write()`는 항상 요청한 바이트를 모두 보내지 않을 수 있음.
* **커널 내부:**

  * 유저 버퍼 → 커널 송신버퍼 복사.
  * TCP가 세그먼트화(MSS 단위), cwnd/rwnd 고려, 혼잡·흐름 제어 하 전송.
  * 수신측이 느리면(윈도우 좁음) 서버의 write가 블록되거나 작은 n으로 자주 반환될 수 있음.
* **주의:** 상대가 먼저 닫으면 `write()`에서 **SIGPIPE**/`EPIPE`가 날 수 있음. 필요 시

  * `signal(SIGPIPE, SIG_IGN);` 또는
  * `send(..., MSG_NOSIGNAL)` 사용.

## 2-7. 종료

```c
close(client_fd);
close(server_fd);
```

* **FIN 교환:** 어느 쪽이 먼저 닫느냐에 따라 TIME\_WAIT은 **능동 종료자**가 가짐(대개 먼저 close한 쪽).

---

# 3) 리눅스 네트워크 스택 “큰그림”

## 3-1. 송신 경로(TCP write)

```
user buf --write-->
  [커널 TCP 송신버퍼(sndbuf)]
     └─(세그먼트화, cwnd/rwnd 고려, 재전송 큐 관리)
        → IP → qdisc(TX 큐잉) → NIC 드라이버
           → DMA 로 TX 링에 적재 → PHY로 전송
```

* **혼잡제어(cwnd)**: 네트워크 혼잡을 피하려 송신 윈도우를 자가 조절(초기 슬로스타트 → 혼잡회피).
* **흐름제어(rwnd)**: 수신측 rcvbuf 여유 기반 윈도우 광고. 수신 애플리케이션이 read()를 잘 해줘야 원활.

## 3-2. 수신 경로(TCP read)

```
선로 → NIC → DMA RX 링
  → 드라이버(softirq/NAPI) → skb 생성
    → IP → TCP(순서 재조립, ACK, 윈도우 업데이트)
      → [커널 TCP 수신버퍼(rcvbuf)]
         --read()--> user buf
```

* **GRO/TSO/GSO:**

  * **GRO**(RX): 여러 패킷을 커널에서 하나로 합쳐 상단부하를 줄임.
  * **TSO/GSO**(TX): 큰 TCP 덩어리를 NIC/커널이 분할해 전송(세그먼트 오프로딩).
* 이 최적화들은 지연/CPU 사용량/처리량에 영향을 줌.

## 3-3. 버퍼/윈도우가 속도를 결정

* 실효 전송률 ≈ `min(cwnd 기반 전송 가능량, rwnd 기반 수신 가능량)` / RTT 스케줄링.
* 디스크 쓰기(클라 `fwrite`)가 느리면 read 소비가 늦어져 rcvbuf가 꽉 차고, 결국 서버 송신이 막힐 수 있음.

---

# 4) 실습·관찰 팁 (바로 써먹기)

* **패킷 캡처(서버 또는 클라에서):**

  ```bash
  sudo tcpdump -i <iface> tcp port 8888 -n
  ```
* **소켓/버퍼 상태 보기:**

  ```bash
  ss -ntpi | grep :8888
  ```

  * `skmem:(rX,wY)` 수치, snd/rcv 큐 길이, 절대/상대 윈도우 확인.
* **커널 TCP 기본값:**

  ```bash
  sysctl net.ipv4.tcp_rmem
  sysctl net.ipv4.tcp_wmem
  sysctl net.ipv4.tcp_congestion_control
  ```
* **오프로딩 확인:**

  ```bash
  ethtool -k <iface> | egrep 'gro|gso|tso'
  ```
* **시스템콜 흐름 추적(블로킹/에러 포인트):**

  ```bash
  strace -tt -e trace=network -p <PID>
  ```

---

# 5) 견고성/성능 “권장 보강” (선택 적용)

### 공통

```c
// 0으로 초기화
memset(&server_addr, 0, sizeof(server_addr));

// 에러 체크 철저 (socket/bind/listen/accept/connect/read/write/fopen)
if (server_fd == -1) { perror("socket"); exit(1); }
```

### 서버(리스닝) 재시작 편의

```c
int yes = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
```

### 타임아웃(영구 블록 방지)

```c
struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
setsockopt(sock_or_clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
setsockopt(sock_or_clientfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
```

### 죽은 피어 탐지 가속(서버·클라)

```c
int ka = 1; setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
```

### 성능 튜닝(상황별)

```c
int sz = 1<<20; // 1MB
setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));

// 작은 청크를 지연 없이 바로 보내고 싶다면:
int one=1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

// 반대로 큰 덩어리로 모아서 한 방에(헤더 오버헤드↓) 보내고 싶다면 TCP_CORK도 고려
```

### SIGPIPE 안전화(서버 전송 루프)

```c
signal(SIGPIPE, SIG_IGN); // 또는 send(..., MSG_NOSIGNAL, ...)
```

### 요청 파싱 견고화

* 서버에서 **개행('\n')까지 읽기**로 바꾸고, 클라가 항상 줄바꿈을 보내도록 하면 부분 수신에도 안전:

  * 예: `"SIZE 10\n"` 형태, 서버는 `\n`까지 읽는 루틴.

### 측정 정밀도

* `clock_gettime(CLOCK_MONOTONIC, ...)` 사용 권장.
* 순수 네트워크 측정 시, 파일 쓰기(fwrite)를 비활성화하거나 tmpfs 사용.

---

# 6) 한 줄 핵심 요약

* \*\*여러분의 write()\*\*는 유저 버퍼를 **커널 송신버퍼**로 옮기고, TCP가 **세그먼트화＋혼잡/흐름제어**를 거쳐 네트워크로 보냅니다.
* **상대 커널**은 이를 **순서 재조립**해 **수신버퍼**에 쌓고, \*\*여러분의 read()\*\*가 가능한 만큼 가져갑니다.
* **스트림 특성** 때문에 “총 바이트 수를 알고 누적 수신으로 종료”가 정석이며, 버퍼/윈도우/RTT/오프로딩 등이 실제 속도를 결정합니다.

---

원하면, 지금 코드에 **REUSEADDR/타임아웃/KEEPALIVE/TCP\_NODELAY** 등을 깔끔히 넣은 “견고+튜닝 버전”을 바로 만들어줄게. 그리고 **epoll 기반 멀티클라이언트 서버**로 확장하는 단계도 이어서 안내할 수 있어.
****
