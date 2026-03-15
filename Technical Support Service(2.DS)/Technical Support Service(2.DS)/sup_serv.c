#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================ Константы ============================
#define MAX_QUEUE_SIZE 5      // максимальный размер одной очереди
#define MIN_TOTAL_SIZE 3       // минимальный суммарный размер для объединения

// ============================ Структуры данных ============================

// Заявка
typedef struct Request {
    int id;
    char username[256];
    int priority;
    time_t timestamp;
} Request;

// Узел очереди
typedef struct QueueNode {
    Request data;
    struct QueueNode* next;
} QueueNode;

// Очередь (приоритетная)
typedef struct Queue {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

// Узел стека отменённых
typedef struct StackNode {
    Request data;
    int department;
    struct StackNode* next;
} StackNode;

// Стек отменённых
typedef struct {
    StackNode* top;
    int size;
} Stack;

// Подразделение (содержит несколько очередей для балансировки)
typedef struct Subdivision {
    Queue** queues;
    int count;
    int capacity;
} Subdivision;

// ============================ Глобальные переменные ============================
int identifier = 0;           // генерация уникальных ID

// ============================ Утилиты ============================

// Инициализация очереди
void init_queue(Queue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

// Инициализация стека
void init_stack(Stack* s) {
    s->top = NULL;
    s->size = 0;
}

// Инициализация подразделения
void init_subdivision(Subdivision* sub) {
    sub->count = 1;
    sub->capacity = 2;
    sub->queues = (Queue**)malloc(sub->capacity * sizeof(Queue*));
    sub->queues[0] = (Queue*)malloc(sizeof(Queue));
    init_queue(sub->queues[0]);
}

// Преобразование time_t в строку (YYYY-MM-DD HH:MM:SS)
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

// Очистка буфера ввода
void clear_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Безопасный ввод целого числа с проверкой диапазона
int input_int(const char* prompt, int min, int max) {
    int value;
    int result;
    do {
        printf("%s (%d-%d): ", prompt, min, max);
        result = scanf("%d", &value);
        clear_input();
        if (result != 1) {
            printf("Ошибка! Введите целое число.\n");
        } else if (value < min || value > max) {
            printf("Ошибка! Число должно быть от %d до %d.\n", min, max);
        } else {
            return value;
        }
    } while (1);
}

// Создание новой заявки
void create_request(Request* request) {
    request->id = ++identifier;
    
    printf("Введите имя пользователя: ");
    fgets(request->username, 256, stdin);
    request->username[strcspn(request->username, "\n")] = '\0';
    if (strlen(request->username) == 0) {
        strcpy(request->username, "Unknown");
    }
    
    request->priority = input_int("Введите приоритет", 1, 5);
    request->timestamp = time(NULL);
}

// ============================ Базовые операции с очередью ============================

// Удаление узла по id из конкретной очереди
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

// Вставка уже существующего узла в очередь с сохранением порядка по приоритету
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

// ============================ Операции очистки памяти ============================

// Очистка памяти одной очереди
void clear_queue(Queue* queue) {
    QueueNode* curr = queue->front;
    while (curr) {
        QueueNode* next = curr->next;
        free(curr);
        curr = next;
    }
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

// Очистка стека
void clear_stack(Stack* stack) {
    StackNode* curr = stack->top;
    while (curr) {
        StackNode* next = curr->next;
        free(curr);
        curr = next;
    }
    stack->top = NULL;
    stack->size = 0;
}

// Очистка памяти подразделения
void clear_subdivision(Subdivision* sub) {
    for (int i = 0; i < sub->count; i++) {
        clear_queue(sub->queues[i]);
        free(sub->queues[i]);
    }
    free(sub->queues);
    sub->queues = NULL;
    sub->count = 0;
    sub->capacity = 0;
}

// ============================ Балансировка очередей (подразделения) ============================

// Суммарное количество заявок в подразделении
int get_total_size(Subdivision* sub) {
    int total = 0;
    for (int i = 0; i < sub->count; i++) {
        total += sub->queues[i]->size;
    }
    return total;
}

// Разделение очереди на две
void split_queue(Subdivision* sub, int idx) {
    Queue* old_q = sub->queues[idx];

    Queue* new_q1 = (Queue*)malloc(sizeof(Queue));
    Queue* new_q2 = (Queue*)malloc(sizeof(Queue));
    init_queue(new_q1);
    init_queue(new_q2);

    QueueNode* curr = old_q->front;
    int turn = 0;
    while (curr) {
        QueueNode* next = curr->next;
        curr->next = NULL;

        if (turn % 2 == 0) {
            if (new_q1->rear == NULL) {
                new_q1->front = new_q1->rear = curr;
            } else {
                new_q1->rear->next = curr;
                new_q1->rear = curr;
            }
            new_q1->size++;
        } else {
            if (new_q2->rear == NULL) {
                new_q2->front = new_q2->rear = curr;
            } else {
                new_q2->rear->next = curr;
                new_q2->rear = curr;
            }
            new_q2->size++;
        }
        curr = next;
        turn++;
    }

    free(old_q);

    if (sub->count + 1 > sub->capacity) {
        sub->capacity *= 2;
        sub->queues = (Queue**)realloc(sub->queues, sub->capacity * sizeof(Queue*));
    }
    for (int i = sub->count - 1; i > idx; i--) {
        sub->queues[i + 1] = sub->queues[i];
    }
    sub->queues[idx] = new_q1;
    sub->queues[idx + 1] = new_q2;
    sub->count++;

    printf("*** Очередь %d разделена на две. Теперь в подразделении %d очередей.\n", idx + 1, sub->count);
}

// Объединение всех очередей подразделения в одну
void merge_queues(Subdivision* sub) {
    if (sub->count <= 1) return;

    Queue* merged = (Queue*)malloc(sizeof(Queue));
    init_queue(merged);

    for (int i = 0; i < sub->count; i++) {
        Queue* q = sub->queues[i];
        QueueNode* curr = q->front;
        while (curr) {
            QueueNode* next = curr->next;
            curr->next = NULL;
            insert_node_by_priority(merged, curr);
            curr = next;
        }
        q->front = q->rear = NULL;
        q->size = 0;
    }

    for (int i = 0; i < sub->count; i++) {
        free(sub->queues[i]);
    }
    free(sub->queues);

    sub->queues = (Queue**)malloc(sizeof(Queue*));
    sub->queues[0] = merged;
    sub->count = 1;
    sub->capacity = 1;

    printf("*** Очереди объединены в одну. Теперь в подразделении 1 очередь.\n");
}

// Вставка готового узла в подразделение
void insert_node_into_subdivision(Subdivision* sub, QueueNode* node) {
    int min_idx = 0;
    for (int i = 1; i < sub->count; i++) {
        if (sub->queues[i]->size < sub->queues[min_idx]->size)
            min_idx = i;
    }
    insert_node_by_priority(sub->queues[min_idx], node);
    
    if (sub->queues[min_idx]->size > MAX_QUEUE_SIZE) {
        split_queue(sub, min_idx);
    }
}

// Добавление новой заявки в подразделение
void add_request_to_subdivision(Subdivision* sub) {
    Request req;
    create_request(&req);

    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        printf("Ошибка выделения памяти.\n");
        return;
    }
    new_node->data = req;
    new_node->next = NULL;

    insert_node_into_subdivision(sub, new_node);
    printf("Заявка №%d добавлена в подразделение.\n", req.id);
}

// Просмотр первой заявки в подразделении
void peek_subdivision(Subdivision* sub) {
    int best_idx = -1;
    Request* best_req = NULL;

    for (int i = 0; i < sub->count; i++) {
        if (sub->queues[i]->front == NULL) continue;
        if (best_idx == -1 || sub->queues[i]->front->data.priority < best_req->priority) {
            best_idx = i;
            best_req = &sub->queues[i]->front->data;
        }
    }

    if (best_idx == -1) {
        printf("Все очереди подразделения пусты.\n");
        return;
    }

    QueueNode* first = sub->queues[best_idx]->front;
    printf("Первая заявка (очередь %d): ID %d, приоритет %d, пользователь %s, время %s\n",
           best_idx + 1, first->data.id, first->data.priority,
           first->data.username, time2str(first->data.timestamp));
}

// Обработка (удаление) заявки с наивысшим приоритетом в подразделении
void process_subdivision(Subdivision* sub) {
    int best_idx = -1;
    Request* best_req = NULL;

    for (int i = 0; i < sub->count; i++) {
        if (sub->queues[i]->front == NULL) continue;
        if (best_idx == -1 || sub->queues[i]->front->data.priority < best_req->priority) {
            best_idx = i;
            best_req = &sub->queues[i]->front->data;
        }
    }

    if (best_idx == -1) {
        printf("Все очереди подразделения пусты.\n");
        return;
    }

    Queue* q = sub->queues[best_idx];
    QueueNode* to_remove = q->front;
    q->front = q->front->next;
    if (q->front == NULL) q->rear = NULL;
    q->size--;

    printf("Заявка №%d обработана.\n", to_remove->data.id);
    free(to_remove);

    int total = get_total_size(sub);
    if (total < MIN_TOTAL_SIZE && sub->count > 1) {
        merge_queues(sub);
    }
}

// Перемещение заявки между подразделениями
void move_request_between_subdivisions(Subdivision* from, Subdivision* to, int id) {
    for (int i = 0; i < from->count; i++) {
        QueueNode* node = remove_node_by_id(from->queues[i], id);
        if (node) {
            insert_node_into_subdivision(to, node);
            printf("Заявка №%d перемещена.\n", id);
            return;
        }
    }
    printf("Заявка №%d не найдена в исходном подразделении.\n", id);
}

// Отмена заявки (перемещение в стек)
void cancel_request_subdivision(Subdivision subs[3], Stack* canceled, int id) {
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* node = remove_node_by_id(sub->queues[i], id);
            if (node != NULL) {
                StackNode* st_node = (StackNode*)malloc(sizeof(StackNode));
                if (st_node == NULL) {
                    printf("Ошибка памяти при отмене.\n");
                    free(node);
                    return;
                }
                st_node->data = node->data;
                st_node->department = d + 1;
                st_node->next = canceled->top;
                canceled->top = st_node;
                canceled->size++;

                free(node);
                printf("Заявка №%d отменена и помещена в стек.\n", id);
                return;
            }
        }
    }
    printf("Заявка №%d не найдена.\n", id);
}

