#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>

static char shm_name[64];
static int shm_exists = 0;

static void cleanup(void)
{
    if (shm_exists && shm_unlink(shm_name) == -1)
        perror("shm_unlink");
}

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "사용법: %s 원본파일 복사본파일\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return EXIT_FAILURE;

    printf("실습 10: POSIX 공유 메모리/mmap 파일 복사\n");

    int source = open(argv[1], O_RDONLY);
    if (source == -1)
        fail("open source");

    struct stat info;
    if (fstat(source, &info) == -1)
        fail("fstat");

    if (!S_ISREG(info.st_mode) || info.st_size < 0 ||
        (uintmax_t)info.st_size > (uintmax_t)SIZE_MAX) {
        fprintf(stderr, "처리 가능한 크기의 일반 파일을 지정하세요.\n");
        close(source);
        return EXIT_FAILURE;
    }

    size_t size = (size_t)info.st_size;

    /* 빈 파일도 공유 메모리는 최소 1바이트로 생성한다. */
    size_t map_size = size == 0 ? 1 : size;

    snprintf(shm_name, sizeof(shm_name), "/lab2_copy_%ld",
             (long)getpid());

    if (atexit(cleanup) != 0)
        return EXIT_FAILURE;

    int shared_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shared_fd == -1)
        fail("shm_open");
    shm_exists = 1;

    if (ftruncate(shared_fd, (off_t)map_size) == -1)
        fail("ftruncate shared memory");

    void *shared = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, shared_fd, 0);
    if (shared == MAP_FAILED)
        fail("mmap shared memory");

    if (close(shared_fd) == -1)
        fail("close shared memory");

    /*
     * 이름은 바로 제거하되 매핑은 유지한다.
     * fork로 상속된 매핑까지 모두 해제되면 객체가 소멸한다.
     */
    if (shm_unlink(shm_name) == -1)
        fail("shm_unlink");
    shm_exists = 0;

    printf("[shm_open/mmap] 공유 메모리 생성 및 매핑 완료\n");
    printf("[shm_unlink] 이름 제거 완료, 매핑은 유지\n");

    size_t total = 0;
    while (total < size) {
        size_t remaining = size - total;
        size_t chunk = remaining > 65536 ? 65536 : remaining;
        ssize_t count = read(source, (char *)shared + total, chunk);

        if (count == -1) {
            if (errno == EINTR)
                continue;
            fail("read");
        }
        if (count == 0) {
            fprintf(stderr, "복사 도중 원본 파일 크기가 줄었습니다.\n");
            return EXIT_FAILURE;
        }
        total += (size_t)count;
    }

    if (close(source) == -1)
        fail("close source");

    printf("[부모 PID=%ld] 원본 %zu바이트를 공유 메모리에 저장\n",
           (long)getpid(), size);

    pid_t child = fork();
    if (child == -1)
        fail("fork");

    if (child == 0) {
        /* 기존 파일과 원본을 덮어쓰지 않는다. */
        int target = open(argv[2], O_CREAT | O_EXCL | O_RDWR, 0644);
        if (target == -1)
            fail("open destination");

        if (ftruncate(target, (off_t)size) == -1)
            fail("ftruncate destination");

        if (size > 0) {
            void *destination = mmap(NULL, size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, target, 0);
            if (destination == MAP_FAILED)
                fail("mmap destination");

            memcpy(destination, shared, size);

            if (msync(destination, size, MS_SYNC) == -1)
                fail("msync");
            if (munmap(destination, size) == -1)
                fail("munmap destination");
        }

        if (close(target) == -1)
            fail("close destination");
        if (munmap(shared, map_size) == -1)
            fail("munmap child");

        printf("[자식 PID=%ld] 복사본에 %zu바이트 기록 완료\n",
               (long)getpid(), size);
        _exit(EXIT_SUCCESS);
    }

    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1)
        fail("waitpid");

    if (munmap(shared, map_size) == -1)
        fail("munmap parent");

    printf("[munmap] 부모 공유 메모리 매핑 해제 완료\n");

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "자식의 파일 복사가 실패했습니다.\n");
        return EXIT_FAILURE;
    }

    printf("복사 완료: %s -> %s (%zu바이트)\n",
           argv[1], argv[2], size);
    return EXIT_SUCCESS;
}
