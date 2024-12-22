
#include "http-server.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_CHATS 100000
#define MAX_REACTIONS 100
#define USERNAME_SIZE 16
#define MESSAGE_SIZE 256
#define REACTION_SIZE 16
#define TIMESTAMP_SIZE 20

//max reaction
#define MAX_BUFFER 99999
struct Reaction {
    char user[USERNAME_SIZE];  
    char message[REACTION_SIZE];  
};
typedef struct Reaction Reaction;

struct Chat {
    uint32_t id;  
    char user[USERNAME_SIZE];  
    char message[MESSAGE_SIZE];  
    char timestamp[TIMESTAMP_SIZE];  
    uint32_t num_reactions;  
    Reaction reactions[MAX_REACTIONS];  
};

typedef struct Chat Chat;
Chat chats[MAX_CHATS];
uint32_t chat_count = 0;

char const HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_200_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";

void handle_404(int client_sock, char *path) {
    printf("SERVER LOG: Got request for unrecognized path \"%s\"\n", path);

    char response_buff[BUFFER_SIZE];
    snprintf(response_buff, sizeof(response_buff), 
             "Error 404:\r\nUnrecognized path \"%s\"\r\n", path);

    write(client_sock, HTTP_404_NOT_FOUND, strlen(HTTP_404_NOT_FOUND));
    write(client_sock, response_buff, strlen(response_buff));
}



char* url_decode(char* str) {
    int len = strlen(str);
    char* buf = malloc(len + 1);
    int j = 0;

    for (int i = 0; i < len; i++) {
        if (str[i] == '%') {
            int hex = 0;
            if (isdigit(str[i + 1])) {
                hex = (str[i + 1] - '0') * 16;
            } else {
                hex = (tolower(str[i + 1]) - 'a' + 10) * 16;
            }
            if (isdigit(str[i + 2])) {
                hex += str[i + 2] - '0';
            } else {
                hex += tolower(str[i + 2]) - 'a' + 10;
            }
            buf[j++] = (char)hex;
            i += 2;
        } else if (str[i] == '+') {
            buf[j++] = ' ';
        } else {
            buf[j++] = str[i];
        }
    }

    buf[j] = '\0';
    return buf;
}

uint8_t add_chat(char* username, char* message) {
    if (chat_count >= MAX_CHATS) {
        return 0;
    }
    
    Chat* new_chat = &chats[chat_count];
    new_chat->id = chat_count + 1;
    strncpy(new_chat->user, username, USERNAME_SIZE - 1);
    new_chat->user[USERNAME_SIZE - 1] = '\0';
    strncpy(new_chat->message, message, MESSAGE_SIZE - 1);
    new_chat->message[MESSAGE_SIZE - 1] = '\0';
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(new_chat->timestamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
    
    new_chat->num_reactions = 0;
    chat_count++;
    return 1;
}
uint8_t add_reaction(char* username, char* message, char* id) {
    uint32_t chat_id = atoi(id);
    if (chat_id == 0 || chat_id > chat_count) {
        return 0;
    }
    
    Chat* chat = &chats[chat_id - 1];
    if (chat->num_reactions >= MAX_REACTIONS) {
        return 0;
    }
    
    Reaction* new_reaction = &chat->reactions[chat->num_reactions];
    strncpy(new_reaction->user, username, USERNAME_SIZE - 1);
    new_reaction->user[USERNAME_SIZE - 1] = '\0';
    strncpy(new_reaction->message, message, REACTION_SIZE - 1);
    new_reaction->message[REACTION_SIZE - 1] = '\0';
    
    chat->num_reactions++;
    return 1;
}
void reset() {
    memset(chats, 0, sizeof(chats));
    chat_count = 0;
}
void respond_with_chats(int client) {
    char response[MAX_BUFFER];
    int offset = 0;
    
    offset += snprintf(response + offset, BUFFER_SIZE - offset, "%s", HTTP_200_OK);
    
    for (uint32_t i = 0; i < chat_count; i++) {
        Chat* chat = &chats[i];
        offset += snprintf(response + offset,sizeof(response), 
                           "[#%d %s] %9s: %s\n", 
                           chat->id, chat->timestamp, chat->user, chat->message);

        if (chat->num_reactions > 0) {
            for (uint32_t j = 0; j < chat->num_reactions; j++) {
                Reaction* reaction = &chat->reactions[j];
                offset += snprintf(response + offset,sizeof(response), 
                                   "                              (%s) %s\n",
                                   reaction->user, reaction->message);
            }
        }
    }
    
    write(client, response, strlen(response));
}

//new functions
uint8_t edit_chat(uint32_t chat_id, char* new_message){
    if(chat_id==0 || chat_id>chat_count){
        //check if chat_id is valid
        return 0;
    }
    //the chat to be editted    
    Chat* chat=&chats[chat_id-1];
    //new message copied
    strncpy(chat->message, new_message, MESSAGE_SIZE-1);
    chat->message[MESSAGE_SIZE-1]='\0';

    return 1;
}
void handle_edit(char* path, int client_sock){
    char* id=strstr(path,"id=");
    char* message=strstr(path, "message=");

    if(!id||!message){
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n",28);
        return;
    }
    id+=3;
    message+=8;
    char* id_end=strchr(id, '&');
    char* message_end=strchr(message,' ');

    if(id_end) *id_end='\0';
    if(message_end) *message_end='\0';  

    char* decoded_id=url_decode(id);
    char* decoded_message=url_decode(message);

    if(strlen(decoded_message)>255){
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n",28);
        free(decoded_id);
        free(decoded_message);

        return;
    }
    uint32_t chat_id=atoi(decoded_id);

    if(edit_chat(chat_id,decoded_message)){
        respond_with_chats(client_sock);
    }else{
        write(client_sock, "HTTP/1.1 500 Internal Server Error\r\n\r\n",37);

    }

    free(decoded_id);
    free(decoded_message);
}


void handle_post(char* path, int client_sock) {
    char* user = strstr(path, "user=");
    char* message = strstr(path, "message=");
    
    if (!user || !message) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        return;
    }
    
    user += 5;
    message += 8;
    
    char* user_end = strchr(user, '&');
    char* message_end = strchr(message, ' ');
    
    if (user_end) *user_end = '\0';
    if (message_end) *message_end = '\0';
    
    char* decoded_user = url_decode(user);
    char* decoded_message = url_decode(message);
    
    if (strlen(decoded_user) > 15 || strlen(decoded_message) > 255) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        free(decoded_user);
        free(decoded_message);
        return;
    }
    
    if (chat_count >= MAX_CHATS) {
        write(client_sock, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 37);
        free(decoded_user);
        free(decoded_message);
        return;
    }
    
    if (add_chat(decoded_user, decoded_message)) {
        respond_with_chats(client_sock);
    } else {
        write(client_sock, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 37);
    }
    
    free(decoded_user);
    free(decoded_message);
}

