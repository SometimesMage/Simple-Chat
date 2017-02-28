#include <stdio.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include "../list.h"
#include "../buffer.h"
#include "client.h"

#define CHAT_PACKET 0x00
#define LOGIN_PACKET 0x01
#define LOGOUT_PACKET 0x02
#define COMMAND_PACKET 0x03
#define NID_PACKET 0x04

#define SWITCH_COMMAND 0x00
#define LIST_COMMAND 0x01

#define GLOBAL_CHANNEL 'g'
#define IOS_CHANNEL 'i'
#define ANDROID_CHANNEL 'a'
#define PRIVATE_CHANNEL 'p'
#define SERVER_CHANNEL 's'

fd_set wfd;
int sock, running = 1;
char channel = GLOBAL_CHANNEL;
char *name;
List *write_queue;
List *client_cache;
Buffer *read_packet;

void handle_input(char *input);

bool starts_with(const char *pre, const char *str);

Buffer *packet_login_create(char *name);

void packet_write(Buffer *packet);

void do_write();

void do_read();

Buffer *packet_buffer_create(Byte packetId);

void packet_read(Buffer *packet);

void packet_process(Buffer *packet);

void process_login_packet();

int client_id_equals(Client *c, int *id);

void process_logout_packet();

void process_chat_packet() ;

void process_nid_packet() ;

Buffer *packet_chat_create(char *msg) ;

int main(int argc, char **argv) {
    fd_set rfd, copy_rfd, copy_wfd;
    char *host;
    char buffer[512];
    int port;
    struct sockaddr_in addr;

    write_queue = list_create();
    client_cache = list_create();

    if (argc < 4) {
        printf("Please input a host, a port, and a name.\n");
        exit(0);
    }

    host = argv[1];
    port = atoi(argv[2]);
    name = argv[3];

    if (port == 0) {
        printf("Please input a valid port.");
        exit(0);
    }

    if (strlen(name) > 15) {
        printf("Please input a name that is equal to or less than 15 characters.");
        exit(0);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);

    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&rfd);
    FD_ZERO(&wfd);
    FD_SET(0, &rfd);
    FD_SET(sock, &rfd);
    FD_SET(sock, &wfd);

    list_add(write_queue, packet_login_create(name));

    printf("Client started and connected!\n");

    while (running) {
        copy_rfd = rfd;
        copy_wfd = wfd;

        struct timeval timeout = {0, 500000};

        int selected = select(sock + 1, &copy_rfd, &copy_wfd, NULL, &timeout);

        if (selected == 0)
            continue;

        //STDIN Read
        if (FD_ISSET(0, &copy_rfd)) {
            fgets((char *) &buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0;
            handle_input(buffer);
        }

        //Server Read
        if (FD_ISSET(sock, &copy_rfd)) {
            do_read();
        }

        //Server Write
        if (FD_ISSET(sock, &copy_wfd)) {
            do_write();
        }
    }
}

