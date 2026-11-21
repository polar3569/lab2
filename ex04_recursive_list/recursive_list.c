#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static int list_recursive(const char *path, unsigned int depth)
{
    struct stat info;

    /* 링크의 대상이 아닌 링크 자체의 정보를 조회한다. */
    if (lstat(path, &info) == -1) {
        perror(path);
        return 1;
    }

    const char *type;
    if (S_ISLNK(info.st_mode))
        type = "LINK";
    else if (S_ISDIR(info.st_mode))
        type = "DIR";
    else if (S_ISREG(info.st_mode))
        type = "FILE";
    else
        type = "OTHER";

    for (unsigned int i = 0; i < depth; i++)
        printf("  ");
    printf("[%s] %s\n", type, path);

    /* 일반 디렉토리만 방문하여 링크로 인한 순환을 방지한다. */
    if (!S_ISDIR(info.st_mode))
        return 0;

    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror(path);
        return 1;
    }

    int result = 0;
    struct dirent *entry;

    for (;;) {
        errno = 0;
        entry = readdir(dir);

        if (entry == NULL) {
            if (errno != 0) {
                perror("readdir");
                result = 1;
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        size_t base_length = strlen(path);
        size_t name_length = strlen(entry->d_name);

        if (base_length > SIZE_MAX - name_length - 2) {
            fprintf(stderr, "경로 길이가 너무 깁니다.\n");
            result = 1;
            continue;
        }

        size_t length = base_length + name_length + 2;
        char *child_path = malloc(length);

        if (child_path == NULL) {
            perror("malloc");
            result = 1;
            break;
        }

        snprintf(child_path, length, "%s/%s", path, entry->d_name);

        if (list_recursive(child_path, depth + 1) != 0)
            result = 1;

        free(child_path);
    }

    if (closedir(dir) == -1) {
        perror("closedir");
        result = 1;
    }

    return result;
}

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "사용법: %s [디렉토리 경로]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argc == 2 ? argv[1] : ".";

    printf("실습 4: 재귀 디렉토리 목록\n");
    printf("심볼릭 링크는 표시만 하고 따라가지 않습니다.\n");

    return list_recursive(path, 0) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
