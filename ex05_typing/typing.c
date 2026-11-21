#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <time.h>

static void fail(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

/* 멀티바이트 문자열을 문자 단위 비교용 문자열로 변환한다. */
static wchar_t *to_wide(const char *text)
{
    size_t count = mbstowcs(NULL, text, 0);
    if (count == (size_t)-1) {
        fprintf(stderr, "문자 변환 실패: UTF-8 로케일을 확인하세요.\n");
        exit(EXIT_FAILURE);
    }

    wchar_t *result = malloc((count + 1) * sizeof(*result));
    if (result == NULL)
        fail("malloc");

    if (mbstowcs(result, text, count + 1) == (size_t)-1) {
        free(result);
        fprintf(stderr, "문자 변환에 실패했습니다.\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

static double elapsed(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

int main(void)
{
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "로케일 설정에 실패했습니다.\n");
        return EXIT_FAILURE;
    }

    const char *sentences[] = {
        "Linux makes programming interesting.",
        "Practice typing every day.",
        "리눅스에서 파일과 프로세스를 공부한다."
    };
    size_t sentence_count = sizeof(sentences) / sizeof(sentences[0]);
    char *line = NULL;
    size_t capacity = 0;
    size_t total_chars = 0;
    size_t total_errors = 0;
    double total_seconds = 0.0;

    printf("실습 5: 타자 연습\n");
    printf("제시된 문장을 직접 입력한 뒤 Enter를 누르세요.\n");
    printf("공백과 문장부호도 문자 수와 오타 비교에 포함합니다.\n");

    for (size_t i = 0; i < sentence_count; i++) {
        struct timespec start, end;
        printf("\n[%zu/%zu] %s\n", i + 1, sentence_count, sentences[i]);
        printf("입력: ");
        fflush(stdout);

        if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
            fail("clock_gettime");

        ssize_t length = getline(&line, &capacity, stdin);

        if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
            fail("clock_gettime");

        if (length == -1) {
            if (ferror(stdin))
                perror("getline");
            else
                fprintf(stderr, "\n입력이 종료되어 연습을 중단합니다.\n");
            free(line);
            return EXIT_FAILURE;
        }

        if (length > 0 && line[length - 1] == '\n')
            line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r')
            line[--length] = '\0';

        wchar_t *expected = to_wide(sentences[i]);
        wchar_t *typed = to_wide(line);
        size_t expected_len = wcslen(expected);
        size_t typed_len = wcslen(typed);
        size_t common = expected_len < typed_len ? expected_len : typed_len;
        size_t errors = expected_len > typed_len
                      ? expected_len - typed_len : typed_len - expected_len;

        /* 같은 위치의 불일치와 길이 차이를 오타로 계산한다. */
        for (size_t j = 0; j < common; j++) {
            if (expected[j] != typed[j])
                errors++;
        }

        double seconds = elapsed(start, end);
        double cpm = seconds > 0.0 ? typed_len * 60.0 / seconds : 0.0;

        printf("입력 문자: %zu자 | 오타: %zu회\n", typed_len, errors);
        printf("소요 시간: %.2f초 | 분당 타자수: %.2f자/분\n", seconds, cpm);

        total_chars += typed_len;
        total_errors += errors;
        total_seconds += seconds;

        free(expected);
        free(typed);
    }

    printf("\n=== 전체 결과 ===\n");
    printf("완료 문장: %zu개\n", sentence_count);
    printf("총 입력 문자: %zu자\n", total_chars);
    printf("총 오타 횟수: %zu회\n", total_errors);
    printf("총 소요 시간: %.2f초\n", total_seconds);
    printf("평균 분당 타자수: %.2f자/분\n",
           total_seconds > 0.0
           ? total_chars * 60.0 / total_seconds : 0.0);

    free(line);
    return EXIT_SUCCESS;
}
