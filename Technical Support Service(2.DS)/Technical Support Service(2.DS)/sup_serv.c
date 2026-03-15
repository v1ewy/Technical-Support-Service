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

typedef struct StackNode {
    Request data;
    int department;
    struct StackNode* next;
} StackNode;

typedef struct {
    StackNode* top;
    int size;
} Stack;

void init_stack(Stack* s) {
    s->top = NULL;
    s->size = 0;
}

void init_queue (struct Queue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

char* time2str(time_t time) {
    static char time_str[30];
    struct tm *tm_info = localtime(&time);
    if (tm_info) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        strcpy(time_str, "Invalid time");
    }
    return time_str;
}

int get_department(void) {
    int department;
    int result;
    char c;
    do {
        printf("Номер подразделения (1-3)\n");
        printf("> ");
        result = scanf("%d", &department);
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (result != 1) {
            printf("Ошибка! Введите целое число.\n");
        } else if (department < 1 || department > 3) {
            printf("Ошибка! Число должно быть от 1 до 3.\n");
            result = 0;
        }
    } while (result != 1);
    return department;
}

void create_request(Request* request) {
    request->id = ++identifier;
    
    printf("Введите имя\n");
    printf("> ");
    fgets(request->username, 256, stdin);
    request->username[strcspn(request->username, "\n")] = '\0';
    
    int priority;
    char c;
    do {
        printf("Введите приоритет (1-5)\n");
        printf("> ");
        int result = scanf("%d", &priority);
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (result != 1) {
            printf("Ошибка! Введите целое число.\n");
        } else if (priority < 1 || priority > 5) {
            printf("Ошибка! Приоритет должен быть от 1 до 5.\n");
        } else {
            request->priority = priority;
            break;
        }
        
    } while (1);
    
    request->timestamp = time(NULL);
}

void add_request(Queue* department) {
    Request request;
    create_request(&request);
    
    struct QueueNode* new_node = (struct QueueNode*)malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        printf("Ошибка выделения памяти.\n");
        return;
    }
    
    new_node->data = request;
    new_node->next = NULL;
    
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
    
    department->size++;
}

void peek_request(Queue* department) {
    if (department->front == NULL) {
        printf("Очередь пуста.\n");
        return;
    }
    
    struct QueueNode* first = department->front;
    char* time = time2str(first->data.timestamp);
    printf("%d. [Приоритет:%d] %s (%s)\n", first->data.id,
           first->data.priority, first->data.username, time);
}



void process_request(struct Queue* department) {
    if (department->front == NULL) {
        printf("Очередь пуста.\n");
        return;
    }
    
    struct QueueNode* complete = department->front;
    
    department->front = department->front->next;
    if (department->front == NULL) {
        department->rear = NULL;
    }
    
    printf("Заявка №%d обработана.\n", complete->data.id);
    free(complete);
    department->size--;
}

QueueNode* remove_node_by_id(Queue* queue, int id) {
    if (queue->front == NULL) return NULL;

    QueueNode *prev = NULL;
    QueueNode *curr = queue->front;

    while (curr != NULL && curr->data.id != id) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) return NULL;

    if (prev == NULL) {
        queue->front = curr->next;
        if (queue->rear == curr) {
            queue->rear = NULL;
        }
    } else {
        prev->next = curr->next;
        if (queue->rear == curr) {
            queue->rear = prev;
        }
    }
    queue->size--;
    curr->next = NULL;
    return curr;
}

void insert_node_by_priority(Queue* queue, QueueNode* node) {
    if (queue->front == NULL) {
        queue->front = node;
        queue->rear = node;
        queue->size++;
        return;
    }

    if (node->data.priority < queue->front->data.priority) {
        node->next = queue->front;
        queue->front = node;
        queue->size++;
        return;
    }

    QueueNode *current = queue->front;
    while (current->next != NULL && node->data.priority >= current->next->data.priority) {
        current = current->next;
    }

    node->next = current->next;
    current->next = node;
    if (node->next == NULL) {
        queue->rear = node;
    }
    queue->size++;
}

void move_request(Queue* from_dept, Queue* to_dept, int id) {
    if (from_dept == to_dept) {
        printf("Ошибка: исходное и целевое подразделения совпадают.\n");
        return;
    }

    QueueNode* node = remove_node_by_id(from_dept, id);
    if (node == NULL) {
        printf("Заявка %d не найдена в указанном подразделении.\n", id);
        return;
    }

    insert_node_by_priority(to_dept, node);

    char* time = time2str(node->data.timestamp);
    printf("Заявка %d (пользователь %s, приоритет %d, время %s) перемещена.\n",
           node->data.id, node->data.username, node->data.priority, time);
}

