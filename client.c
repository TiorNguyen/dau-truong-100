#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 512
int client_socket;
struct sockaddr_in server_addr;

int connect_to_server() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(client_socket);
        return -1;
    }

    printf("Connected to server\n");
    return 0;
}

void handle_login() {
    char username[50], password[50], buffer[256];

    printf("Enter username: ");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", password);

    sprintf(buffer, "LOGIN %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);
    printf("Sent login data to server: %s\n", buffer);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    } else {
        printf("Failed to receive response from server.\n");
        exit(0);
    }

    if (strcmp(buffer, "LOGIN_SUCCESS") == 0) {
        printf("Login successful!\n");
    } else if (strcmp(buffer, "ACCOUNT_INACTIVE") == 0) {
        printf("Account inactive.\n");
        exit(0);
    } else {
        printf("Login failed.\n");
        exit(0);
    }
}

void handle_register() {
    char username[50], password[50], buffer[256];

    printf("Enter username for registration: ");
    scanf("%s", username);
    printf("Enter password for registration: ");
    scanf("%s", password);

    sprintf(buffer, "REGISTER %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);
    printf("Sent registration data to server: %s\n", buffer);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    } else {
        printf("Failed to receive response from server.\n");
        exit(0);
    }

    if (strcmp(buffer, "REGISTER_SUCCESS") == 0) {
        printf("Registration successful! You can now log in.\n");
    } else if (strcmp(buffer, "USER_EXISTS") == 0) {
        printf("Username already exists. Please choose a different username.\n");
        exit(0);
    } else {
        printf("Registration failed.\n");
        exit(0);
    }
}

void handle_create_room() {
    char buffer[256];
    send(client_socket, "CREATE_ROOM", 11, 0);
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

    if (strncmp(buffer, "ROOM_CREATED", 12) == 0) {
        printf("Room created successfully. You have joined the room.\n");
        handle_gameplay();  // Bắt đầu trò chơi khi phòng được tạo
    } else if (strncmp(buffer, "ALREADY_IN_ROOM", 15) == 0) {
        printf("You are already in a room and cannot create a new one.\n");
    } else {
        printf("Failed to create room.\n");
    }
}

void handle_list_rooms() {
    char buffer[512];
    send(client_socket, "LIST_ROOMS", 10, 0);
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    printf("Available Rooms:\n%s", buffer);
}

void handle_join_room() {
    int room_id;
    char buffer[256];
    printf("Enter Room ID to join: ");
    scanf("%d", &room_id);

    sprintf(buffer, "JOIN_ROOM %d", room_id);
    send(client_socket, buffer, strlen(buffer), 0);
    
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

    if (strncmp(buffer, "JOINED_ROOM", 11) == 0) {
        printf("Joined room successfully. Waiting for game to start...\n");
        handle_gameplay();  // Bắt đầu chơi khi vào phòng
    } else if (strncmp(buffer, "JOIN_ROOM_FAILED", 16) == 0) {
        printf("Failed to join room. The room may be full or closed.\n");
    }
}

void handle_leave_room() {
    char buffer[256];
    send(client_socket, "LEAVE_ROOM", 10, 0);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);
}

int safe_recv(int socket, char *buffer, size_t length) {
    int total_received = 0;
    while (total_received < length) {
        int received = recv(socket, buffer + total_received, length - total_received, 0);
        if (received <= 0) {
            printf("Connection closed or error on socket %d.\n", socket);
            return -1; // Thoát nếu kết nối bị đóng hoặc có lỗi
        }
        total_received += received;
    }
    return total_received;
}



void handle_gameplay() {
    char buffer[BUFFER_SIZE];

    while (1) {
        // Nhận câu hỏi hoặc thông báo từ server
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Server closed the connection.\n");
            } else {
                perror("Error receiving data from server");
            }
            break;
        }
        buffer[bytes_received] = '\0';  // Đảm bảo kết thúc chuỗi

        printf("Received from server: %s\n", buffer);  // In ra câu hỏi để kiểm tra

        // Kiểm tra nếu nhận được thông báo bị loại hoặc chiến thắng
        // if (strstr(buffer, "You are eliminated.") != NULL || strstr(buffer, "Congratulations!") != NULL) {
        //     printf("Game over: %s\n", buffer);
        //     break;
        // }

        // Kiểm tra xem thông điệp có phải là câu hỏi không
        if (strstr(buffer, "Your answer") != NULL) {
            // printf("Your answer (A, B, C, or D): ");
            char answer[10];
            if (fgets(answer, sizeof(answer), stdin) != NULL) {
                answer[strcspn(answer, "\n")] = '\0'; // Xóa newline
                if (strlen(answer) == 1 && (toupper(answer[0]) == 'A' || toupper(answer[0]) == 'B' || 
                                            toupper(answer[0]) == 'C' || toupper(answer[0]) == 'D')) {
                    if (send(client_socket, answer, strlen(answer), 0) <= 0) {
                        perror("Failed to send answer to server");
                        break;
                    }
                } else {
                    printf("Invalid input. Please enter A, B, C, or D.\n");
                }
            }
            
        }
    }
}









int main() {
    if (connect_to_server() != 0) {
        return -1;
    }

    int choice;
    printf("Select an option:\n1. Login\n2. Register\nEnter choice: ");
    scanf("%d", &choice);

    if (choice == 1) {
        handle_login();
    } else if (choice == 2) {
        handle_register();
    } else {
        printf("Invalid choice\n");
        close(client_socket);
        return 0;
    }

    // Menu sau khi đăng nhập thành công
    while (1) {
        printf("Options:\n1. Create Room\n2. List Rooms\n3. Join Room\n4. Leave Room\n5. Exit\nEnter choice: ");
        scanf("%d", &choice);
        
        if (choice == 1) {
            handle_create_room();  // Tạo phòng và tự động tham gia trò chơi
            break;  // Thoát khỏi menu khi bắt đầu trò chơi
        } else if (choice == 2) {
            handle_list_rooms();
        } else if (choice == 3) {
            handle_join_room();  // Tham gia phòng và tự động vào trò chơi
            break;  // Thoát khỏi menu khi bắt đầu trò chơi
        } else if (choice == 4) {
            handle_leave_room();
        } else if (choice == 5) {
            printf("Exiting...\n");
            break;
        } else {
            printf("Invalid choice\n");
        }
    }

    close(client_socket);
    return 0;
}