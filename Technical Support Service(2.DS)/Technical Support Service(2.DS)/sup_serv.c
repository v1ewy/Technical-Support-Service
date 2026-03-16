#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================ Константы ============================
#define MAX_QUEUE_SIZE 5      // максимальный размер одной очереди
#define MIN_TOTAL_SIZE 3       // минимальный суммарный размер для объединения
#define MAX_DEPENDENCIES 100   // максимальное число зависимостей у заявки
#define SAVE_FILE "support_data.txt"

// ============================ Макрос для потоко-безопасного strtok ============================
#ifdef _WIN32
#define STRTOK strtok_s
#else
#define STRTOK strtok_r
#endif

// ============================ Структуры данных ============================

// Заявка
typedef struct Request {
    int id;
    char username[256];
    int priority;
    time_t timestamp;
    int* dependencies;      // массив ID зависимостей
    int dep_count;          // количество зависимостей
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
    int department;          // откуда отменена (1..3)
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
int identifier = 0;                     // генерация уникальных ID
Subdivision subs[3];                    // три подразделения
Stack canceled;                         // стек отменённых

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
    struct tm* tm_info = localtime(&time);
    if (tm_info) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    }
    else {
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
        }
        else if (value < min || value > max) {
            printf("Ошибка! Число должно быть от %d до %d.\n", min, max);
        }
        else {
            return value;
        }
    } while (1);
}

// Проверка существования ID в очередях (не в стеке!)
int is_id_in_queues(int id) {
    for (int d = 0; d < 3; d++) {
        for (int i = 0; i < subs[d].count; i++) {
            QueueNode* curr = subs[d].queues[i]->front;
            while (curr) {
                if (curr->data.id == id) return 1;
                curr = curr->next;
            }
        }
    }
    return 0;
}

// Проверка существования ID в стеке
int is_id_in_stack(int id) {
    StackNode* st = canceled.top;
    while (st) {
        if (st->data.id == id) return 1;
        st = st->next;
    }
    return 0;
}

// Проверка существования ID где-либо (очереди + стек)
int is_id_exists(int id) {
    return is_id_in_queues(id) || is_id_in_stack(id);
}

// Поиск местоположения заявки по ID (только в очередях)
int find_request_location(int id, int* out_dept, int* out_queue, QueueNode** out_node, QueueNode** out_prev) {
    for (int d = 0; d < 3; d++) {
        for (int i = 0; i < subs[d].count; i++) {
            QueueNode* prev = NULL;
            QueueNode* curr = subs[d].queues[i]->front;
            while (curr) {
                if (curr->data.id == id) {
                    *out_dept = d;
                    *out_queue = i;
                    *out_node = curr;
                    *out_prev = prev;
                    return 1;
                }
                prev = curr;
                curr = curr->next;
            }
        }
    }
    return 0;
}

// Создание новой заявки (с вводом зависимостей)
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

    // Ввод зависимостей
    int dep_count = input_int("Введите количество зависимостей", 0, MAX_DEPENDENCIES);
    request->dep_count = dep_count;
    if (dep_count > 0) {
        request->dependencies = (int*)malloc(dep_count * sizeof(int));
        for (int i = 0; i < dep_count; i++) {
            int dep_id;
            do {
                printf("Введите ID зависимости #%d: ", i + 1);
                scanf("%d", &dep_id);
                clear_input();
                if (!is_id_exists(dep_id)) {
                    printf("Ошибка: заявка с ID %d не существует. Повторите ввод.\n", dep_id);
                }
                else {
                    request->dependencies[i] = dep_id;
                    break;
                }
            } while (1);
        }
    }
    else {
        request->dependencies = NULL;
    }
}

// ============================ Базовые операции с очередью ============================

