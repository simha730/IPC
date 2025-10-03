//receiver_chat.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MSG_KEY 1234

struct msg_buffer {
    long msg_type;
    char sender[20];
    char text[100];
};

int main() {
    int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    struct msg_buffer message;

    printf("Listening for messages...\n");
    while (1) {
        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        // Check for exit signal
        if (strcmp(message.text, "__EXIT__") == 0) {
            printf("[System]: %s has left the chat.\n", message.sender);
            break;
        }

        printf("[%s]: %s\n", message.sender, message.text);
    }

    // Cleanup
    msgctl(msgid, IPC_RMID, NULL);
    return 0;
}

