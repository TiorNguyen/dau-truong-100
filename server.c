#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_ROOMS 10
#define MAX_PLAYERS_PER_ROOM 3
#define BUFFER_SIZE 512

typedef struct {
    int id;
    int player_sockets[MAX_PLAYERS_PER_ROOM];
    int player_count;
    int is_open; // 1 nếu phòng mở và có thể tham gia, 0 nếu đã đầy hoặc đóng
} Room;

Room rooms[MAX_ROOMS];
int room_count = 0;
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;

int authenticate_user(char *username, char *password, int *status);
int register_user(char *username, char *password);
int create_room(int client_socket); 
/**
 * join_room() trả về:
 *   -1: Không join được (phòng không tồn tại, phòng đã đủ người, hoặc đóng...)
 *    0: Join thành công, phòng chưa đủ 3
 *    2: Join thành công và phòng vừa đủ 3 người
 */
int join_room(int room_id, int client_socket);
int leave_room(int client_socket);
void start_game(Room *room);
void list_rooms(int client_socket);

// -----------------------------------------
int create_room(int client_socket) {
    pthread_mutex_lock(&room_mutex);

    if (room_count >= MAX_ROOMS) {
        pthread_mutex_unlock(&room_mutex);
        return -1;  // Giới hạn số phòng
    }

    Room *new_room = &rooms[room_count];
    new_room->id = room_count;
    new_room->player_count = 1; // Người tạo phòng là người đầu tiên tham gia
    new_room->player_sockets[0] = client_socket;
    new_room->is_open = 1;

    room_count++;
    pthread_mutex_unlock(&room_mutex);

    return new_room->id;
}

int is_player_in_any_room(int client_socket) {
    pthread_mutex_lock(&room_mutex);
    for (int i = 0; i < room_count; i++) {
        Room *room = &rooms[i];
        for (int j = 0; j < room->player_count; j++) {
            if (room->player_sockets[j] == client_socket) {
                pthread_mutex_unlock(&room_mutex);
                return 1; // Người chơi đã ở trong một phòng
            }
        }
    }
    pthread_mutex_unlock(&room_mutex);
    return 0; // Người chơi không ở trong phòng nào
}

int join_room(int room_id, int client_socket) {
    pthread_mutex_lock(&room_mutex);

    if (room_id < 0 || room_id >= room_count || !rooms[room_id].is_open) {
        pthread_mutex_unlock(&room_mutex);
        return -1; // Phòng không tồn tại hoặc đã đóng
    }

    Room *room = &rooms[room_id];
    if (room->player_count >= MAX_PLAYERS_PER_ROOM) {
        // Phòng đã đủ người
        room->is_open = 0;
        pthread_mutex_unlock(&room_mutex);
        return -1;
    }

    // Thêm client_socket vào danh sách phòng
    room->player_sockets[room->player_count++] = client_socket;

    // Nếu vừa đủ 3, đánh dấu phòng đã full, trả về mã 2
    if (room->player_count == MAX_PLAYERS_PER_ROOM) {
        room->is_open = 0;
        pthread_mutex_unlock(&room_mutex);
        return 2; // phòng vừa đủ 3
    }

    pthread_mutex_unlock(&room_mutex);
    return 0; // join phòng thành công, nhưng chưa đủ 3
}

// Xoá phòng, dời những phòng sau nó lên
void delete_room(int room_id) {
    for (int i = room_id; i < room_count - 1; i++) {
        rooms[i] = rooms[i + 1];
    }
    room_count--;
    printf("DEBUG: Room %d deleted.\n", room_id);
}