// Удаление узла по id из конкретной очереди (без проверки зависимостей)
QueueNode* remove_node_by_id(Queue* queue, int id) {
    if (queue->front == NULL) return NULL;

    QueueNode* prev = NULL;
    QueueNode* curr = queue->front;

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
    }
    else {
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

    QueueNode* current = queue->front;
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

// ============================ Операции с зависимостями ============================

// Удалить зависимость с заданным ID из всех заявок (очереди + стек)
void remove_dependency_from_all(int removed_id) {
    // Проходим по всем очередям
    for (int d = 0; d < 3; d++) {
        for (int i = 0; i < subs[d].count; i++) {
            QueueNode* curr = subs[d].queues[i]->front;
            while (curr) {
                if (curr->data.dep_count > 0) {
                    // Проверяем, есть ли removed_id в dependencies
                    int found = 0;
                    for (int j = 0; j < curr->data.dep_count; j++) {
                        if (curr->data.dependencies[j] == removed_id) {
                            found = 1;
                            break;
                        }
                    }
                    if (found) {
                        // Создаём новый массив без removed_id
                        int new_count = curr->data.dep_count - 1;
                        int* new_deps = NULL;
                        if (new_count > 0) {
                            new_deps = (int*)malloc(new_count * sizeof(int));
                            int idx = 0;
                            for (int j = 0; j < curr->data.dep_count; j++) {
                                if (curr->data.dependencies[j] != removed_id) {
                                    new_deps[idx++] = curr->data.dependencies[j];
                                }
                            }
                        }
                        free(curr->data.dependencies);
                        curr->data.dependencies = new_deps;
                        curr->data.dep_count = new_count;
                    }
                }
                curr = curr->next;
            }
        }
    }

    // Проходим по стеку
    StackNode* st = canceled.top;
    while (st) {
        if (st->data.dep_count > 0) {
            int found = 0;
            for (int j = 0; j < st->data.dep_count; j++) {
                if (st->data.dependencies[j] == removed_id) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                int new_count = st->data.dep_count - 1;
                int* new_deps = NULL;
                if (new_count > 0) {
                    new_deps = (int*)malloc(new_count * sizeof(int));
                    int idx = 0;
                    for (int j = 0; j < st->data.dep_count; j++) {
                        if (st->data.dependencies[j] != removed_id) {
                            new_deps[idx++] = st->data.dependencies[j];
                        }
                    }
                }
                free(st->data.dependencies);
                st->data.dependencies = new_deps;
                st->data.dep_count = new_count;
            }
        }
        st = st->next;
    }
}

// ============================ Операции очистки памяти ============================

// Очистка памяти одной очереди (с освобождением зависимостей)
void clear_queue(Queue* queue) {
    QueueNode* curr = queue->front;
    while (curr) {
        QueueNode* next = curr->next;
        if (curr->data.dependencies) free(curr->data.dependencies);
        free(curr);
        curr = next;
    }
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

// Очистка стека (с освобождением зависимостей)
void clear_stack(Stack* stack) {
    StackNode* curr = stack->top;
    while (curr) {
        StackNode* next = curr->next;
        if (curr->data.dependencies) free(curr->data.dependencies);
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
            }
            else {
                new_q1->rear->next = curr;
                new_q1->rear = curr;
            }
            new_q1->size++;
        }
        else {
            if (new_q2->rear == NULL) {
                new_q2->front = new_q2->rear = curr;
            }
            else {
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
void add_request_to_subdivision(int dept_idx) {
    Request req;
    create_request(&req);

    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        printf("Ошибка выделения памяти.\n");
        if (req.dependencies) free(req.dependencies);
        return;
    }
    new_node->data = req;
    new_node->next = NULL;

    insert_node_into_subdivision(&subs[dept_idx], new_node);
    printf("Заявка №%d добавлена в подразделение %d.\n", req.id, dept_idx + 1);
}

// Просмотр первой заявки в подразделении
void peek_subdivision(int dept_idx) {
    Subdivision* sub = &subs[dept_idx];
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
        printf("Все очереди подразделения %d пусты.\n", dept_idx + 1);
        return;
    }

    QueueNode* first = sub->queues[best_idx]->front;
    printf("Первая заявка (подразделение %d, очередь %d): ID %d, приоритет %d, пользователь %s, время %s\n",
        dept_idx + 1, best_idx + 1, first->data.id, first->data.priority,
        first->data.username, time2str(first->data.timestamp));
}

// Проверка, удовлетворены ли все зависимости (учитываем только заявки в очередях)
int all_dependencies_satisfied(Request* req) {
    for (int i = 0; i < req->dep_count; i++) {
        if (is_id_in_queues(req->dependencies[i])) {
            return 0; // есть зависимость, которая всё ещё в очереди
        }
    }
    return 1;
}

// Обработка конкретной заявки по ID (без рекурсивной проверки зависимостей)
// Возвращает 1, если успешно, 0 если не удалось (зависимости или ошибка)
int process_request_by_id(int id) {
    int dept, queue;
    QueueNode* node;
    QueueNode* prev;
    if (!find_request_location(id, &dept, &queue, &node, &prev)) {
        printf("Заявка с ID %d не найдена в очередях.\n", id);
        return 0;
    }

    // Проверяем зависимости (только в очередях)
    if (!all_dependencies_satisfied(&node->data)) {
        printf("Невозможно обработать заявку №%d: есть невыполненные зависимости в очередях.\n", id);
        return 0;
    }

    // Удаляем ID этой заявки из зависимостей всех других заявок
    remove_dependency_from_all(id);

    // Удаляем заявку
    Queue* q = subs[dept].queues[queue];
    if (prev == NULL) {
        q->front = node->next;
        if (q->rear == node) q->rear = NULL;
    }
    else {
        prev->next = node->next;
        if (q->rear == node) q->rear = prev;
    }
    q->size--;

    printf("Заявка №%d обработана.\n", node->data.id);
    if (node->data.dependencies) free(node->data.dependencies);
    free(node);

    // Балансировка
    int total = get_total_size(&subs[dept]);
    if (total < MIN_TOTAL_SIZE && subs[dept].count > 1) {
        merge_queues(&subs[dept]);
    }

    return 1;
}

// Рекурсивная обработка заявки и всех её зависимостей (только из очередей)
int process_request_recursive(int id) {
    if (!is_id_in_queues(id)) {
        // Уже обработана (или в стеке, но стек не считаем)
        return 1;
    }

    // Находим заявку (чтобы получить список зависимостей)
    int dept, queue;
    QueueNode* node;
    QueueNode* prev;
    if (!find_request_location(id, &dept, &queue, &node, &prev)) {
        return 0; // не должно случиться
    }

    // Сначала обрабатываем все зависимости, которые ещё в очередях
    for (int i = 0; i < node->data.dep_count; i++) {
        int dep_id = node->data.dependencies[i];
        if (is_id_in_queues(dep_id)) {
            if (!process_request_recursive(dep_id)) {
                printf("Не удалось обработать зависимость %d для заявки %d.\n", dep_id, id);
                return 0;
            }
        }
    }

    // Теперь все зависимости удовлетворены, обрабатываем саму заявку
    return process_request_by_id(id);
}

// Обработка заявки с наивысшим приоритетом в подразделении
void process_subdivision(int dept_idx) {
    Subdivision* sub = &subs[dept_idx];
    int best_idx = -1;
    Request* best_req = NULL;
    QueueNode* best_node = NULL;

    for (int i = 0; i < sub->count; i++) {
        if (sub->queues[i]->front == NULL) continue;
        if (best_idx == -1 || sub->queues[i]->front->data.priority < best_req->priority) {
            best_idx = i;
            best_req = &sub->queues[i]->front->data;
            best_node = sub->queues[i]->front;
        }
    }

    if (best_idx == -1) {
        printf("Все очереди подразделения %d пусты.\n", dept_idx + 1);
        return;
    }

    int id = best_node->data.id;

    // Собираем список неудовлетворённых зависимостей (только те, что в очередях)
    int pending_deps[MAX_DEPENDENCIES];
    int pending_count = 0;
    for (int i = 0; i < best_node->data.dep_count; i++) {
        int dep_id = best_node->data.dependencies[i];
        if (is_id_in_queues(dep_id)) {
            pending_deps[pending_count++] = dep_id;
        }
    }

    if (pending_count == 0) {
        // Нет зависимостей в очередях, можно обработать сразу
        process_request_by_id(id);
        return;
    }

    // Есть неудовлетворённые зависимости
    printf("Заявка №%d имеет невыполненные зависимости (в очередях):\n", id);
    for (int i = 0; i < pending_count; i++) {
        printf("  %d. ID %d\n", i + 1, pending_deps[i]);
    }
    printf("Выберите действие:\n");
    printf("  1. Обработать одну из зависимостей (введите её ID)\n");
    printf("  2. Обработать все зависимые заявки (рекурсивно)\n");
    printf("  3. Вернуться в меню\n");
    printf("Ваш выбор: ");
    int choice;
    scanf("%d", &choice);
    clear_input();

    if (choice == 1) {
        printf("Введите ID зависимости для обработки: ");
        int dep_id;
        scanf("%d", &dep_id);
        clear_input();
        if (is_id_in_queues(dep_id)) {
            if (process_request_recursive(dep_id)) {
                printf("Зависимость %d обработана. Повторная попытка обработать заявку %d...\n", dep_id, id);
                process_subdivision(dept_idx); // рекурсивно вызываем для того же подразделения
            }
            else {
                printf("Не удалось обработать зависимость %d.\n", dep_id);
            }
        }
        else {
            printf("Заявка с ID %d не найдена в очередях.\n", dep_id);
        }
    }
    else if (choice == 2) {
        // Пытаемся обработать исходную заявку рекурсивно (она обработает все зависимости)
        if (process_request_recursive(id)) {
            printf("Заявка %d и все её зависимости успешно обработаны.\n", id);
        }
        else {
            printf("Не удалось обработать все зависимости.\n");
        }
    }
    else {
        printf("Возврат в меню.\n");
    }
}

// Перемещение заявки между подразделениями
void move_request_between_subdivisions(int from, int to, int id) {
    if (from == to) {
        printf("Ошибка: подразделения совпадают.\n");
        return;
    }
    Subdivision* sub_from = &subs[from];
    Subdivision* sub_to = &subs[to];

    for (int i = 0; i < sub_from->count; i++) {
        QueueNode* node = remove_node_by_id(sub_from->queues[i], id);
        if (node) {
            insert_node_into_subdivision(sub_to, node);
            printf("Заявка №%d перемещена из подразделения %d в %d.\n", id, from + 1, to + 1);
            return;
        }
    }
    printf("Заявка №%d не найдена в исходном подразделении.\n", id);
}

// Отмена заявки (перемещение в стек)
void cancel_request_subdivision(int id) {
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* node = remove_node_by_id(sub->queues[i], id);
            if (node != NULL) {
                // Создаём узел стека с копией зависимостей
                StackNode* st_node = (StackNode*)malloc(sizeof(StackNode));
                if (st_node == NULL) {
                    printf("Ошибка памяти при отмене.\n");
                    free(node->data.dependencies);
                    free(node);
                    return;
                }
                st_node->data.id = node->data.id;
                strcpy(st_node->data.username, node->data.username);
                st_node->data.priority = node->data.priority;
                st_node->data.timestamp = node->data.timestamp;
                st_node->data.dep_count = node->data.dep_count;
                if (node->data.dep_count > 0) {
                    st_node->data.dependencies = (int*)malloc(node->data.dep_count * sizeof(int));
                    memcpy(st_node->data.dependencies, node->data.dependencies, node->data.dep_count * sizeof(int));
                }
                else {
                    st_node->data.dependencies = NULL;
                }
                st_node->department = d + 1;
                st_node->next = canceled.top;
                canceled.top = st_node;
                canceled.size++;

                free(node->data.dependencies);
                free(node);

                printf("Заявка №%d отменена и помещена в стек.\n", id);
                return;
            }
        }
    }
    printf("Заявка №%d не найдена.\n", id);
}

