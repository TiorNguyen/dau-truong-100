#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

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
void start_game(Room *room);
int authenticate_user(char *username, char *password, int *status);
int register_user(char *username, char *password);

int create_room(int client_socket); 

// Hàm tạo phòng mới
int create_room(int client_socket) {
    if (room_count >= MAX_ROOMS) return -1;  // Giới hạn số phòng

    Room *new_room = &rooms[room_count];
    new_room->id = room_count;
    new_room->player_count = 1;  // Người tạo phòng là người đầu tiên tham gia phòng
    new_room->player_sockets[0] = client_socket;
    new_room->is_open = 1;
    room_count++;
    return new_room->id;
}

// Hàm kiểm tra nếu người chơi đã ở trong phòng nào

int is_player_in_any_room(int client_socket) {
    for (int i = 0; i < room_count; i++) {
        Room *room = &rooms[i];
        for (int j = 0; j < room->player_count; j++) {
            if (room->player_sockets[j] == client_socket) {
                return 1; // Người chơi đã ở trong một phòng
            }
        }
    }
    return 0; // Người chơi không ở trong phòng nào
}

// Hàm thêm người chơi vào phòng hiện có
int join_room(int room_id, int client_socket) {
    if (room_id < 0 || room_id >= room_count || !rooms[room_id].is_open) {
        return -1; // Phòng không tồn tại hoặc đã đóng
    }

    Room *room = &rooms[room_id];
    if (room->player_count >= MAX_PLAYERS_PER_ROOM) {
        room->is_open = 0; // Đóng phòng nếu đầy
        return -1;
    }

    room->player_sockets[room->player_count++] = client_socket;

    // Kiểm tra nếu phòng đã đủ 3 người chơi, bắt đầu trò chơi
    if (room->player_count == MAX_PLAYERS_PER_ROOM) {
        room->is_open = 0;  // Đóng phòng
        start_game(room);  // Bắt đầu trò chơi khi đủ người
    }
    return 0;
}

// Hàm xóa phòng khỏi danh sách
void delete_room(int room_id) {
    for (int i = room_id; i < room_count - 1; i++) {
        rooms[i] = rooms[i + 1];
    }
    room_count--;
    printf("Room %d deleted.\n", room_id);
}

// Hàm loại người chơi khỏi phòng
int leave_room(int client_socket) {
    for (int i = 0; i < room_count; i++) {
        Room *room = &rooms[i];
        for (int j = 0; j < room->player_count; j++) {
            if (room->player_sockets[j] == client_socket) {
                // Di chuyển người chơi cuối cùng lên vị trí của người chơi rời phòng
                room->player_sockets[j] = room->player_sockets[room->player_count - 1];
                room->player_count--;

                // Mở lại phòng nếu có chỗ trống
                if (room->player_count < MAX_PLAYERS_PER_ROOM) {
                    room->is_open = 1;
                }

                // Nếu phòng trống, xóa phòng
                if (room->player_count == 0) {
                    delete_room(i);
                }
                return 0; // Người chơi đã rời phòng
            }
        }
    }
    return -1; // Người chơi không ở trong phòng nào
}




// Hàm hiển thị danh sách phòng
void list_rooms(int client_socket) {
    char buffer[256] = "Room List:\n";

    for (int i = 0; i < room_count; i++) {
        char room_info[50];
        sprintf(room_info, "Room ID: %d, Players: %d/%d\n",
                rooms[i].id, rooms[i].player_count, MAX_PLAYERS_PER_ROOM);
        strcat(buffer, room_info);
    }
    send(client_socket, buffer, strlen(buffer), 0);
}


