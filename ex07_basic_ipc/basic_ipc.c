#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

int main(void)
{
    int pipe_fd[2];
    const char *message = "Hello from parent! IPC test success.";

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return EXIT_FAILURE;

    printf("실습 7: 파이프를 이용한 기본 IPC\n");

    if (pipe(pipe_fd) == -1)
        fail("pipe");

    printf("[pipe] 읽기 FD=%d, 쓰기 FD=%d\n",
           pipe_fd[0], pipe_fd[1]);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        fail("fork");
    }

    if (child_pid == 0) {
        /* 자식은 수신만 하므로 쓰기 끝을 닫는다. */
        if (close(pipe_fd[1]) == -1)
            fail("close");

        char buffer[128];
        size_t total = 0;

        while (total < sizeof(buffer) - 1) {
            ssize_t count = read(pipe_fd[0], buffer + total,
                                 sizeof(buffer) - 1 - total);

            if (count == -1) {
                if (errno == EINTR)
                    continue;
                fail("read");
            }

            if (count == 0)
                break;

            total += (size_t)count;
        }

        buffer[total] = '\0';

        printf("[자식 PID=%ld] 수신: %s\n",
               (long)getpid(), buffer);
        printf("[자식] read로 받은 데이터: %zu바이트\n", total);

        if (close(pipe_fd[0]) == -1)
            fail("close");

        printf("[자식] 읽기 끝을 닫고 종료합니다.\n");
        _exit(EXIT_SUCCESS);
    }

    /* 부모는 송신만 하므로 읽기 끝을 닫는다. */
    if (close(pipe_fd[0]) == -1)
        fail("close");

    printf("[부모 PID=%ld] 송신: %s\n",
           (long)getpid(), message);

    size_t length = strlen(message);
    size_t total = 0;

    while (total < length) {
        ssize_t count = write(pipe_fd[1], message + total,
                              length - total);

        if (count == -1) {
            if (errno == EINTR)
                continue;
            fail("write");
        }

        if (count == 0) {
            fprintf(stderr, "write: 데이터를 보내지 못했습니다.\n");
            return EXIT_FAILURE;
        }

        total += (size_t)count;
    }

    /*
     * 쓰기 끝을 닫으면 자식은 남은 데이터를 읽은 후
     * read가 0을 반환하는 EOF를 확인할 수 있다.
     */
    if (close(pipe_fd[1]) == -1)
        fail("close");

    printf("[부모] %zu바이트 전송 후 쓰기 끝 닫기\n", total);

    int status;
    pid_t waited;

    do {
        waited = waitpid(child_pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1)
        fail("waitpid");

    if (!WIFEXITED(status)) {
        fprintf(stderr, "[부모] 자식이 비정상 종료했습니다.\n");
        return EXIT_FAILURE;
    }

    printf("[부모] 자식 종료 코드: %d\n", WEXITSTATUS(status));

    if (WEXITSTATUS(status) != 0)
        return EXIT_FAILURE;

    printf("실습 완료: 메시지 전달 및 파이프 정리 완료\n");
    return EXIT_SUCCESS;
}