// Восстановление последней отменённой заявки
void restore_last_canceled_subdivision(void) {
    if (canceled.top == NULL) {
        printf("Стек отменённых пуст.\n");
        return;
    }

    StackNode* st_node = canceled.top;
    canceled.top = st_node->next;
    canceled.size--;

    QueueNode* q_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (q_node == NULL) {
        printf("Ошибка памяти при восстановлении.\n");
        if (st_node->data.dependencies) free(st_node->data.dependencies);
        free(st_node);
        return;
    }

    q_node->data.id = st_node->data.id;
    strcpy(q_node->data.username, st_node->data.username);
    q_node->data.priority = st_node->data.priority;
    q_node->data.timestamp = st_node->data.timestamp;
    q_node->data.dep_count = st_node->data.dep_count;
    if (st_node->data.dep_count > 0) {
        q_node->data.dependencies = (int*)malloc(st_node->data.dep_count * sizeof(int));
        memcpy(q_node->data.dependencies, st_node->data.dependencies, st_node->data.dep_count * sizeof(int));
    }
    else {
        q_node->data.dependencies = NULL;
    }
    q_node->next = NULL;

    int dept_idx = st_node->department - 1;
    insert_node_into_subdivision(&subs[dept_idx], q_node);

    printf("Заявка №%d восстановлена в подразделение %d.\n", st_node->data.id, st_node->department);

    if (st_node->data.dependencies) free(st_node->data.dependencies);
    free(st_node);
}