void *handle_client(void *client_socket) {
    int sock = *(int*)client_socket;
    char buffer[256];
    int bytes;

    while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes] = '\0';
        printf("Received from client: %s\n", buffer);

        // Xử lý đăng nhập
        if (strncmp(buffer, "LOGIN", 5) == 0) {
            char username[50], password[50];
            int status;
            sscanf(buffer + 6, "%s %s", username, password);
            if (authenticate_user(username, password, &status)) {
                if (status == 1) {
                    send(sock, "LOGIN_SUCCESS", 13, 0);
                    printf("Sent to client: LOGIN_SUCCESS\n");
                } else {
                    send(sock, "ACCOUNT_INACTIVE", 16, 0);
                    printf("Sent to client: ACCOUNT_INACTIVE\n");
                }
            } else {
                send(sock, "LOGIN_FAIL", 10, 0);
                printf("Sent to client: LOGIN_FAIL\n");
            }
        }

        // Xử lý đăng ký
        else if (strncmp(buffer, "REGISTER", 8) == 0) {
            char username[50], password[50];
            sscanf(buffer + 9, "%s %s", username, password);
            if (register_user(username, password) == 1) {
                send(sock, "REGISTER_SUCCESS", 16, 0);
                printf("Sent to client: REGISTER_SUCCESS\n");
            } else {
                send(sock, "USER_EXISTS", 11, 0);
                printf("Sent to client: USER_EXISTS\n");
            }
        }

        // Xử lý tạo phòng
        // else if (strncmp(buffer, "CREATE_ROOM", 11) == 0) {
        //     int room_id = create_room(sock);
        //     if (room_id != -1) {
        //         sprintf(buffer, "ROOM_CREATED %d", room_id);
        //         send(sock, buffer, strlen(buffer), 0);
        //     } else {
        //         send(sock, "ROOM_CREATION_FAILED", 20, 0);
        //     }
        // }

        else if (strncmp(buffer, "CREATE_ROOM", 11) == 0) {
            if (is_player_in_any_room(sock)) {
                send(sock, "ALREADY_IN_ROOM", 15, 0); // Từ chối tạo phòng nếu người chơi đã ở trong phòng
                printf("Client %d attempted to create a room but is already in one.\n", sock);
            } else {
                int room_id = create_room(sock);
                if (room_id != -1) {
                    sprintf(buffer, "ROOM_CREATED %d", room_id);
                    send(sock, buffer, strlen(buffer), 0);
                } else {
                send(sock, "ROOM_CREATION_FAILED", 20, 0);
                }
            }
        }

        // Xử lý hiển thị danh sách phòng
        else if (strncmp(buffer, "LIST_ROOMS", 10) == 0) {
            list_rooms(sock);
        }

        // Xử lý vào phòng
        else if (strncmp(buffer, "JOIN_ROOM", 9) == 0) {
            int room_id;
            sscanf(buffer + 10, "%d", &room_id);
            if (join_room(room_id, sock) == 0) {
                sprintf(buffer, "JOINED_ROOM %d", room_id);
                send(sock, buffer, strlen(buffer), 0);
            } else {
                send(sock, "JOIN_ROOM_FAILED", 16, 0);
            }
        }

        // Xử lý thoát phòng
        else if (strncmp(buffer, "LEAVE_ROOM", 10) == 0) {
            if (leave_room(sock) == 0) {
                send(sock, "LEFT_ROOM", 9, 0);
            } else {
                send(sock, "NOT_IN_ROOM", 11, 0);
            }
        }
        else if (strncmp(buffer, "START_GAME", 10) == 0) {
            FILE *file = fopen("questions.txt", "r");
            if (!file) {
                send(sock, "QUESTION_FILE_ERROR", 19, 0);
                printf("Error: Could not open questions file.\n");
                continue;
            }

            char line[256], question[200], option_a[50], option_b[50], option_c[50], option_d[50], correct_answer;
            while (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%c",
                        question, option_a, option_b, option_c, option_d, &correct_answer) != 6) {
                    printf("Skipping malformed question.\n");
                    continue;
                }

                // Gửi câu hỏi đến client
                snprintf(buffer, sizeof(buffer),
                        "%s\nA. %s\nB. %s\nC. %s\nD. %s\nYour answer (A, B, C, D): ",
                        question, option_a, option_b, option_c, option_d);
                if (send(sock, buffer, strlen(buffer), 0) <= 0) {
                    printf("Failed to send question to client (Socket %d).\n", sock);
                    break;
                }

                // Chờ nhận câu trả lời từ client
                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(sock, buffer, sizeof(buffer), 0);
                if (bytes <= 0) {
                    printf("Client (Socket %d) disconnected.\n", sock);
                    break;
                }

                buffer[bytes] = '\0';
                printf("Received answer from client (Socket %d): %s\n", sock, buffer);

                // Kiểm tra đáp án
                if (toupper(buffer[0]) == correct_answer) {
                    send(sock, "Correct answer!\n", 16, 0);
                } else {
                    send(sock, "Wrong answer. Try again.\n", 24, 0);
                }
            }

            fclose(file);
        }

    }



    close(sock);
    return NULL;
}

