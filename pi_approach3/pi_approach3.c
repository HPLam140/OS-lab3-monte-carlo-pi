#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define PERMS 0644
#define MSG_KEY 0x123

struct my_msgbuf {
    long mtype;
    char mtext[200];
};

int msqid;
long send_type;
long recv_type;
int user_id;

// Luồng chuyên gửi tin nhắn
void *send_routine(void *arg) {
    struct my_msgbuf buf;
    buf.mtype = send_type;
    int len;

    while (fgets(buf.mtext, sizeof(buf.mtext), stdin) != NULL) {
        len = strlen(buf.mtext);
        /* Xóa ký tự xuống dòng '\n' ở cuối nếu có */
        if (len > 0 && buf.mtext[len - 1] == '\n') {
            buf.mtext[len - 1] = '\0';
            len--;
        }

        if (msgsnd(msqid, &buf, len + 1, 0) == -1) { /* +1 cho '\0' */
            perror("msgsnd");
            break;
        }

        /* Nếu người dùng gõ "end" thì kết thúc */
        if (strcmp(buf.mtext, "end") == 0) {
            printf("Bạn đã kết thúc cuộc trò chuyện.\n");
            exit(0);
        }
    }

    /* Trường hợp nhấn Ctrl+D (EOF) */
    strcpy(buf.mtext, "end");
    msgsnd(msqid, &buf, strlen(buf.mtext) + 1, 0);
    exit(0);
}

// Luồng chuyên nhận tin nhắn
void *recv_routine(void *arg) {
    struct my_msgbuf buf;
    int other_user = (user_id == 1) ? 2 : 1;

    for (;;) {
        /* Chỉ nhận tin nhắn có mtype khớp với recv_type */
        if (msgrcv(msqid, &buf, sizeof(buf.mtext), recv_type, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        if (strcmp(buf.mtext, "end") == 0) {
            printf("\n[User %d đã rời cuộc trò chuyện]\n", other_user);
            /* Xóa message queue khi kết thúc */
            msgctl(msqid, IPC_RMID, NULL);
            exit(0);
        }

        printf("[User %d]: \"%s\"\n", other_user, buf.mtext);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Cách dùng: %s <1 hoặc 2>\n", argv[0]);
        printf("  Ví dụ Terminal 1: %s 1\n", argv[0]);
        printf("  Ví dụ Terminal 2: %s 2\n", argv[0]);
        return 1;
    }

    user_id = atoi(argv[1]);
    if (user_id == 1) {
        send_type = 1;
        recv_type = 2;
    } else if (user_id == 2) {
        send_type = 2;
        recv_type = 1;
    } else {
        printf("Lỗi: Tham số chỉ nhận 1 (User 1) hoặc 2 (User 2)!\n");
        return 1;
    }

    /* Kết nối hoặc tạo mới Message Queue */
    if ((msqid = msgget(MSG_KEY, PERMS | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }

    printf("=== USER %d SẴN SÀNG GIAO TIẾP 2 CHIỀU ===\n", user_id);
    printf("(Gõ nội dung và nhấn Enter để gửi, gõ 'end' để thoát)\n\n");

    pthread_t snd_thread, rcv_thread;

    /* Tạo 2 luồng gửi và nhận chạy song song */
    pthread_create(&snd_thread, NULL, send_routine, NULL);
    pthread_create(&rcv_thread, NULL, recv_routine, NULL);

    /* Đợi 2 luồng kết thúc */
    pthread_join(snd_thread, NULL);
    pthread_join(rcv_thread, NULL);

    return 0;
}
