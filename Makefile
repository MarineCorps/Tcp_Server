# 서버 컴파일 설정
CC = gcc
CFLAGS = -Wall 
TARGET = server_stream
SRC = server_stream.c

# 기본 타겟: 서버 빌드
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) 

clean:
	rm -f $(TARGET)
