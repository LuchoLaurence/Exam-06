#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int		count = 0, max_fd = 0;
int		ids[65536];
char	*msgs[65536];

fd_set	rfds, wfds, afds;
char	buf_read[1001], buf_write[42];


// START COPY-PASTE FROM GIVEN MAIN

int extract_message(char **buf, char **msg)
// remplace msg par la valeur avant le /n et buf par la valeur apres le /n
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
// concatene add a buf, free buf et retourne le resultat
{

	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

// END COPY-PASTE


void	fatal_error()
//error("Fatal error\n");
{
	write(2, "Fatal error\n", 12);
	exit(1);
}


//fonction qui sert a notifier les autres clients que author qui a envoye un message
void	notify_other(int author, char *str)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &wfds) && fd != author) // verifie que fd est bien dans wfds
			send(fd, str, strlen(str), 0); // envoie le message au client associe a fd
	}
}
//Lorsqu'un client se connecte ou se déconnecte, le serveur utilise notify_other pour informer tous les autres clients de l'événement.
//Lorsque l'un des clients envoie un message, le serveur utilise cette fonction pour relayer le message à tous les autres clients.



// ajoute un client lors de la connexion au serveur et le notifie aux autres clients
void	register_client(int fd)
{
	max_fd = fd > max_fd ? fd : max_fd; // mis a jour de la valeur de max fd
	ids[fd] = count++; // associe un id unique a chaque client = count++ = 0, 1, 2, ...
	msgs[fd] = NULL; // initialisation de msgs[fd] a NULL
	FD_SET(fd, &afds); // ajoute fd a l'ensemble de descripteurs de fichiers
	sprintf(buf_write, "server: client %d just arrived\n", ids[fd]); //stock dans buf_write le message de connexion
	notify_other(fd, buf_write); // notifie les autres clients de la connexion
}

void	remove_client(int fd)
{
	sprintf(buf_write, "server: client %d just left\n", ids[fd]); // stock dans buf_write le message de deconnexion
	notify_other(fd, buf_write);
	free(msgs[fd]); // libere msgs[fd]
	FD_CLR(fd, &afds); // supprime fd de l'ensemble de descripteurs de fichiers
	close(fd); // ferme la connexion avec le client
}

void	send_msg(int fd)
{
	char *msg;

	while (extract_message(&(msgs[fd]), &msg)) // tant que extract_message renvoie 1
	{
		sprintf(buf_write, "client %d: ", ids[fd]);
		notify_other(fd, buf_write);
		notify_other(fd, msg);
		free(msg);
	}
}

int		create_socket() // create a socket and return its file descriptor
{
	max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (max_fd < 0)
		fatal_error();
	FD_SET(max_fd, &afds); // ajoute max_fd a l'ensemble de descripteurs de fichiers
	return max_fd;
}

int		main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	FD_ZERO(&afds); // initialise l'ensemble de descripteurs de fichiers
	int sockfd = create_socket();

	// START COPY-PASTE FROM MAIN

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) // bind la socket a l'adresse
		fatal_error();
	if (listen(sockfd, SOMAXCONN)) // met le socket en mode ecoute
		fatal_error();

	// END COPY-PASTE

	while (1)
	{
		rfds = wfds = afds;

		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0) // bloque le processus jusqu'a ce qu'un descripteur de fichier soit pret pour la lecture, l'ecriture ou une exception
			fatal_error();

		for (int fd = 0; fd <= max_fd; fd++) // parcourt tous les descripteurs de fichiers
		{
			if (!FD_ISSET(fd, &rfds)) // verifie si fd est bien dans rfds
				continue;

			if (fd == sockfd) // si le socket serveur est le fd actuel
			{
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len); // attend la connexion d'un client
				if (client_fd >= 0)
				{
					register_client(client_fd);
					break ;
				}
			}
			else // sinon c'est un client deja connecte
			{
				int read_bytes = recv(fd, buf_read, 1000, 0); // read_bytes contient le nombre d'octets lus
				if (read_bytes <= 0) // si erreur le client se deconnecte
				{
					remove_client(fd);
					break ;
				}
				buf_read[read_bytes] = '\0'; // ajoute un caractere de fin de chaine
				msgs[fd] = str_join(msgs[fd], buf_read); // met a jour la chaine de message :msgs[fd] avec le message recu pour ce fd specifique
				send_msg(fd); // envoie le message recu a tous les autres clients
			}
		}
	}
	return 0;
}
