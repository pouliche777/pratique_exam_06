#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>


typedef struct s_client
{
    int fd;
    int id;
    struct s_client *next;
    
}t_client;

t_client *clients_list = NULL;

int server_socket;
int id_counter;

fd_set russell;
fd_set machiavel;
fd_set spinoza;

char message[42];

char str[42*4096], tempo[42*4096], buffer[42*4096 + 42];

void fatal() 
{
    write (2, "Fatal error\n", strlen("Fatal error\n"));
    close(server_socket);
    exit(1);
}

int get_client_id(int fd){
    t_client *tempo = clients_list;

    while(tempo)
    {
        if (tempo->fd == fd)
            return (tempo->id);
        tempo= tempo->next;
    }
    return (-1);
}


int get_max_fd(){
    t_client *tempo = clients_list;
    int max = server_socket;
    while(tempo){
        if(tempo->fd > max)
            max = tempo->fd;
        tempo = tempo->next;
    }
    return (max);

}

void send_all(int fd, char *to_send){
    t_client *tempo = clients_list;
    //ici on envoie les messages \ tout le monde sauf a soi-meme
    while (tempo){
        if( tempo->fd != fd && FD_ISSET(tempo->fd, &spinoza)){
            if(send(tempo->fd, to_send, strlen(to_send), 0) < 0)
                fatal();
        }
        tempo= tempo->next;

    }
}


//grosse fonction pour ajouter un client (en general les fonction prennent toujours un fd) 
int  add_client_to_chain(int fd){
    t_client *tempo = clients_list;
    t_client *new_client;

    if(!(new_client= calloc(1, sizeof(t_client))))
        fatal();
    new_client->id = id_counter++;
    new_client->fd = fd;
    new_client->next = NULL;
    //sil na pas de client encore :
    if(!clients_list){
        clients_list = new_client;
    }
    else {
        while(tempo->next){
            tempo = tempo->next;
        }
        //on est rendu au dernier chainon de la list, on attache le nouveau \ la fin
        tempo->next = new_client;
    }
    //afin de pouvoir imprimmer facilement
    return (new_client->id);
}

//suveillance du fd etr creation du client
void surveille_fd(){
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int client_fd;
    //on accepte!
    if ((client_fd = accept(server_socket, (struct sockaddr *)&clientaddr, &len))<0)
        fatal();
    //preaparation du msg
    sprintf(message, "server: client %d just arrived\n", add_client_to_chain(client_fd));
    send_all(client_fd, message);
    FD_SET(client_fd, &russell);
}

int rm_client(int fd)
{
    t_client *tempo = clients_list;
    t_client *to_del;
    int id =get_client_id(fd);
    //si c<est le premier de la liste quil faut delete
    if(tempo && tempo->fd == fd){
        clients_list=tempo->next;
        free(tempo);
    }
    else{
        while(tempo && tempo->next && tempo->next->fd !=fd)
            tempo=tempo->next;
        to_del = tempo->next;
        //on remplace le next par le suivant de celui quon delete
        tempo->next= tempo->next->next;
        free(to_del);
    }
    return(id);
}

void build_message(int fd){
    int i =0;
    int j = 0;
    while (str[i]){
        //on met le contenu de str dabs tempo
        tempo[j] = str[i];
        j++;
        if(str[i] == '\n'){
            // on envoie la ligne (buffer sera la nouvelle string, celle quon envoie sur le socket)
            sprintf(buffer, "client %d: %s", get_client_id(fd), tempo);
            send_all(fd, buffer);
            j=0;
            bzero(&tempo, strlen(tempo));
            bzero(&buffer,strlen(buffer));
            
        }
        i++;
    }
    bzero(&str, strlen(str));
}

int main (int argc, char **argv){

    if(argc !=2)
    {
        write (2,"Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }
    struct sockaddr_in servaddr;
    uint16_t port= atoi(argv[1]);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 


    if((server_socket= socket(AF_INET, SOCK_STREAM, 0)) <0){ 
		fatal(); 
	} 
    if ((bind(server_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) { 
		fatal(); 
	} 
	if (listen(server_socket, 0) < 0) {
		fatal();
	}
    FD_ZERO (&russell);
    FD_SET(server_socket, &russell);
    //sizeof ici, et non str(len)
    bzero(&tempo, sizeof(tempo));
    bzero(&buffer, sizeof(buffer));
    bzero(&str, sizeof(str));

    //OK!  grosse boucle!!

    while(1){
            //en partant select
        spinoza= machiavel = russell;
        if(select(get_max_fd() + 1, &machiavel, &spinoza, NULL, NULL) < 0)
            continue;
    //on iter sur les fds
        for (int fd = 0; fd <= get_max_fd(); fd++){
            if(FD_ISSET(fd, &machiavel)){
                if(fd== server_socket){
                    bzero(&message, sizeof(message));
                    surveille_fd();
                    break;
                }
                else{
                    int ret_recv = 1000;
                    while (ret_recv == 1000 || str[strlen(str) - 1] != '\n'){
                        ret_recv = recv(fd, str +strlen(str), 1000, 0);
                        if(ret_recv <= 0)
                            break ;
                    }
                    if(ret_recv <= 0)
                    {
                        bzero(&message, sizeof(message));
                        sprintf(message, "server: client %d just left\n", rm_client(fd));
                        send_all(fd, message);
                        FD_CLR(fd, &russell);
                        close (fd);
                        break;

                    }
                    else{
                        build_message(fd);
                    }
                    
                }
            }
        }

    }

    return (0);

}