int leave_room(int client_socket) {
    pthread_mutex_lock(&room_mutex);
    for (int i = 0; i < room_count; i++) {
        Room *room = &rooms[i];
        for (int j = 0; j < room->player_count; j++) {
            if (room->player_sockets[j] == client_socket) {
                // Đưa socket cuối cùng lên vị trí j, giảm count
                room->player_sockets[j] = room->player_sockets[room->player_count - 1];
                room->player_count--;

                // Nếu dưới 3 người thì mở lại phòng
                if (room->player_count < MAX_PLAYERS_PER_ROOM) {
                    room->is_open = 1;
                }

                // Nếu phòng không còn người thì xoá
                if (room->player_count == 0) {
                    delete_room(i);
                }

                pthread_mutex_unlock(&room_mutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

void list_rooms(int client_socket) {
    pthread_mutex_lock(&room_mutex);

    char buffer[256] = "Room List:\n";
    for (int i = 0; i < room_count; i++) {
        char room_info[64];
        sprintf(room_info, "Room ID: %d, Players: %d/%d\n",
                rooms[i].id, rooms[i].player_count, MAX_PLAYERS_PER_ROOM);
        strcat(buffer, room_info);
    }

    pthread_mutex_unlock(&room_mutex);
    send(client_socket, buffer, strlen(buffer), 0);
}

int authenticate_user(char *username, char *password, int *status) {
    FILE *file = fopen("account.txt", "r");
    if (!file) return 0;

    char line[100], file_username[50], file_password[50];
    int file_status;

    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s %d", file_username, file_password, &file_status);
        if (strcmp(file_username, username) == 0 && strcmp(file_password, password) == 0) {
            *status = file_status;
            fclose(file);
            return 1; // đăng nhập thành công
        }
    }
    fclose(file);
    return 0;
}

int register_user(char *username, char *password) {
    FILE *file = fopen("account.txt", "a+");
    if (!file) return 0;

    char line[100], file_username[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s", file_username);
        if (strcmp(file_username, username) == 0) {
            fclose(file);
            return -1;  // Người dùng đã tồn tại
        }
    }

    fprintf(file, "%s %s %d\n", username, password, 1);
    fclose(file);
    return 1;
}

// -----------------------------------------
// Cấu trúc cho luồng gửi/nhận câu hỏi
typedef struct {
    int socket;
    char question[200];
    char option_a[50];
    char option_b[50];
    char option_c[50];
    char option_d[50];
    char correct_answer;
    int *player_status;  
    int player_index;
    pthread_mutex_t *status_mutex;
} PlayerThreadArgs;

void *handle_player_question(void *args) {
    PlayerThreadArgs *player_args = (PlayerThreadArgs *)args;

    char question_buffer[BUFFER_SIZE];
    snprintf(question_buffer, sizeof(question_buffer), 
             "%s\nA. %s\nB. %s\nC. %s\nD. %s\nYour answer (A, B, C, D): ",
             player_args->question,
             player_args->option_a,
             player_args->option_b,
             player_args->option_c,
             player_args->option_d);

    printf("DEBUG: Sending question to player %d (socket %d)\n",
           player_args->player_index, player_args->socket);

    int sent_bytes = send(player_args->socket, question_buffer, strlen(question_buffer), 0);
    if (sent_bytes <= 0) {
        // Không gửi được câu hỏi => coi như player bị loại
        printf("DEBUG: Failed to send question to player %d (socket %d)\n",
               player_args->player_index, player_args->socket);
        pthread_mutex_lock(player_args->status_mutex);
        player_args->player_status[player_args->player_index] = 0;
        pthread_mutex_unlock(player_args->status_mutex);
        return NULL;
    }

    printf("DEBUG: Question sent. Waiting for player %d's answer...\n", player_args->player_index);

    char response[10];
    int bytes_received = recv(player_args->socket, response, sizeof(response) - 1, 0);

    printf("DEBUG: After recv from player %d, bytes_received = %d\n",
           player_args->player_index, bytes_received);

    if (bytes_received <= 0) {
        // Mất kết nối hoặc client không trả lời
        printf("DEBUG: Player %d (socket %d) did not respond or disconnected.\n",
               player_args->player_index, player_args->socket);
        pthread_mutex_lock(player_args->status_mutex);
        player_args->player_status[player_args->player_index] = 0;
        pthread_mutex_unlock(player_args->status_mutex);
        return NULL;
    }

    response[bytes_received] = '\0';
    printf("DEBUG: Player %d (socket %d) answered: '%s'\n",
           player_args->player_index, player_args->socket, response);

    // Kiểm tra đáp án
    if (toupper(response[0]) != player_args->correct_answer) {
        send(player_args->socket, "You are eliminated.\n", 20, 0);
        pthread_mutex_lock(player_args->status_mutex);
        player_args->player_status[player_args->player_index] = 0;
        pthread_mutex_unlock(player_args->status_mutex);
    } else {
        // Đúng, vẫn còn trụ
        pthread_mutex_lock(player_args->status_mutex);
        player_args->player_status[player_args->player_index] = 1;
        pthread_mutex_unlock(player_args->status_mutex);
    }

    return NULL;
}

// -----------------------------------------
void start_game(Room *room) {
    FILE *file = fopen("question.txt", "r");
    if (!file) {
        printf("Could not open question file.\n");
        return;
    }

    printf("DEBUG: start_game() called for room %d with %d players.\n",
           room->id, room->player_count);

    int player_status[MAX_PLAYERS_PER_ROOM];
    for (int i = 0; i < room->player_count; i++) {
        player_status[i] = 1; // Tất cả bắt đầu với trạng thái đang chơi
    }

    pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

    char line[256];
    char question[200], option_a[50], option_b[50], option_c[50], option_d[50];
    char correct_answer;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%[^|]| %[^|]| %[^|]| %[^|]| %[^|]| %c",
                   question, option_a, option_b, option_c, option_d, &correct_answer) != 6) {
            printf("DEBUG: Error reading question format. Skipping line.\n");
            continue;
        }

        // Đếm người chơi còn trụ
        int active_count = 0;
        for (int i = 0; i < room->player_count; i++) {
            if (player_status[i] == 1 && room->player_sockets[i] != -1) {
                active_count++;
            }
        }

        // Nếu <= 1 người, kết thúc
        if (active_count <= 1) {
            if (active_count == 1) {
                for (int i = 0; i < room->player_count; i++) {
                    if (player_status[i] == 1 && room->player_sockets[i] != -1) {
                        send(room->player_sockets[i], 
                             "Congratulations! You are the winner.\n", 37, 0);
                        printf("DEBUG: Player (socket %d) is the winner!\n", 
                               room->player_sockets[i]);
                        break;
                    }
                }
            }
            fclose(file);
            return;
        }

        // Gửi câu hỏi cho tất cả các player còn trụ
        pthread_t threads[MAX_PLAYERS_PER_ROOM];
        PlayerThreadArgs args[MAX_PLAYERS_PER_ROOM];
        int thread_count = 0;

        for (int i = 0; i < room->player_count; i++) {
            if (player_status[i] == 1 && room->player_sockets[i] != -1) {
                strcpy(args[i].question, question);
                strcpy(args[i].option_a, option_a);
                strcpy(args[i].option_b, option_b);
                strcpy(args[i].option_c, option_c);
                strcpy(args[i].option_d, option_d);
                args[i].correct_answer = toupper(correct_answer);
                args[i].socket = room->player_sockets[i];
                args[i].player_status = player_status;
                args[i].player_index = i;
                args[i].status_mutex = &status_mutex;

                pthread_create(&threads[thread_count], NULL, handle_player_question, &args[i]);
                thread_count++;
            }
        }

        // Đợi các luồng trả lời xong
        for (int i = 0; i < thread_count; i++) {
            pthread_join(threads[i], NULL);
        }

        // Kiểm tra số người chơi còn trụ
        active_count = 0;
        int last_player_socket = -1;
        for (int i = 0; i < room->player_count; i++) {
            if (player_status[i] == 1 && room->player_sockets[i] != -1) {
                active_count++;
                last_player_socket = room->player_sockets[i];
            }
        }

        if (active_count == 1) {
            // Có 1 người thắng
            send(last_player_socket, "Congratulations! You are the winner.\n", 37, 0);
            printf("DEBUG: Player (socket %d) is the winner!\n", last_player_socket);
            fclose(file);
            return;
        }
        if (active_count == 0) {
            // Không còn ai
            printf("DEBUG: No winners. All players eliminated.\n");
            fclose(file);
            return;
        }
    }

    // Đã hết câu hỏi
    fclose(file);

    int active_count = 0;
    int last_player_socket = -1;
    for (int i = 0; i < room->player_count; i++) {
        if (player_status[i] == 1 && room->player_sockets[i] != -1) {
            active_count++;
            last_player_socket = room->player_sockets[i];
        }
    }

    if (active_count == 1) {
        send(last_player_socket, "Congratulations! You are the winner.\n", 37, 0);
    } else if (active_count > 1) {
        // Hoà
        for (int i = 0; i < room->player_count; i++) {
            if (player_status[i] == 1 && room->player_sockets[i] != -1) {
                send(room->player_sockets[i], "No more questions. It's a draw!\n", 32, 0);
            }
        }
    }
}

// -----------------------------------------
void *handle_client(void *arg) {
    int sock = *(int*)arg;
    char buffer[256];
    int bytes;

    printf("DEBUG: New client thread started for socket %d\n", sock);

    while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes] = '\0';
        printf("DEBUG: Received from client %d: %s\n", sock, buffer);

        if (strncmp(buffer, "LOGIN", 5) == 0) {
            char username[50], password[50];
            int status;
            sscanf(buffer + 6, "%s %s", username, password);
            if (authenticate_user(username, password, &status)) {
                if (status == 1) {
                    send(sock, "LOGIN_SUCCESS", 13, 0);
                    printf("DEBUG: Sent to client %d: LOGIN_SUCCESS\n", sock);
                } else {
                    send(sock, "ACCOUNT_INACTIVE", 16, 0);
                    printf("DEBUG: Sent to client %d: ACCOUNT_INACTIVE\n", sock);
                }
            } else {
                send(sock, "LOGIN_FAIL", 10, 0);
                printf("DEBUG: Sent to client %d: LOGIN_FAIL\n", sock);
            }
        }
        else if (strncmp(buffer, "REGISTER", 8) == 0) {
            char username[50], password[50];
            sscanf(buffer + 9, "%s %s", username, password);
            if (register_user(username, password) == 1) {
                send(sock, "REGISTER_SUCCESS", 16, 0);
                printf("DEBUG: Sent to client %d: REGISTER_SUCCESS\n", sock);
            } else {
                send(sock, "USER_EXISTS", 11, 0);
                printf("DEBUG: Sent to client %d: USER_EXISTS\n", sock);
            }
        }
        else if (strncmp(buffer, "CREATE_ROOM", 11) == 0) {
            if (is_player_in_any_room(sock)) {
                send(sock, "ALREADY_IN_ROOM", 15, 0);
                printf("DEBUG: Client %d attempted to create a room but is already in one.\n", sock);
            } else {
                int room_id = create_room(sock);
                if (room_id != -1) {
                    sprintf(buffer, "ROOM_CREATED %d", room_id);
                    send(sock, buffer, strlen(buffer), 0);
                    printf("DEBUG: Sent to client %d: ROOM_CREATED %d\n", sock, room_id);
                } else {
                    send(sock, "ROOM_CREATION_FAILED", 20, 0);
                    printf("DEBUG: Sent to client %d: ROOM_CREATION_FAILED\n", sock);
                }
            }
        }
        else if (strncmp(buffer, "LIST_ROOMS", 10) == 0) {
            list_rooms(sock);
        }
        else if (strncmp(buffer, "JOIN_ROOM", 9) == 0) {
            int room_id;
            sscanf(buffer + 10, "%d", &room_id);

            int ret = join_room(room_id, sock);
            if (ret >= 0) {
                // Thành công join phòng
                sprintf(buffer, "JOINED_ROOM %d", room_id);
                send(sock, buffer, strlen(buffer), 0);
                printf("DEBUG: Sent to client %d: JOINED_ROOM %d\n", sock, room_id);

                // Nếu ret == 2 => vừa đủ 3 => start_game
                if (ret == 2) {
                    pthread_mutex_lock(&room_mutex);
                    Room *r = &rooms[room_id]; 
                    pthread_mutex_unlock(&room_mutex);

                    printf("DEBUG: Room %d is now full (3 players). Starting game...\n", room_id);
                    // Bắt đầu game (gửi câu hỏi) sau khi đã gửi JOINED_ROOM
                    start_game(r);
                }
            } else {
                send(sock, "JOIN_ROOM_FAILED", 16, 0);
                printf("DEBUG: Client %d failed to join room %d\n", sock, room_id);
            }
        }
        else if (strncmp(buffer, "LEAVE_ROOM", 10) == 0) {
            if (leave_room(sock) == 0) {
                send(sock, "LEFT_ROOM", 9, 0);
                printf("DEBUG: Client %d left the room.\n", sock);
            } else {
                send(sock, "NOT_IN_ROOM", 11, 0);
                printf("DEBUG: Client %d is not in any room.\n", sock);
            }
        }
        else {
            // Lệnh không nhận diện
            printf("DEBUG: Unknown command from client %d: %s\n", sock, buffer);
        }
    }

    // client thoát, ta cho rời phòng nếu vẫn ở trong phòng
    leave_room(sock);
    close(sock);
    printf("DEBUG: Client %d disconnected.\n", sock);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        printf("DEBUG: Accepted new client %d\n", client_socket);

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, &client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}
