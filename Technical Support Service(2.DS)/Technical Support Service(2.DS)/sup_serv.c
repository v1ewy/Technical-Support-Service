#include <stdio.h>

typedef struct Request {
    int id;
    char username[256];
    char priority;
    char timestamp[10];
} Request;

typedef struct QueueNode {
    Request data;
    struct QueueNode* next;
} QueueNode;

typedef struct Queue {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

typedef struct HistoryReq {
    char department;
    Request data;
    struct HistoryReq* next;
} HistoryReq;

int main(void) {
    printf("Hello, World!\n");
    return 0;
}
