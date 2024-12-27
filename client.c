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

// -----------------------------------------
void handle_login() {
    char username[50], password[50], buffer[256];

    printf("Enter username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading username.\n");
        exit(1);
    }
    username[strcspn(username, "\n")] = '\0';

    printf("Enter password: ");
    if (fgets(password, sizeof(password), stdin) == NULL) {
        printf("Error reading password.\n");
        exit(1);
    }
    password[strcspn(password, "\n")] = '\0';

    sprintf(buffer, "LOGIN %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);
    printf("DEBUG: Sent login data to server: %s\n", buffer);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response from server.\n");
        exit(0);
    }
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

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
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading username.\n");
        exit(1);
    }
    username[strcspn(username, "\n")] = '\0';

    printf("Enter password for registration: ");
    if (fgets(password, sizeof(password), stdin) == NULL) {
        printf("Error reading password.\n");
        exit(1);
    }
    password[strcspn(password, "\n")] = '\0';

    sprintf(buffer, "REGISTER %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);
    printf("DEBUG: Sent registration data to server: %s\n", buffer);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response from server.\n");
        exit(0);
    }
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

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

// -----------------------------------------
// Chờ server gửi câu hỏi -> in ra -> nhập đáp án -> gửi lại
// Hoặc nhận thông báo kết thúc -> thoát
void handle_gameplay() {
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("Disconnected from server or connection closed unexpectedly.\n");
            break;
        }
        buffer[bytes_received] = '\0';

        // In toàn bộ nội dung server gửi
        printf("%s", buffer);

        // Nếu chứa chuỗi kết thúc
        if (strstr(buffer, "You are eliminated.") != NULL ||
            strstr(buffer, "Congratulations!") != NULL  ||
            strstr(buffer, "No more questions") != NULL) {
            // Trò chơi kết thúc
            printf("DEBUG: Game ended message detected.\n");
            break;
        }

        // Nếu thông điệp chứa "Your answer", thì chờ người dùng nhập đáp án
        if (strstr(buffer, "Your answer") != NULL) {
            printf("DEBUG: Server is asking for your answer.\n");
            char answer[10];
            if (fgets(answer, sizeof(answer), stdin) == NULL) {
                printf("Error reading answer.\n");
                break;
            }
            answer[strcspn(answer, "\n")] = '\0';

            printf("DEBUG: Client is sending answer: '%s'\n", answer);
            int sent_bytes = send(client_socket, answer, strlen(answer), 0);
            if (sent_bytes <= 0) {
                printf("Failed to send answer to server.\n");
                break;
            }
        }
    }
}

// -----------------------------------------
void handle_create_room() {
    char buffer[256];
    send(client_socket, "CREATE_ROOM", 11, 0);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response.\n");
        return;
    }
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

    if (strncmp(buffer, "ROOM_CREATED", 12) == 0) {
        printf("Room created successfully. Waiting for enough players...\n");
        // Khi đủ 3 người, server gọi start_game => ta cần handle_gameplay()
        handle_gameplay();
    } else if (strncmp(buffer, "ALREADY_IN_ROOM", 15) == 0) {
        printf("You are already in a room.\n");
    } else {
        printf("Failed to create room.\n");
    }
}

void handle_list_rooms() {
    char buffer[512];
    send(client_socket, "LIST_ROOMS", 10, 0);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response.\n");
        return;
    }
    buffer[bytes_received] = '\0';
    printf("%s", buffer);
}

void handle_join_room() {
    char input[50], buffer[256];
    printf("Enter Room ID to join: ");

    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("Error reading input.\n");
        return;
    }
    
    input[strcspn(input, "\n")] = '\0';
    int room_id = atoi(input);

    sprintf(buffer, "JOIN_ROOM %d", room_id);
    send(client_socket, buffer, strlen(buffer), 0);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response.\n");
        return;
    }
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);

    if (strncmp(buffer, "JOINED_ROOM", 11) == 0) {
        printf("Joined room successfully. Waiting for game to start...\n");
        // Khi đủ 3 người chơi, server sẽ start_game, ta ở đây handle_gameplay
        handle_gameplay();
    } else if (strncmp(buffer, "JOIN_ROOM_FAILED", 16) == 0) {
        printf("Failed to join room. The room may be full or closed.\n");
    }
}

void handle_leave_room() {
    char buffer[256];
    send(client_socket, "LEAVE_ROOM", 10, 0);

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        printf("Failed to receive response.\n");
        return;
    }
    buffer[bytes_received] = '\0';
    printf("Server response: %s\n", buffer);
}

// -----------------------------------------
int main() {
    if (connect_to_server() != 0) {
        return -1;
    }

    char input[10];
    printf("Select an option:\n1. Login\n2. Register\nEnter choice: ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("Error reading choice.\n");
        close(client_socket);
        return 0;
    }
    input[strcspn(input, "\n")] = '\0';
    int choice = atoi(input);

    if (choice == 1) {
        handle_login();
    } else if (choice == 2) {
        handle_register();
    } else {
        printf("Invalid choice.\n");
        close(client_socket);
        return 0;
    }

    while (1) {
        printf("\nOptions:\n");
        printf("1. Create Room\n");
        printf("2. List Rooms\n");
        printf("3. Join Room\n");
        printf("4. Leave Room\n");
        printf("5. Exit\n");
        printf("Enter choice: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Error reading choice.\n");
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        choice = atoi(input);

        if (choice == 1) {
            handle_create_room();
        } else if (choice == 2) {
            handle_list_rooms();
        } else if (choice == 3) {
            handle_join_room();
        } else if (choice == 4) {
            handle_leave_room();
        } else if (choice == 5) {
            printf("Exiting...\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    close(client_socket);
    return 0;
}