// Поиск заявки по ID
void find_by_id_subdivision(int id) {
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* curr = sub->queues[i]->front;
            int pos = 1;
            while (curr) {
                if (curr->data.id == id) {
                    printf("Заявка №%d найдена в подразделении %d, очередь %d, позиция %d (в очереди).\n",
                        id, d + 1, i + 1, pos);
                    return;
                }
                curr = curr->next;
                pos++;
            }
        }
    }

    StackNode* st_curr = canceled.top;
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
void find_by_username_subdivision(const char* name) {
    int found = 0;
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* curr = sub->queues[i]->front;
            int pos = 1;
            while (curr) {
                if (strcmp(curr->data.username, name) == 0) {
                    printf("Пользователь '%s' (ID %d) найден в подразделении %d, очередь %d, позиция %d (в очереди).\n",
                        name, curr->data.id, d + 1, i + 1, pos);
                    found = 1;
                }
                curr = curr->next;
                pos++;
            }
        }
    }

    StackNode* st_curr = canceled.top;
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
void print_all_status(void) {
    printf("\n========== ТЕКУЩЕЕ СОСТОЯНИЕ ==========\n");

    for (int d = 0; d < 3; d++) {
        printf("\nПОДРАЗДЕЛЕНИЕ %d (всего очередей: %d):\n", d + 1, subs[d].count);
        if (get_total_size(&subs[d]) == 0) {
            printf("  Нет заявок.\n");
        }
        else {
            for (int i = 0; i < subs[d].count; i++) {
                Queue* q = subs[d].queues[i];
                printf("  Очередь %d (заявок: %d):\n", i + 1, q->size);
                QueueNode* curr = q->front;
                int pos = 1;
                while (curr) {
                    printf("    %d. ID %d [приор %d] %s (%s) Зависимости: ",
                        pos++, curr->data.id, curr->data.priority,
                        curr->data.username, time2str(curr->data.timestamp));
                    if (curr->data.dep_count == 0) printf("нет");
                    else {
                        for (int j = 0; j < curr->data.dep_count; j++) {
                            printf("%d ", curr->data.dependencies[j]);
                        }
                    }
                    printf("\n");
                    curr = curr->next;
                }
            }
        }
    }

    printf("\nСТЕК ОТМЕНЁННЫХ (всего: %d):\n", canceled.size);
    if (canceled.size == 0) {
        printf("  Стек пуст.\n");
    }
    else {
        StackNode* curr = canceled.top;
        int pos = 1;
        while (curr) {
            printf("  %d. ID %d [приор %d] %s (был в подр.%d) (%s) Зависимости: ",
                pos++, curr->data.id, curr->data.priority,
                curr->data.username, curr->department,
                time2str(curr->data.timestamp));
            if (curr->data.dep_count == 0) printf("нет");
            else {
                for (int j = 0; j < curr->data.dep_count; j++) {
                    printf("%d ", curr->data.dependencies[j]);
                }
            }
            printf("\n");
            curr = curr->next;
        }
    }
    printf("========================================\n");
}