// Восстановление последней отменённой заявки
void restore_last_canceled_subdivision(Subdivision subs[3], Stack* canceled) {
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
    insert_node_into_subdivision(&subs[dept_idx], q_node);

    printf("Заявка №%d восстановлена в подразделение %d.\n",
           st_node->data.id, st_node->department);
    free(st_node);
}

// Поиск заявки по ID
void find_by_id_subdivision(Subdivision subs[3], Stack* canceled, int id) {
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* curr = sub->queues[i]->front;
            int pos = 1;
            while (curr) {
                if (curr->data.id == id) {
                    printf("Заявка №%d найдена в подразделении %d, очередь %d, позиция %d (в очереди).\n",
                           id, d+1, i+1, pos);
                    return;
                }
                curr = curr->next;
                pos++;
            }
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

    printf("Заявка №%d не найдена.\n", id);
}

// Поиск по имени пользователя
void find_by_username_subdivision(Subdivision subs[3], Stack* canceled, const char* name) {
    int found = 0;
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* curr = sub->queues[i]->front;
            int pos = 1;
            while (curr) {
                if (strcmp(curr->data.username, name) == 0) {
                    printf("Пользователь '%s' (ID %d) найден в подразделении %d, очередь %d, позиция %d (в очереди).\n",
                           name, curr->data.id, d+1, i+1, pos);
                    found = 1;
                }
                curr = curr->next;
                pos++;
            }
        }
    }

    StackNode* st_curr = canceled->top;
    int st_pos = 1;
    while (st_curr) {
        if (strcmp(st_curr->data.username, name) == 0) {
            printf("Пользователь '%s' (ID %d) найден в стеке отменённых, позиция %d.\n",
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

// Вывод состояния всех структур
void print_all_status(Subdivision subs[3], Stack* canceled) {
    printf("\n========== ТЕКУЩЕЕ СОСТОЯНИЕ ==========\n");
    
    for (int d = 0; d < 3; d++) {
        printf("\nПОДРАЗДЕЛЕНИЕ %d (всего очередей: %d):\n", d+1, subs[d].count);
        if (get_total_size(&subs[d]) == 0) {
            printf("  Нет заявок.\n");
        } else {
            for (int i = 0; i < subs[d].count; i++) {
                Queue* q = subs[d].queues[i];
                printf("  Очередь %d (заявок: %d):\n", i+1, q->size);
                QueueNode* curr = q->front;
                int pos = 1;
                while (curr) {
                    printf("    %d. ID %d [приор %d] %s (%s)\n",
                           pos++, curr->data.id, curr->data.priority,
                           curr->data.username, time2str(curr->data.timestamp));
                    curr = curr->next;
                }
            }
        }
    }

    printf("\nСТЕК ОТМЕНЁННЫХ (всего: %d):\n", canceled->size);
    if (canceled->size == 0) {
        printf("  Стек пуст.\n");
    } else {
        StackNode* curr = canceled->top;
        int pos = 1;
        while (curr) {
            printf("  %d. ID %d [приор %d] %s (был в подр.%d) (%s)\n",
                   pos++, curr->data.id, curr->data.priority,
                   curr->data.username, curr->department,
                   time2str(curr->data.timestamp));
            curr = curr->next;
        }
    }
    printf("========================================\n");
}

// ============================ Основная программа с меню ============================

int main(void) {
    // Создаём три подразделения
    Subdivision subs[3];
    for (int i = 0; i < 3; i++) {
        init_subdivision(&subs[i]);
    }

    // Стек отменённых
    Stack canceled;
    init_stack(&canceled);

    int choice;
    do {
        printf("\n========== МЕНЮ ==========\n");
        printf("1. Добавить заявку\n");
        printf("2. Просмотреть первую заявку в подразделении\n");
        printf("3. Обработать заявку (с наивысшим приоритетом)\n");
        printf("4. Переместить заявку между подразделениями\n");
        printf("5. Отменить заявку (поместить в стек)\n");
        printf("6. Восстановить последнюю отменённую\n");
        printf("7. Найти заявку по ID\n");
        printf("8. Найти заявку по имени пользователя\n");
        printf("9. Вывести состояние всех структур\n");
        printf("0. Выход\n");
        printf("Ваш выбор: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_input();
            choice = -1;
        } else {
            clear_input();
        }

        switch (choice) {
            case 1: {
                int dept = input_int("Введите номер подразделения", 1, 3);
                add_request_to_subdivision(&subs[dept-1]);
                break;
            }
            case 2: {
                int dept = input_int("Введите номер подразделения", 1, 3);
                peek_subdivision(&subs[dept-1]);
                break;
            }
            case 3: {
                int dept = input_int("Введите номер подразделения", 1, 3);
                process_subdivision(&subs[dept-1]);
                break;
            }
            case 4: {
                int id = input_int("Введите ID заявки", 1, 1000000);
                int from = input_int("Из какого подразделения", 1, 3);
                int to = input_int("В какое подразделение", 1, 3);
                if (from == to) {
                    printf("Ошибка: подразделения совпадают.\n");
                } else {
                    move_request_between_subdivisions(&subs[from-1], &subs[to-1], id);
                }
                break;
            }
            case 5: {
                int id = input_int("Введите ID заявки для отмены", 1, 1000000);
                cancel_request_subdivision(subs, &canceled, id);
                break;
            }
            case 6: {
                restore_last_canceled_subdivision(subs, &canceled);
                break;
            }
            case 7: {
                int id = input_int("Введите ID для поиска", 1, 1000000);
                find_by_id_subdivision(subs, &canceled, id);
                break;
            }
            case 8: {
                char name[256];
                printf("Введите имя пользователя: ");
                fgets(name, 256, stdin);
                name[strcspn(name, "\n")] = '\0';
                find_by_username_subdivision(subs, &canceled, name);
                break;
            }
            case 9: {
                print_all_status(subs, &canceled);
                break;
            }
            case 0: {
                printf("Выход из программы.\n");
                break;
            }
            default: {
                printf("Неверный выбор. Попробуйте снова.\n");
                break;
            }
        }
    } while (choice != 0);

    // Очистка памяти
    for (int i = 0; i < 3; i++) {
        clear_subdivision(&subs[i]);
    }
    clear_stack(&canceled);

    return 0;
}
