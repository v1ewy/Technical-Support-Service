#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int identifier = 0;

typedef struct Request {
    int id;
    char username[256];
    int priority;
    time_t timestamp;
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

typedef struct History {
    char department;
    Request data;
    struct HistoryReq* next;
} History;

void init_queue (struct Queue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

void create_req(Request* request) {
    request->id = identifier++;
    
    printf("Введите имя\n");
    printf("> ");
    getchar();
    
    fgets(request->username, 256, stdin);
    request->username[strcspn(request->username, "\n")] = '\0';
    
    printf("Введите приоритет (1-5)\n");
    printf("> ");
    
    scanf("%d", &request->priority);
    
    request->timestamp = time(NULL);
    
}

void add_request(Queue* department) {
    Request request;
    create_req(&request);
    
    struct QueueNode* new_node = (struct QueueNode*)malloc(sizeof(QueueNode));
    new_node->data = request;
    
    if (department->front == NULL || request.priority < department->front->data.priority) {
        new_node->next = department->front;
        department->front = new_node;
        
        if (department->rear == NULL) {
            department->rear = new_node;
        }
    } else {
        struct QueueNode* current = department->front;
        while (current->next != NULL && request.priority >= current->next->data.priority) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
        if (new_node->next == NULL) {
            department->rear = new_node;
        }
    }
}



int main(void) {
    History* history = NULL;
    Queue first;
    Queue second;
    Queue third;
    init_queue(&first);
    init_queue(&second);
    init_queue(&third);
    
    int department;
    printf("Номер подразделения(1-3)\n");
    printf("> ");
    scanf("%d", &department);
    
    switch (department) {
        case 1:
            add_request(&first);
            break;
        case 2:
            add_request(&second);
            break;
        case 3:
            add_request(&third);
            break;
            
        default:
            printf("Введен не правильный номер подразделения");
            break;
    }
    
    return 0;
}