// ============================ Сериализация ============================

// Сохранение данных в файл
void save_to_file(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Ошибка: не удалось открыть файл для записи.\n");
        return;
    }

    fprintf(f, "ID:%d\n", identifier);

    // Сохраняем очереди
    for (int d = 0; d < 3; d++) {
        Subdivision* sub = &subs[d];
        for (int i = 0; i < sub->count; i++) {
            QueueNode* curr = sub->queues[i]->front;
            while (curr) {
                fprintf(f, "Q;%d;%d;%d;%s;%d;%lld;",
                    d + 1, i, curr->data.id, curr->data.username,
                    curr->data.priority, (long long)curr->data.timestamp);
                // зависимости
                if (curr->data.dep_count > 0) {
                    for (int j = 0; j < curr->data.dep_count; j++) {
                        if (j > 0) fprintf(f, ",");
                        fprintf(f, "%d", curr->data.dependencies[j]);
                    }
                }
                fprintf(f, "\n");
                curr = curr->next;
            }
        }
    }

    // Сохраняем стек
    StackNode* st = canceled.top;
    while (st) {
        fprintf(f, "S;%d;0;%d;%s;%d;%lld;",
            st->department, st->data.id, st->data.username,
            st->data.priority, (long long)st->data.timestamp);
        if (st->data.dep_count > 0) {
            for (int j = 0; j < st->data.dep_count; j++) {
                if (j > 0) fprintf(f, ",");
                fprintf(f, "%d", st->data.dependencies[j]);
            }
        }
        fprintf(f, "\n");
        st = st->next;
    }

    fclose(f);
    printf("Данные сохранены в файл %s\n", filename);
}

