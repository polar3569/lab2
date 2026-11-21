#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define TO_CLIENT "server_to_client.fifo"
#define TO_SERVER "client_to_server.fifo"

static int made_client_fifo = 0;
static int made_server_fifo = 0;

static void cleanup(void)
{
    if (made_client_fifo)
        unlink(TO_CLIENT);
    if (made_server_fifo)
        unlink(TO_SERVER);
}

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

/* 기존 파일을 덮어쓰지 않고 새 FIFO를 생성한다. */
static void create_fifos(void)
{
    if (mkfifo(TO_CLIENT, 0600) == -1)
        fail("mkfifo server_to_client");
    made_client_fifo = 1;

    if (mkfifo(TO_SERVER, 0600) == -1)
        fail("mkfifo client_to_server");
    made_server_fifo = 1;
}

static FILE *open_pipe(const char *path, int flags, const char *mode)
{
    int fd = open(path, flags);
    if (fd == -1)
        fail(path);

    FILE *stream = fdopen(fd, mode);
    if (stream == NULL) {
        int saved = errno;
        close(fd);
        errno = saved;
        fail("fdopen");
    }
    return stream;
}

static int send_message(FILE *output)
{
    char *line = NULL;
    size_t capacity = 0;

    printf("나: ");
    fflush(stdout);

    ssize_t length = getline(&line, &capacity, stdin);
    if (length == -1) {
        if (ferror(stdin))
            fail("getline");
        free(line);

        if (fputs("/quit\n", output) == EOF || fflush(output) == EOF)
            fail("send");
        return 0;
    }

    /* 줄 끝에 개행이 없는 입력도 메시지 한 줄로 전송한다. */
    if (fputs(line, output) == EOF)
        fail("send");
    if (length == 0 || line[length - 1] != '\n') {
        if (fputc('\n', output) == EOF)
            fail("send");
    }
    if (fflush(output) == EOF)
        fail("fflush");

    int keep_chatting = strcmp(line, "/quit\n") != 0 &&
                        strcmp(line, "/quit") != 0;
    free(line);
    return keep_chatting;
}

static int receive_message(FILE *input)
{
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length = getline(&line, &capacity, input);

    if (length == -1) {
        if (ferror(input))
            fail("receive");
        printf("상대방이 연결을 종료했습니다.\n");
        free(line);
        return 0;
    }

    if (strcmp(line, "/quit\n") == 0) {
        printf("상대방이 채팅을 종료했습니다.\n");
        free(line);
        return 0;
    }

    printf("상대: %s", line);
    free(line);
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 2 ||
        (strcmp(argv[1], "server") != 0 &&
         strcmp(argv[1], "client") != 0)) {
        fprintf(stderr, "사용법: %s server|client\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        fail("signal");

    int server = strcmp(argv[1], "server") == 0;
    FILE *input;
    FILE *output;

    printf("실습 8: FIFO 파이프 채팅 (%s)\n", argv[1]);
    printf("서버부터 번갈아 입력합니다. /quit 입력 시 종료합니다.\n");
    fflush(stdout);

    if (server) {
        if (atexit(cleanup) != 0)
            return EXIT_FAILURE;
        create_fifos();
        printf("FIFO 생성 완료. 클라이언트 연결 대기 중...\n");
        fflush(stdout);

        output = open_pipe(TO_CLIENT, O_WRONLY, "w");
        input = open_pipe(TO_SERVER, O_RDONLY, "r");
    } else {
        input = open_pipe(TO_CLIENT, O_RDONLY, "r");
        output = open_pipe(TO_SERVER, O_WRONLY, "w");
    }

    printf("연결 완료\n");

    for (;;) {
        if (server) {
            if (!send_message(output) || !receive_message(input))
                break;
        } else {
            if (!receive_message(input) || !send_message(output))
                break;
        }
    }

    int result = EXIT_SUCCESS;
    if (fclose(output) == EOF) {
        perror("fclose output");
        result = EXIT_FAILURE;
    }
    if (fclose(input) == EOF) {
        perror("fclose input");
        result = EXIT_FAILURE;
    }

    printf("채팅 종료 및 파이프 FD 닫기 완료\n");
    if (server)
        printf("서버 종료 시 FIFO 파일을 삭제합니다.\n");

    return result;
}
