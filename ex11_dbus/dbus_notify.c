#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVICE "org.example.Lab2Notify"
#define OBJECT_PATH "/org/example/Lab2Notify"
#define INTERFACE "org.example.Lab2Notify"

static void fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static DBusConnection *connect_bus(void)
{
    DBusError error;
    dbus_error_init(&error);

    DBusConnection *connection =
        dbus_bus_get_private(DBUS_BUS_SESSION, &error);

    if (connection == NULL) {
        fprintf(stderr, "버스 연결 실패: %s\n",
                error.message ? error.message : "알 수 없는 오류");
        dbus_error_free(&error);
        exit(EXIT_FAILURE);
    }

    dbus_connection_set_exit_on_disconnect(connection, FALSE);
    return connection;
}

static int run_server(DBusConnection *connection)
{
    DBusError error;
    dbus_error_init(&error);

    int result = dbus_bus_request_name(
        connection, SERVICE, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);

    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "서비스 등록 실패: %s\n", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }

    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "이미 실행 중인 서버가 있습니다.\n");
        return EXIT_FAILURE;
    }

    printf("실습 11: D-Bus 알림 서버\n");
    printf("버스 이름: %s\n", SERVICE);
    printf("객체 경로: %s\n", OBJECT_PATH);
    printf("알림 수신 대기 중...\n");
    fflush(stdout);

    int running = 1;

    while (running) {
        if (!dbus_connection_read_write(connection, 1000)) {
            fprintf(stderr, "세션 버스 연결이 종료되었습니다.\n");
            return EXIT_FAILURE;
        }

        DBusMessage *request;
        while (running &&
               (request = dbus_connection_pop_message(connection)) != NULL) {
            if (dbus_message_get_type(request) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
                dbus_message_unref(request);
                continue;
            }

            DBusMessage *reply;
            const char *text = NULL;
            int quit = 0;

            if (!dbus_message_has_path(request, OBJECT_PATH) ||
                !dbus_message_is_method_call(request, INTERFACE, "Notify")) {
                reply = dbus_message_new_error(
                    request, DBUS_ERROR_UNKNOWN_METHOD,
                    "Unknown object, interface or method");
            } else if (!dbus_message_get_args(
                           request, &error,
                           DBUS_TYPE_STRING, &text,
                           DBUS_TYPE_INVALID)) {
                reply = dbus_message_new_error(
                    request, DBUS_ERROR_INVALID_ARGS,
                    "Notify expects one string");
                dbus_error_free(&error);
            } else {
                quit = strcmp(text, "/quit") == 0;
                printf("[서버] 알림 수신: %s\n", text);

                const char *answer = quit
                    ? "서버 종료 요청을 받았습니다."
                    : "알림 메시지를 정상 수신했습니다.";

                reply = dbus_message_new_method_return(request);
                if (reply == NULL)
                    fail("응답 메시지 생성 실패");

                if (!dbus_message_append_args(
                        reply, DBUS_TYPE_STRING, &answer,
                        DBUS_TYPE_INVALID))
                    fail("응답 문자열 추가 실패");
            }

            if (reply == NULL)
                fail("응답 메시지 생성 실패");

            if (!dbus_connection_send(connection, reply, NULL))
                fail("응답 전송 실패");

            dbus_connection_flush(connection);
            dbus_message_unref(reply);
            dbus_message_unref(request);

            printf("[서버] 응답 전송 완료\n");
            fflush(stdout);

            if (quit)
                running = 0;
        }
    }

    result = dbus_bus_release_name(connection, SERVICE, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "서비스 이름 해제 실패: %s\n", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }
    if (result != DBUS_RELEASE_NAME_REPLY_RELEASED)
        return EXIT_FAILURE;

    printf("[서버] 서비스 이름 해제 완료\n");
    return EXIT_SUCCESS;
}

static int run_client(DBusConnection *connection, const char *text)
{
    if (!dbus_validate_utf8(text, NULL)) {
        fprintf(stderr, "메시지는 올바른 UTF-8 문자열이어야 합니다.\n");
        return EXIT_FAILURE;
    }

    DBusMessage *request = dbus_message_new_method_call(
        SERVICE, OBJECT_PATH, INTERFACE, "Notify");

    if (request == NULL)
        fail("요청 메시지 생성 실패");

    if (!dbus_message_append_args(
            request, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID))
        fail("요청 문자열 추가 실패");

    printf("[클라이언트] 알림 전송: %s\n", text);

    DBusError error;
    dbus_error_init(&error);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        connection, request, 5000, &error);
    dbus_message_unref(request);

    if (reply == NULL) {
        fprintf(stderr, "호출 실패: %s\n",
                error.message ? error.message : "알 수 없는 오류");
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }

    const char *answer;
    if (!dbus_message_get_args(
            reply, &error, DBUS_TYPE_STRING, &answer,
            DBUS_TYPE_INVALID)) {
        fprintf(stderr, "응답 해석 실패: %s\n", error.message);
        dbus_error_free(&error);
        dbus_message_unref(reply);
        return EXIT_FAILURE;
    }

    printf("[클라이언트] 서버 응답: %s\n", answer);
    dbus_message_unref(reply);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    int server = argc == 2 && strcmp(argv[1], "server") == 0;
    int client = argc == 3 && strcmp(argv[1], "client") == 0;

    if (!server && !client) {
        fprintf(stderr,
                "사용법:\n  %s server\n  %s client \"메시지\"\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    DBusConnection *connection = connect_bus();
    int result = server
        ? run_server(connection)
        : run_client(connection, argv[2]);

    dbus_connection_close(connection);
    dbus_connection_unref(connection);
    printf("[%s] D-Bus 연결 정리 완료\n",
           server ? "서버" : "클라이언트");
    return result;
}