void handle_reaction(char* path, int client_sock) {
    char* user = strstr(path, "user=");
    char* message = strstr(path, "message=");
    char* id = strstr(path, "id=");
    
    if (!user || !message || !id) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        return;
    }
    
    user += 5;
    message += 8;
    id += 3;
    
    char* user_end = strchr(user, '&');
    char* message_end = strchr(message, '&');
    char* id_end = strchr(id, ' ');
    
    if (user_end) *user_end = '\0';
    if (message_end) *message_end = '\0';
    if (id_end) *id_end = '\0';
    
    char* decoded_user = url_decode(user);
    char* decoded_message = url_decode(message);
    
    if (strlen(decoded_user) > 15 || strlen(decoded_message) > REACTION_SIZE - 1) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        free(decoded_user);
        free(decoded_message);
        return;
    }
    
    if (add_reaction(decoded_user, decoded_message, id)) {
        respond_with_chats(client_sock);
    } else {
        write(client_sock, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 37);
    }
    
    free(decoded_user);
    free(decoded_message);
}

void handle_response(char *request, int client_sock) {
  char method[8];
    char *path = strchr(request, ' '); 

    if (path == NULL) {
        handle_404(client_sock, "Invalid request");
        return;
    }
    *path = '\0';
    strcpy(method, request);

    path++; 
    char *end = strchr(path, ' ');
    if (end == NULL) {
        handle_404(client_sock, "Invalid request");
        return;
    }
    *end = '\0';  

 
    printf("\nSERVER LOG: Got %s request for path: \"%s\"\n", method, path);

 if (strncmp(path, "/post", 5) == 0) {
        handle_post(path, client_sock);
    } else if (strncmp(path, "/react", 6) == 0) {
        handle_reaction(path, client_sock);
    } else if (strcmp(path, "/chats") == 0) {
        respond_with_chats(client_sock);
    } else if (strcmp(path, "/reset") == 0) {
        reset();
        write(client_sock, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n", 44);
        write(client_sock, "Server data has been reset.\r\n", 29);
    } else if(strncmp(path, "/edit",5)==0){
            handle_edit(path,client_sock);
        
        }else {
        handle_404(client_sock, path);
    }
}

int main(int argc, char *argv[]) {
    int port = 0;
    if(argc >= 2) {     
        port = atoi(argv[1]);
    }

    start_server(&handle_response, port);
}
