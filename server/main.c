#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "../list.h"
#include "client.h"

#define MAX_SERVER_SIZE 10

#define CHAT_PACKET 0x00
#define LOGIN_PACKET 0x01
#define LOGOUT_PACKET 0x02
#define COMMAND_PACKET 0x03
#define NID_PACKET 0x04

#define SWITCH_COMMAND 0x00
#define LIST_COMMAND 0x01

int max_set_size, server_fd, running = 1;
fd_set rfd, wfd;
List *client_list;

void do_accept(int socket_fd);

void do_read(int socket_fd);

void do_write(int socket_fd);

void packet_write(int socket_fd, Buffer *packet);

void packet_read(int socket_fd, Buffer *packet);

Buffer *packet_buffer_create(Byte packetId);

void packet_process(Client *client);

void packet_process_chat(Client *client);

void packet_process_login(Client *client);

void packet_process_logout(Client *client);

void packet_process_command(Client *client);

void packet_process_nid(Client *client);

Buffer *packet_client_logout_create(int client_id);

Buffer *packet_server_message_create(const char *msg);

Buffer *packet_server_logout_create();

void client_write(Client *client, Buffer *packet);

void client_all_write(Buffer *packet);

void client_all_write_except(Buffer *packet, int client_id_except);

void client_channel_write(Buffer *buffer, char channel);

void client_channel_write_except(Buffer *buffer, char channel, int client_id_except);

void client_disconnect(Client *client);

Client *client_get(int socket_fd);

int client_equals(Client *client, int *id);

Buffer *packet_nid_create(Client *client) ;

void client_list_free(Client *client) ;

