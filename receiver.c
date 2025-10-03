//sender_chat.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>

#define MSG_KEY 1234

struct msg_buffer {
    long msg_type;
    char sender[20];
    char text[100];
};

int msgid;
struct msg_buffer message;

void handle_exit(int sig) {
    strcpy(message.text, "__EXIT__");
    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
    printf("\n[System]: Exiting chat...\n");
    exit(0);
}

int main() {
    msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    message.msg_type = 1;

    printf("Enter your name: ");
    fgets(message.sender, sizeof(message.sender), stdin);
    message.sender[strcspn(message.sender, "\n")] = '\0';

    // Catch Ctrl+C
    signal(SIGINT, handle_exit);

    printf("Type messages (Ctrl+C to quit):\n");
    while (1) {
        if (!fgets(message.text, sizeof(message.text), stdin)) {
            handle_exit(0); // if input stream closed (e.g., terminal closed)
        }
        message.text[strcspn(message.text, "\n")] = '\0';

        if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
    }

    return 0;
}