void cancel_request(Queue* depts[3], Stack* canceled, int id) {
    for (int i = 0; i < 3; i++) {
        QueueNode* node = remove_node_by_id(depts[i], id);
        if (node != NULL) {
            StackNode* st_node = (StackNode*)malloc(sizeof(StackNode));
            if (st_node == NULL) {
                printf("Ошибка памяти при отмене.\n");
                free(node);
                return;
            }
            st_node->data = node->data;
            st_node->department = i + 1;
            st_node->next = canceled->top;
            canceled->top = st_node;
            canceled->size++;

            free(node);
            printf("Заявка №%d отменена и помещена в стек.\n", id);
            return;
        }
    }
    printf("Заявка №%d не найдена.\n", id);
}

void restore_last_canceled(Queue* depts[3], Stack* canceled) {
    if (canceled->top == NULL) {
        printf("Стек отменённых пуст.\n");
        return;
    }

    StackNode* st_node = canceled->top;
    canceled->top = st_node->next;
    canceled->size--;

    QueueNode* q_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (q_node == NULL) {
        printf("Ошибка памяти при восстановлении.\n");
        free(st_node);
        return;
    }
    q_node->data = st_node->data;
    q_node->next = NULL;

    int dept_idx = st_node->department - 1;
    insert_node_by_priority(depts[dept_idx], q_node);

    printf("Заявка №%d восстановлена в подразделение %d.\n",
           st_node->data.id, st_node->department);
    free(st_node);
}

void find_by_id(Queue* depts[3], Stack* canceled, int id) {
    for (int i = 0; i < 3; i++) {
        QueueNode* curr = depts[i]->front;
        int pos = 1;
        while (curr) {
            if (curr->data.id == id) {
                printf("Заявка №%d найдена в подразделении %d, позиция %d (в очереди).\n",
                       id, i+1, pos);
                return;
            }
            curr = curr->next;
            pos++;
        }
    }

    StackNode* st_curr = canceled->top;
    int st_pos = 1;
    while (st_curr) {
        if (st_curr->data.id == id) {
            printf("Заявка №%d найдена в стеке отменённых, позиция %d.\n", id, st_pos);
            return;
        }
        st_curr = st_curr->next;
        st_pos++;
    }

    printf("Заявка с №%d не найдена.\n", id);
}

void find_by_username(Queue* depts[3], Stack* canceled, const char* name) {
    int found = 0;
    for (int i = 0; i < 3; i++) {
        QueueNode* curr = depts[i]->front;
        int pos = 1;
        while (curr) {
            if (strcmp(curr->data.username, name) == 0) {
                printf("Пользователь '%s' (%d) найден в подразделении %d, позиция %d (в очереди).\n",
                       name, curr->data.id, i+1, pos);
                found = 1;
            }
            curr = curr->next;
            pos++;
        }
    }

    StackNode* st_curr = canceled->top;
    int st_pos = 1;
    while (st_curr) {
        if (strcmp(st_curr->data.username, name) == 0) {
            printf("Пользователь '%s' (%d) найден в стеке отменённых, позиция %d.\n",
                   name, st_curr->data.id, st_pos);
            found = 1;
        }
        st_curr = st_curr->next;
        st_pos++;
    }

    if (!found) {
        printf("Пользователь '%s' не найден.\n", name);
    }
}

void clear_queue(struct Queue* queue, int num) {
    int cnt_req = 0;
    struct QueueNode* current = queue->front;
    struct QueueNode* next;
    
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
        cnt_req++;
    }
    
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    printf("Очищено %d запросов из очереди номер %d.\n", cnt_req, num);
}

void clear_stack(Stack* canceled) {
    int cnt_req = 0;
    StackNode* st_curr = canceled->top;
    
    while (st_curr) {
        StackNode* tmp = st_curr;
        st_curr = st_curr->next;
        free(tmp);
        cnt_req++;
    }
    canceled->top = NULL;
    canceled->size = 0;
    printf("Очищено %d запросов из памяти.\n", cnt_req);
}

int main(void) {
    Queue depts[3];
    for (int i = 0; i < 3; i++) init_queue(&depts[i]);

    Stack canceled;
    init_stack(&canceled);

    Queue* queues[3] = { &depts[0], &depts[1], &depts[2] };

    printf("=== Добавление заявок ===\n");
    add_request(queues[0]);
    add_request(queues[1]);
    add_request(queues[2]);

    printf("\n=== Первая заявка в подразделении 1 ===\n");
    peek_request(queues[0]);

    printf("\n=== Перемещение заявки ID=1 из 1 в 2 ===\n");
    move_request(queues[0], queues[1], 1);
    peek_request(queues[1]);

    printf("\n=== Отмена заявки ID=2 ===\n");
    cancel_request(queues, &canceled, 2);

    printf("\n=== Поиск по ID=2 ===\n");
    find_by_id(queues, &canceled, 2);

    printf("\n=== Восстановление последней отменённой ===\n");
    restore_last_canceled(queues, &canceled);

    printf("\n=== Обработка заявки в подразделении 2 ===\n");
    process_request(queues[1]);

    for (int i = 0; i < 3; i++) clear_queue(queues[i], i+1);
    
    clear_stack(&canceled);
    
    return 0;
}