void *handle_client_game(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    FILE *file = fopen("questions.txt", "r");
    if (!file) {
        printf("Could not open question file.\n");
        pthread_exit(NULL);
    }

    char line[256], question[200], option_a[50], option_b[50], option_c[50], option_d[50], correct_answer;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%c",
                   question, option_a, option_b, option_c, option_d, &correct_answer) != 6) {
            printf("Skipping malformed question.\n");
            continue;
        }

        // Gửi câu hỏi tới client
        char question_buffer[BUFFER_SIZE];
        snprintf(question_buffer, sizeof(question_buffer),
                 "%s\nA. %s\nB. %s\nC. %s\nD. %s\nYour answer (A, B, C, D): ",
                 question, option_a, option_b, option_c, option_d);

        if (send(client_socket, question_buffer, strlen(question_buffer), 0) <= 0) {
            printf("Failed to send question to client (Socket %d).\n", client_socket);
            break;
        }

        // Nhận câu trả lời từ client
        char response[10] = {0};
        int bytes_received = recv(client_socket, response, sizeof(response) - 1, 0);

        if (bytes_received <= 0) {
            printf("Client (Socket %d) disconnected unexpectedly.\n", client_socket);
            break;
        }

        response[bytes_received] = '\0';
        printf("Received answer from client (Socket %d): %s\n", client_socket, response);

        // Kiểm tra đáp án
        if (toupper(response[0]) == correct_answer) {
            send(client_socket, "Correct answer!\n", 16, 0);
        } else {
            send(client_socket, "Wrong answer. Try again.\n", 24, 0);
        }
    }

    fclose(file);
    close(client_socket);
    pthread_exit(NULL);
}




// Hàm xác thực người dùng khi đăng nhập
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
            return 1;
        }
    }
    fclose(file);
    return 0;
}

// Hàm đăng ký người dùng mới
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

    // Thêm tài khoản mới vào cuối file với trạng thái mặc định là 1 (active)
    fprintf(file, "%s %s %d\n", username, password, 1);
    fclose(file);
    return 1;  // Đăng ký thành công
}



void start_game(Room *room) {
    for (int i = 0; i < room->player_count; i++) {
        if (room->player_sockets[i] == -1) continue;

        pthread_t thread_id;
        int *client_socket = malloc(sizeof(int));
        *client_socket = room->player_sockets[i];

        if (pthread_create(&thread_id, NULL, handle_client_game, (void *)client_socket) != 0) {
            perror("Failed to create thread for client");
            free(client_socket);
        }

        pthread_detach(thread_id); // Tự động giải phóng tài nguyên luồng sau khi kết thúc
    }
}









int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Tạo socket server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Gắn socket với địa chỉ
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Lắng nghe kết nối
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Chấp nhận và xử lý kết nối từ client
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue; // Không dừng server nếu xảy ra lỗi accept
        }

        // Hiển thị thông tin client kết nối
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d (Socket %d)\n", client_ip, ntohs(client_addr.sin_port), client_socket);

        // Tạo luồng riêng xử lý client
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }
        *new_sock = client_socket;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Failed to create thread");
            free(new_sock);
            close(client_socket);
            continue;
        }

        pthread_detach(thread); // Tự động giải phóng tài nguyên sau khi luồng kết thúc
    }

    close(server_socket);
    return 0;
}