// Загрузка данных из файла
void load_from_file(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Файл %s не найден. Загрузка пропущена.\n", filename);
        return;
    }

    // Очищаем текущие данные
    for (int i = 0; i < 3; i++) {
        clear_subdivision(&subs[i]);
        init_subdivision(&subs[i]);
    }
    clear_stack(&canceled);

    char line[1024];
    int line_num = 0;

    // Читаем первую строку с идентификатором
    if (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "ID:%d", &identifier) == 1) {
            // успешно
        }
        else {
            rewind(f);
            identifier = 0;
        }
    }

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char* token;
        char* context = NULL;
        char line_copy[1024];
        strcpy(line_copy, line);

        token = STRTOK(line_copy, ";\n", &context);
        if (!token) continue;

        char type = token[0];
        if (type != 'Q' && type != 'S') {
            printf("Строка %d: неверный тип '%c', пропуск.\n", line_num, type);
            continue;
        }

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        int dept = atoi(token) - 1;

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        int queue_idx = atoi(token); // для очередей, для стека не используется

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        int id = atoi(token);

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        char username[256];
        strcpy(username, token);

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        int priority = atoi(token);

        token = STRTOK(NULL, ";\n", &context);
        if (!token) { printf("Строка %d: не хватает данных.\n", line_num); continue; }
        time_t timestamp = (time_t)atoll(token);

        token = STRTOK(NULL, ";\n", &context);
        char* deps_str = token;

        // Проверка на дубликат ID
        if (is_id_exists(id)) {
            printf("Строка %d: заявка с ID %d уже существует, пропуск.\n", line_num, id);
            continue;
        }

        int dep_count = 0;
        int deps[MAX_DEPENDENCIES];
        if (deps_str && strlen(deps_str) > 0) {
            char deps_copy[256];
            strcpy(deps_copy, deps_str);
            char* dep_context = NULL;
            char* dep_token = STRTOK(deps_copy, ",", &dep_context);
            while (dep_token && dep_count < MAX_DEPENDENCIES) {
                deps[dep_count++] = atoi(dep_token);
                dep_token = STRTOK(NULL, ",", &dep_context);
            }
        }

        if (type == 'Q') {
            QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
            if (!node) {
                printf("Ошибка памяти при загрузке.\n");
                continue;
            }
            node->data.id = id;
            strcpy(node->data.username, username);
            node->data.priority = priority;
            node->data.timestamp = timestamp;
            node->data.dep_count = dep_count;
            if (dep_count > 0) {
                node->data.dependencies = (int*)malloc(dep_count * sizeof(int));
                memcpy(node->data.dependencies, deps, dep_count * sizeof(int));
            }
            else {
                node->data.dependencies = NULL;
            }
            node->next = NULL;

            insert_node_into_subdivision(&subs[dept], node);
        }
        else { // type == 'S'
            StackNode* node = (StackNode*)malloc(sizeof(StackNode));
            if (!node) {
                printf("Ошибка памяти при загрузке.\n");
                continue;
            }
            node->data.id = id;
            strcpy(node->data.username, username);
            node->data.priority = priority;
            node->data.timestamp = timestamp;
            node->data.dep_count = dep_count;
            if (dep_count > 0) {
                node->data.dependencies = (int*)malloc(dep_count * sizeof(int));
                memcpy(node->data.dependencies, deps, dep_count * sizeof(int));
            }
            else {
                node->data.dependencies = NULL;
            }
            node->department = dept + 1;
            node->next = canceled.top;
            canceled.top = node;
            canceled.size++;
        }

        if (id > identifier) identifier = id;
    }

    fclose(f);
    printf("Данные загружены из файла %s\n", filename);
}

