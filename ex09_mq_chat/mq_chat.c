#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

#define MESSAGE_SIZE 512

static char to_client[64];
static char to_server[64];
static int made_client_queue = 0;
static int made_server_queue = 0;

static void cleanup(void)
{
    if (made_client_queue) {
        if (mq_unlink(to_client) == -1)
            perror("mq_unlink to_client");
        else
            printf("[정리] %s 삭제 완료\n", to_client);
    }

    if (made_server_queue) {
        if (mq_unlink(to_server) == -1)
            perror("mq_unlink to_server");
        else
            printf("[정리] %s 삭제 완료\n", to_server);
    }
}

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

static void send_text(mqd_t queue, const char *text)
{
    int result;
    do {
        result = mq_send(queue, text, strlen(text), 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
        fail("mq_send");
}

static int send_message(mqd_t queue)
{
    char *line = NULL;
    size_t capacity = 0;

    for (;;) {
        printf("나: ");
        fflush(stdout);

        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (ferror(stdin))
                fail("getline");

            free(line);
            send_text(queue, "/quit");
            return 0;
        }

        if (length > 0 && line[length - 1] == '\n')
            line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r')
            line[--length] = '\0';

        if (length >= MESSAGE_SIZE) {
            printf("메시지는 UTF-8 기준 511바이트 이하로 입력하세요.\n");
            continue;
        }

        send_text(queue, line);
        int keep_chatting = strcmp(line, "/quit") != 0;
        free(line);
        return keep_chatting;
    }
}

static int receive_message(mqd_t queue)
{
    char buffer[MESSAGE_SIZE + 1];
    unsigned int priority;
    ssize_t length;

    do {
        length = mq_receive(queue, buffer, MESSAGE_SIZE, &priority);
    } while (length == -1 && errno == EINTR);

    if (length == -1)
        fail("mq_receive");

    buffer[length] = '\0';

    if (strcmp(buffer, "/quit") == 0) {
        printf("상대방이 채팅을 종료했습니다.\n");
        return 0;
    }

    printf("상대: %s\n", buffer);
    printf("  [mq_receive] %zd바이트, 우선순위 %u\n",
           length, priority);
    return 1;
}

int main(int argc, char *argv[])
{
    snprintf(to_client, sizeof(to_client),
             "/lab2_ydh_%lu_to_client", (unsigned long)getuid());
    snprintf(to_server, sizeof(to_server),
             "/lab2_ydh_%lu_to_server", (unsigned long)getuid());

    if (argc != 2 ||
        (strcmp(argv[1], "server") != 0 &&
         strcmp(argv[1], "client") != 0 &&
         strcmp(argv[1], "clean") != 0)) {
        fprintf(stderr, "사용법: %s server|client|clean\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* 강제 종료 후 남은 큐를 정리할 때만 사용한다. */
    if (strcmp(argv[1], "clean") == 0) {
        int result = EXIT_SUCCESS;
        if (mq_unlink(to_client) == -1 && errno != ENOENT) {
            perror("mq_unlink to_client");
            result = EXIT_FAILURE;
        }
        if (mq_unlink(to_server) == -1 && errno != ENOENT) {
            perror("mq_unlink to_server");
            result = EXIT_FAILURE;
        }
        if (result == EXIT_SUCCESS)
            printf("남아 있는 실습용 메시지 큐 정리 완료\n");
        return result;
    }

    int server = strcmp(argv[1], "server") == 0;
    mqd_t input, output;

    printf("실습 9: POSIX 메시지 큐 채팅 (%s)\n", argv[1]);
    printf("서버부터 번갈아 입력합니다. /quit 입력 시 종료합니다.\n");

    if (server) {
        struct mq_attr attr = {0};
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = MESSAGE_SIZE;

        if (atexit(cleanup) != 0)
            return EXIT_FAILURE;

        output = mq_open(to_client,
                         O_CREAT | O_EXCL | O_WRONLY, 0600, &attr);
        if (output == (mqd_t)-1)
            fail("mq_open to_client");
        made_client_queue = 1;

        input = mq_open(to_server,
                        O_CREAT | O_EXCL | O_RDONLY, 0600, &attr);
        if (input == (mqd_t)-1)
            fail("mq_open to_server");
        made_server_queue = 1;

        printf("[mq_open] 큐 생성 완료\n");
        printf("  서버 -> 클라이언트: %s\n", to_client);
        printf("  클라이언트 -> 서버: %s\n", to_server);
    } else {
        input = mq_open(to_client, O_RDONLY);
        if (input == (mqd_t)-1)
            fail("mq_open: 서버를 먼저 실행하세요");

        output = mq_open(to_server, O_WRONLY);
        if (output == (mqd_t)-1) {
            mq_close(input);
            fail("mq_open to_server");
        }
        printf("[mq_open] 기존 큐 열기 완료\n");
    }

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
    if (mq_close(output) == -1) {
        perror("mq_close output");
        result = EXIT_FAILURE;
    }
    if (mq_close(input) == -1) {
        perror("mq_close input");
        result = EXIT_FAILURE;
    }

    printf("[mq_close] 큐 닫기 완료\n");
    if (server)
        printf("서버 종료 시 메시지 큐 두 개를 삭제합니다.\n");

    return result;
}