int main(int argc, char **argv) {
    fd_set copy_rfd, copy_wfd;
    int selected;
    uint16_t port;
    struct sockaddr_in server_addr;

    if (argc < 2) {
        fprintf(stderr, "Please input a port number.\n");
        exit(0);
    }

    port = (uint16_t) atoi(argv[1]);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Running server on port %d.\n", port);

    FD_ZERO(&rfd);
    FD_ZERO(&wfd);
    FD_SET(0, &rfd);
    FD_SET(server_fd, &rfd);
    max_set_size = server_fd;
    client_list = list_create();

    listen(server_fd, 5);

    printf("Server started!\nWaiting for client...\n");

    while (running) {
        copy_rfd = rfd;
        copy_wfd = wfd;
        struct timeval timeout = {0, 500000}; //500 Milliseconds
        selected = select(max_set_size + 1, &copy_rfd, &copy_wfd, NULL, &timeout);

        if(selected == 0)
            continue;

        if (selected < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        //STDIN Read
        if (FD_ISSET(0, &copy_rfd)) {
            printf("Received input\n");
            char input[256];
            fgets(input, 256, stdin);


            if (strcmp("quit\n", input) == 0) {
                printf("Quiting...\n");
                list_free(client_list, (void (*)(void *)) &client_list_free);
                exit(0);
            }
        }//End STDIN read

        //Server Accept
        if (FD_ISSET(server_fd, &copy_rfd)) {
            do_accept(server_fd);
        }

        //Client Read
        for (int i = 3; i <= max_set_size; i++) {
            if (i == server_fd)
                continue;

            if (FD_ISSET(i, &copy_rfd)) {
                do_read(i);
            }
        }//End for loop

        //Client Write
        for (int i = 3; i <= max_set_size; i++) {
            if (FD_ISSET(i, &copy_wfd)) {
                do_write(i);
            }
        }//End for loop
    }//End while running
}

void do_accept(int socket_fd) {
    int client_fd = accept(socket_fd, NULL, NULL);

    if (client_fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    //If server full
    if (client_fd >= MAX_SERVER_SIZE) {
        Buffer *msg_packet = packet_server_message_create("Server is full.");
        Buffer *logout_packet = packet_server_logout_create();
        packet_write(socket_fd, msg_packet);
        packet_write(socket_fd, logout_packet);
        buffer_free(msg_packet);
        buffer_free(logout_packet);
        close(client_fd);
        printf("Client tried to connect but server is full.\n");
        return;
    }//End if server full

    if (max_set_size < client_fd) {
        max_set_size = client_fd;
    }

    FD_SET(client_fd, &rfd);
    Client *client = client_create(client_fd);
    list_add(client_list, client);

    printf("Client %d established connection. Waiting for login packet...\n", client_fd);
}

void do_read(int socket_fd) {
    int read_bytes;
    Client *client = client_get(socket_fd);

    if (client->readPacket == NULL) {
        Byte packetId = 0x00;
        read_bytes = (int) read(socket_fd, &packetId, sizeof(Byte));

        if (read_bytes < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (read_bytes == 0) {
            client_disconnect(client);
            return;
        }

        //If client doesn't have name and packetId is not login packet
        if (strlen(client->name) == 0 && packetId != LOGIN_PACKET) {
            Buffer *packet = packet_server_message_create("Send Login Packet");
            packet_write(socket_fd, packet);
            buffer_free(packet);
        }

        client->readPacket = packet_buffer_create(packetId);

        if(client->readPacket == NULL)
            return;
    }

    packet_read(socket_fd, client->readPacket);

    if (client->readPacket->position == client->readPacket->limit) {
        //Don't Process packet unless it is a login packet or the client's name isn't empty.
        if(buffer_get_at(client->readPacket, 0) == LOGIN_PACKET || strlen(client->name) != 0) {
            buffer_flip(client->readPacket);
            packet_process(client);
        }

        buffer_free(client->readPacket);
        client->readPacket = NULL;
    }
}

void do_write(int socket_fd) {
    Client *client = client_get(socket_fd);

    if (client->writeQueue->size == 0) {
        FD_CLR(socket_fd, &wfd);
        return;
    }

    Buffer *packet = client_peek_write(client);
    packet_write(socket_fd, packet);

    if (packet->position == packet->limit) {
        buffer_free(client_poll_write(client));
    }
}

void packet_read(int socket_fd, Buffer *packet) {
    int read_bytes;

    read_bytes = (int) read(socket_fd, packet->buffer + packet->position, (size_t) (packet->limit - packet->position));

    if (read_bytes < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    if (read_bytes == 0 && packet->limit != packet->position) {
        client_disconnect(client_get(socket_fd));
    }

    packet->position += read_bytes;
}

void packet_write(int socket_fd, Buffer *packet) {
    int write_bytes;

    write_bytes = (int) write(socket_fd, packet->buffer + packet->position,
                              (size_t) (packet->limit - packet->position));

    if (write_bytes < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    packet->position += write_bytes;
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

void packet_process(Client *client) {
    Byte packetId = buffer_get_at(client->readPacket, 0);

    switch (packetId) {
        case CHAT_PACKET:
            packet_process_chat(client);
            break;
        case LOGIN_PACKET:
            packet_process_login(client);
            break;
        case LOGOUT_PACKET:
            packet_process_logout(client);
            break;
        case COMMAND_PACKET:
            packet_process_command(client);
            break;
        case NID_PACKET:
            packet_process_nid(client);
            break;
        default:
            break;
    }
}

void packet_process_chat(Client *client) {
    Buffer *packet = client->readPacket;

    char channel = buffer_get_at(packet, 1);
    buffer_set(packet, 2, (Byte) client->id);//Set FromID
    char *msg = (char *) (packet->buffer + 4);

    if (channel == PRIVATE_CHANNEL) {
        Byte toId = buffer_get_at(packet, 3);
        Client *toClient = client_get(toId);
        if (toClient) {
            client_write(toClient, packet);
            printf("%s->%s: %s\n", client->name, toClient->name, msg);
        }
        return;
    }

    if (channel == GLOBAL_CHANNEL) {
        client_all_write_except(packet, client->id);
        printf("[Global] %s: %s\n", client->name, msg);
        return;
    }

    if (channel == SERVER_CHANNEL) {
        printf("%s->Server : %s\n", client->name, msg);
        return;
    }

    client_channel_write_except(packet, channel, client->id);

    if(channel == IOS_CHANNEL) {
        printf("[iOS] %s: %s\n", client->name, msg);
    } else if(channel == ANDROID_CHANNEL) {
        printf("[Android] %s: %s\n", client->name, msg);
    }
}

void packet_process_login(Client *client) {
    buffer_set(client->readPacket, 1, (Byte) client->id); //Set Client ID
    char *name = (char *) (client->readPacket->buffer + 2);
    client_set_name(client, name);
    client_all_write(client->readPacket);

    //Sends data about each connected client to the newly logged in client for caching.
    for(int i = 0; i < client_list->size; i++) {
        Client *c = list_get(client_list, i);
        if(c->id != client->id) {
            Buffer *packet = packet_nid_create(c);
            client_write(client, packet);
            buffer_free(packet);
        }
    }

    printf("[NOTICE] %s logged in.\n", client->name);
}

void packet_process_logout(Client *client) {
    client_disconnect(client);
}

void packet_process_command(Client *client) {
    Byte commandId = buffer_get_at(client->readPacket, 1);
    Byte channel = buffer_get_at(client->readPacket, 2);
    int i;
    Client *c;
    char msg[41];

    switch(commandId) {
        case SWITCH_COMMAND:
            client->channel = channel;
            break;
        case LIST_COMMAND:
            snprintf(msg, 41, "List for channel %c", channel);
            client_write(client, packet_server_message_create(msg));
            for(i = 0; i < client_list->size; i++) {
                c = list_get(client_list, i);
                if(c->id != client->id && (c->channel == channel || channel == GLOBAL_CHANNEL) && c->name != NULL) {
                    snprintf(msg, 41, "%s : %d", c->name, c->id);
                    client_write(client, packet_server_message_create(msg));
                }
            }
            break;
        default:
            break;
    }
}

void packet_process_nid(Client *client) {

}

Buffer *packet_client_logout_create(int client_id) {
    Buffer *packet = packet_buffer_create(LOGOUT_PACKET);
    buffer_put(packet, (Byte) client_id); //Client ID
    buffer_flip(packet);
    return packet;
}

Buffer *packet_server_message_create(const char *msg) {
    char packet_msg[40];
    memset(packet_msg, 0, 40);

    strncpy(packet_msg, msg, 40);

    if (strlen(msg) > 40) {
        fprintf(stderr, "Message was truncated. Was: %s | Now: %s | (packet_server_message_create)\n", msg, packet_msg);
    }

    Buffer *packet = packet_buffer_create(CHAT_PACKET);
    buffer_put(packet, SERVER_CHANNEL); //Channel
    buffer_put(packet, 0xFF); //From
    buffer_put(packet, 0xFF); //To

    for (int i = 0; i < 40; i++) {
        buffer_put(packet, (Byte) packet_msg[i]); //Message
    }

    buffer_flip(packet); //Flip for writing
    return packet;
}

Buffer *packet_server_logout_create() {
    Buffer *packet = buffer_create(LOGOUT_PACKET);
    buffer_put(packet, 0xFF); //Client ID
    buffer_flip(packet); //Flip for writing
    return packet;
}

Buffer *packet_nid_create(Client *client) {
    Buffer *packet = packet_buffer_create(NID_PACKET);
    buffer_put(packet, (Byte) client->id);

    int name_len = (int) strlen(client->name);

    for(int i = 0; i < name_len; i++) {
        buffer_put(packet, (Byte) client->name[i]);
    }

    if(name_len < 15) {
        for(int i = name_len; i < 15; i++) {
            buffer_put(packet, 0x00);
        }
    }

    buffer_flip(packet);
    return packet;
}

void client_write(Client *client, Buffer *packet) {
    client_add_write(client, packet);
    if (!FD_ISSET(client->id, &wfd)) {
        FD_SET(client->id, &wfd);
    }
}

void client_all_write(Buffer *buffer) {
    for(int i = 0; i < client_list->size; i++) {
        Client *c = list_get(client_list, i);
        client_write(c, buffer);
    }
}

void client_all_write_except(Buffer *buffer, int client_id_except) {
    for(int i = 0; i < client_list->size; i++) {
        Client *c = list_get(client_list, i);
        if(c->id != client_id_except) {
            client_write(c, buffer);
        }
    }
}

void client_channel_write(Buffer *buffer, char channel) {
    for(int i = 0; i < client_list->size; i++) {
        Client *c = list_get(client_list, i);
        if(c->channel == channel || c->channel == GLOBAL_CHANNEL) {
            client_write(c, buffer);
        }
    }
}

void client_channel_write_except(Buffer *buffer, char channel, int client_id_except) {
    for(int i = 0; i < client_list->size; i++) {
        Client *c = list_get(client_list, i);
        if((c->channel == channel || c->channel == GLOBAL_CHANNEL) && c->id != client_id_except) {
            client_write(c, buffer);
        }
    }
}

void client_disconnect(Client *client) {
    FD_CLR(client->id, &rfd);
    FD_CLR(client->id, &wfd);
    close(client->id);

    Buffer *logout = packet_client_logout_create(client->id);

    list_remove_value(client_list, &client->id, (int (*)(void *, void *)) &client_equals);
    client_all_write(logout);

    printf("Client %d disconnected.\n", client->id);

    buffer_free(logout);
    client_free(client);
}

Client *client_get(int socket_fd) {
    int index = list_contains(client_list, (void *) &socket_fd, (int (*)(void *, void *)) &client_equals);

    if (index == -1) {
        return NULL;
    }

    return list_get(client_list, index);
}

int client_equals(Client *client, int *id) {
    if (client->id == *id) {
        return 1;
    } else {
        return 0;
    }
}

void client_list_free(Client *client) {
    client_free(client);
}