void do_read() {
    if (read_packet == NULL) {
        Byte packetId = 0x00;
        int read_bytes = (int) read(sock, &packetId, sizeof(Byte));

        if (read_bytes < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (read_bytes == 0) {
            printf("Server disconnected! Quiting...\n");
            exit(0);
        }

        read_packet = packet_buffer_create(packetId);

        if (read_packet == NULL)
            return;
    }

    packet_read(read_packet);

    if (read_packet->position == read_packet->limit) {
        buffer_flip(read_packet);
        packet_process(read_packet);
        buffer_free(read_packet);
        read_packet = NULL;
    }
}

void do_write() {
    if (write_queue->size == 0) {
        FD_CLR(sock, &wfd);
        return;
    }

    Buffer *packet = list_get(write_queue, 0);
    packet_write(packet);

    if (packet->position == packet->limit) {
        buffer_free(list_remove(write_queue, 0));
    }
}

void packet_read(Buffer *packet) {
    int read_bytes;

    read_bytes = (int) read(sock, packet->buffer + packet->position, (size_t) (packet->limit - packet->position));

    if (read_bytes < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    if (read_bytes == 0 && packet->limit != packet->position) {
        printf("Server disconnected! Quiting...\n");
        exit(0);
    }

    packet->position += read_bytes;
}

void packet_write(Buffer *packet) {
    int write_bytes = (int) write(sock, packet->buffer + packet->position, (size_t) (packet->limit - packet->position));

    if (write_bytes < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    packet->position += write_bytes;
}

void packet_process(Buffer *packet) {
    Byte packetId = buffer_get_at(packet, 0);

    switch (packetId) {
        case CHAT_PACKET:
            process_chat_packet();
            break;
        case LOGIN_PACKET:
            process_login_packet();
            break;
        case LOGOUT_PACKET:
            process_logout_packet();
            break;
        case COMMAND_PACKET: //Client shouldn't be receiving a command packet.
            break;
        case NID_PACKET:
            process_nid_packet();
            break;
        default:
            break;
    }
}

void process_chat_packet() {
    Client *client = NULL;
    char channel = buffer_get_at(read_packet, 1);
    int from_id = buffer_get_at(read_packet, 2);
    char *msg = (char *) (read_packet->buffer + 4);

    int index = list_contains(client_cache, &from_id, (int (*)(void *, void *)) &client_id_equals);

    if (index > -1)
        client = list_get(client_cache, index);

    switch (channel) {
        case PRIVATE_CHANNEL:
            if (client != NULL)
                printf("%s(%d)->%s : %s\n", client->name, client->id, name, msg);
            else
                printf("%d->%s : %s\n", from_id, name, msg);
            break;
        case GLOBAL_CHANNEL:
            if (client != NULL)
                printf("[GLOBAL] %s(%d) : %s\n", client->name, client->id, msg);
            else
                printf("[GLOBAL] %d : %s\n", from_id, msg);
            break;
        case ANDROID_CHANNEL:
            if (client != NULL)
                printf("[Android] %s(%d) : %s\n", client->name, client->id, msg);
            else
                printf("[Android] %d : %s\n", from_id, msg);
            break;
        case IOS_CHANNEL:
            if (client != NULL)
                printf("[iOS] %s(%d) : %s\n", client->name, client->id, msg);
            else
                printf("[iOS] %d : %s\n", from_id, msg);
            break;
        case SERVER_CHANNEL:
            printf("[SERVER] : %s\n", msg);
            break;
        default:
            break;
    }
}

void process_login_packet() {
    Byte client_id = buffer_get_at(read_packet, 1);
    char *client_name = (char *) (read_packet->buffer + 2);

    Client *client = malloc(sizeof(Client));
    memset(client, 0, sizeof(Client));

    client->id = client_id;

    for (int i = 0; i < 15; i++) {
        client->name[i] = client_name[i];
    }

    int index = list_contains(client_cache, &client->id, (int (*)(void *, void *)) &client_id_equals);

    if (index > -1) {
        Client *client2 = list_remove(client_cache, index);
        free(client2);
    }

    list_add(client_cache, client);
    printf("[NOTICE] %s logged in.\n", client->name);
}

void process_logout_packet() {
    int client_id = buffer_get_at(read_packet, 1);

    int index = list_contains(client_cache, &client_id, (int (*)(void *, void *)) &client_id_equals);

    if (index > -1) {
        Client *client = list_remove(client_cache, index);
        printf("[NOTICE] %s logged out.\n", client->name);
        free(client);
    }
}

void process_nid_packet() {
    Byte client_id = buffer_get_at(read_packet, 1);
    char *client_name = (char *) (read_packet->buffer + 2);

    Client *client = malloc(sizeof(Client));
    memset(client, 0, sizeof(Client));

    client->id = client_id;

    for (int i = 0; i < 15; i++) {
        client->name[i] = client_name[i];
    }

    int index = list_contains(client_cache, &client->id, (int (*)(void *, void *)) &client_id_equals);

    if (index > -1) {
        Client *client2 = list_remove(client_cache, index);
        free(client2);
    }

    list_add(client_cache, client); 
}

int client_id_equals(Client *c, int *id) {
    if (c->id == *id)
        return 1;
    return 0;
}

Buffer *packet_buffer_create(Byte packetId) {
    Buffer *result;

    switch (packetId) {
        case CHAT_PACKET: //Chat Message
            result = buffer_create(44);
            buffer_put(result, packetId);
            return result;
        case LOGIN_PACKET: //Login
            result = buffer_create(17);
            buffer_put(result, packetId);
            return result;
        case LOGOUT_PACKET: //Logout
            result = buffer_create(2);
            buffer_put(result, packetId);
            return result;
        case COMMAND_PACKET: //Command
            result = buffer_create(3);
            buffer_put(result, packetId);
            return result;
        case NID_PACKET: //Name/ID Request/Reply
            result = buffer_create(17);
            buffer_put(result, packetId);
            return result;
        default:
            return NULL;
    }

    return NULL;
}

Buffer *packet_login_create(char *name) {
    Buffer *packet = packet_buffer_create(LOGIN_PACKET);
    buffer_put(packet, 0xFF);

    int name_len = (int) strlen(name);

    for (int i = 0; i < name_len; i++) {
        buffer_put(packet, (Byte) name[i]);
    }

    if (name_len < 15) {
        for (int i = name_len; i < 15; i++) {
            buffer_put(packet, 0x00);
        }
    }

    buffer_flip(packet);

    return packet;
}

Buffer *packet_private_chat_create(char *msg, int id) {
    Buffer *packet = packet_chat_create(msg);
    buffer_set(packet, 1, (Byte) PRIVATE_CHANNEL);
    buffer_set(packet, 3, (Byte) id);
    return packet;
}

Buffer *packet_chat_create(char *msg) {
    Buffer *packet = packet_buffer_create(CHAT_PACKET);
    buffer_put(packet, (Byte) channel);
    buffer_put(packet, 0xFF);
    buffer_put(packet, 0xFF);

    int msg_len = (int) strlen(msg);

    for (int i = 0; i < msg_len; i++) {
        buffer_put(packet, (Byte) msg[i]);
    }

    if (msg_len < 40) {
        for (int i = msg_len; i < 40; i++) {
            buffer_put(packet, 0x00);
        }
    }

    buffer_flip(packet);

    return packet;
}

Buffer *packet_command_create(Byte commandId, char channelId) {
    Buffer *packet = packet_buffer_create(COMMAND_PACKET);
    buffer_put(packet, commandId);
    buffer_put(packet, (Byte) channelId);

    buffer_flip(packet);

    return packet;
}

void client_free(Client *client) {
    free(client);
}

void handle_input(char *input) {
    if (starts_with("/quit", input)) {
        printf("Quiting...\n");
        list_free(write_queue, (void (*)(void *)) &buffer_free);
        list_free(client_cache, (void (*)(void *)) &client_free);
        buffer_free(read_packet);
        exit(0);
    }

    if(starts_with("/msg", input)) {
        strsep(&input, " "); //Ignore first token which should be "/msg"
        char *str_id = strsep(&input, " ");

        if(str_id == NULL) {
            printf("[NOTICE] Usage: /msg [id] [msg]\n");
            return;
        }

        int id = atoi(str_id);

        if(id == 0) {
            printf("[NOTICE] Usage: /msg [id] [msg]\n");
            return;
        }

        if(strlen(input) > 40) {
            printf("[NOTICE] You can only send messages of 40 characters of length.\n");
        } else {
            Buffer *packet = packet_private_chat_create(input, id);
            list_add(write_queue, packet);
            if(!FD_ISSET(sock, &wfd))
                FD_SET(sock, &wfd);
        }

        return;
    }

    if(starts_with("/channel", input)) {
        strsep(&input, " ");
        char *input_channel = strsep(&input, " ");

        if(input_channel == NULL) {
            printf("[NOTICE] Usage: /channel [channel]\n");
            return;
        }

        if(input_channel[0] != GLOBAL_CHANNEL && input_channel[0] != ANDROID_CHANNEL && input_channel[0] != IOS_CHANNEL) {
            printf("[NOTICE] Valid channels names: g, i, and a.\n");
            return;
        }

        Buffer *packet = packet_command_create(SWITCH_COMMAND, input_channel[0]);
        channel = input_channel[0];
        list_add(write_queue, packet);
        if(!FD_ISSET(sock, &wfd))
            FD_SET(sock, &wfd);
        printf("[NOTICE] Channel switched to %c\n", input_channel[0]);
        return;
    }

    if(starts_with("/list", input)) {
        Buffer *packet = packet_command_create(LIST_COMMAND, channel);
        list_add(write_queue, packet);
        if(!FD_ISSET(sock, &wfd))
            FD_SET(sock, &wfd);
        return;
    }

    if(strlen(input) > 40) {
        printf("[NOTICE] You can only send messages of 40 characters of length.\n");
    } else {
        Buffer *packet = packet_chat_create(input);
        list_add(write_queue, packet);
        if(!FD_ISSET(sock, &wfd))
            FD_SET(sock, &wfd);
    }
}

bool starts_with(const char *pre, const char *str) {
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}