// ============================ Основная программа с меню ============================

int main(void) {
#ifdef _WIN32
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#endif

    for (int i = 0; i < 3; i++) {
        init_subdivision(&subs[i]);
    }
    init_stack(&canceled);

    load_from_file(SAVE_FILE);

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
        printf("10. Экспортировать данные в файл\n");
        printf("11. Импортировать данные из файла\n");
        printf("0. Выход\n");
        printf("Ваш выбор: ");

        if (scanf("%d", &choice) != 1) {
            clear_input();
            choice = -1;
        }
        else {
            clear_input();
        }

        switch (choice) {
        case 1: {
            int dept = input_int("Введите номер подразделения", 1, 3);
            add_request_to_subdivision(dept - 1);
            break;
        }
        case 2: {
            int dept = input_int("Введите номер подразделения", 1, 3);
            peek_subdivision(dept - 1);
            break;
        }
        case 3: {
            int dept = input_int("Введите номер подразделения", 1, 3);
            process_subdivision(dept - 1);
            break;
        }
        case 4: {
            int id = input_int("Введите ID заявки", 1, 1000000);
            int from = input_int("Из какого подразделения", 1, 3) - 1;
            int to = input_int("В какое подразделение", 1, 3) - 1;
            move_request_between_subdivisions(from, to, id);
            break;
        }
        case 5: {
            int id = input_int("Введите ID заявки для отмены", 1, 1000000);
            cancel_request_subdivision(id);
            break;
        }
        case 6: {
            restore_last_canceled_subdivision();
            break;
        }
        case 7: {
            int id = input_int("Введите ID для поиска", 1, 1000000);
            find_by_id_subdivision(id);
            break;
        }
        case 8: {
            char name[256];
            printf("Введите имя пользователя: ");
            fgets(name, 256, stdin);
            name[strcspn(name, "\n")] = '\0';
            find_by_username_subdivision(name);
            break;
        }
        case 9: {
            print_all_status();
            break;
        }
        case 10: {
            save_to_file(SAVE_FILE);
            break;
        }
        case 11: {
            load_from_file(SAVE_FILE);
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

    save_to_file(SAVE_FILE);

    for (int i = 0; i < 3; i++) {
        clear_subdivision(&subs[i]);
    }
    clear_stack(&canceled);

    return 0;
}
