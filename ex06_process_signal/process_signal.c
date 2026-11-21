#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

static void wait_signal(const sigset_t *set, const char *role)
{
    int received;
    int error = sigwait(set, &received);

    if (error != 0) {
        errno = error;
        fail("sigwait");
    }

    printf("[%s] 시그널 수신: %s\n", role,
           received == SIGUSR1 ? "SIGUSR1" : "SIGUSR2");
}

int main(void)
{
    sigset_t blocked, child_set, parent_set;
    sigset_t original_mask;

    /* fork 이후 출력이 중복되거나 버퍼에 남지 않도록 한다. */
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return EXIT_FAILURE;

    printf("실습 6: 프로세스 생성과 시그널 통신\n");

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    sigaddset(&blocked, SIGUSR2);

    /*
     * 먼저 도착한 시그널도 sigwait에서 받을 수 있도록
     * fork 전에 차단한다. 자식도 이 설정을 상속한다.
     */
    if (sigprocmask(SIG_BLOCK, &blocked, &original_mask) == -1)
        fail("sigprocmask");

    sigemptyset(&child_set);
    sigaddset(&child_set, SIGUSR1);

    sigemptyset(&parent_set);
    sigaddset(&parent_set, SIGUSR2);

    pid_t parent_pid = getpid();
    pid_t child_pid = fork();

    if (child_pid == -1)
        fail("fork");

    if (child_pid == 0) {
        printf("[자식] PID=%ld, PPID=%ld\n",
               (long)getpid(), (long)getppid());
        printf("[자식] 부모의 SIGUSR1을 기다립니다.\n");

        wait_signal(&child_set, "자식");

        printf("[자식] 부모 PID=%ld에 SIGUSR2 응답 전송\n",
               (long)parent_pid);
        if (kill(parent_pid, SIGUSR2) == -1)
            fail("kill");

        printf("[자식] 종료 코드 7로 종료합니다.\n");
        _exit(7);
    }

    printf("[부모] PID=%ld, 자식 PID=%ld\n",
           (long)getpid(), (long)child_pid);
    printf("[부모] 자식에게 SIGUSR1 전송\n");

    if (kill(child_pid, SIGUSR1) == -1)
        fail("kill");

    printf("[부모] 자식의 SIGUSR2 응답을 기다립니다.\n");
    wait_signal(&parent_set, "부모");

    int status;
    pid_t waited;

    do {
        waited = waitpid(child_pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1)
        fail("waitpid");

    printf("[부모] waitpid로 자식 PID=%ld 회수 완료\n",
           (long)waited);

    if (WIFEXITED(status)) {
        printf("[부모] 자식 정상 종료, 종료 코드=%d\n",
               WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[부모] 자식이 시그널 %d로 종료됨\n",
               WTERMSIG(status));
    }

    if (sigprocmask(SIG_SETMASK, &original_mask, NULL) == -1)
        fail("sigprocmask");

    printf("[부모] 실습 완료, 종료 코드 0으로 종료합니다.\n");
    return EXIT_SUCCESS;
}
