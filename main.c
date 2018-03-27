#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#define FALHA 1

#define	TAM_MEU_BUFFER	1000

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

/* -lrt */

int cria_socket_local(void)
{
	int socket_local;		/* Socket usado na comunicacao*/

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		return;
	}
	return socket_local;
}


struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino)
{
	struct sockaddr_in servidor; 	/* Endere o do servidor incluindo ip e porta */
	struct hostent *dest_internet;	/* Endere o destino em formato pr prio */
	struct in_addr dest_ip;		/* Endere o destino em formato ip num rico */

	if (inet_aton ( destino, &dest_ip ))
		dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
	else
		dest_internet = gethostbyname(destino);

	if (dest_internet == NULL) {
		fprintf(stderr,"Endere o de rede inv lido\n");
		exit(FALHA);
	}

	memset((char *) &servidor, 0, sizeof(servidor));
	memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(porta_destino);

	return servidor;
}




void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem)
{
	/* Envia msg ao servidor */

	if (sendto(socket_local, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{ 
		perror("sendto");
		return;
	}
}


int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER)
{
	int bytes_recebidos;		/* N mero de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}

int str_cut(char *str, int begin, int len)
{
    int l = strlen(str);

    if (len < 0) len = l - begin;
    if (begin + len > l) len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);

    return len;
}

int main(int argc, char* argv[])
{
	
	struct timespec t;
	int interval = 50000; /* 50us*/
	
	/*Variaveis de comunicação*/

	int porta_destino = atoi( argv[2]);

	int socket_local = cria_socket_local();

	struct sockaddr_in endereco_destino = cria_endereco_destino(argv[1], porta_destino);

	int i = 0;    
	char msg_enviada[1000];  
	char msg_recebida[1000];
	int nrec;

	/*Variaveis de controle*/
	float error_prior = 0;
	float error = 0;
	float integral = 0;
	float derivative = 0;
	float bias = 0.0000001;
	float KP = 10000;
	float KI = 50000;
	float Href = 2;
	float output = 0;
	double H = 0;
	char outputStr[] = "ani";
	char outputStr2[20];


        /* start after one second */
	clock_gettime(CLOCK_MONOTONIC ,&t);
	t.tv_sec++;

	while(1) {
		/* wait until next shot */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		/* do the stuff */
		
		/*Enviar mensagem*/
		// envia_mensagem(socket_local, endereco_destino, "st-0");
		/*Recebe mensagem*/
		// nrec = recebe_mensagem(socket_local, msg_recebida, 1000);

		/*
			Map das Chaves:
			sh-0 -> H -> Altura do nível de água
			aniX -> Ni -> Váriavel de controle. Entrada da água. X é a qtd.		
		*/
		envia_mensagem(socket_local, endereco_destino, "sh-0");
		nrec = recebe_mensagem(socket_local, msg_recebida, 1000);
		msg_recebida[nrec]='\0';
		//printf("String>>>%s\n", msg_recebida);

		str_cut(msg_recebida, 0, 3);
		H = atof(msg_recebida);
		//printf("String>>>%s Double>>>%lf\n", msg_recebida, H);
		
		error = Href - H;
		integral = integral + (error*interval/NSEC_PER_SEC);
		derivative = (error - error_prior)/(interval/NSEC_PER_SEC);
		output = KP*error + KI*integral + bias;
		error_prior = error;

		/* Saturação */
		if(output < 0){
			output = 0;
		}
		
		/* Envia String via UDP */
		snprintf(outputStr2, 20, "%lf", output); 
		strcat(outputStr, outputStr2);
		envia_mensagem(socket_local, endereco_destino, outputStr);
		nrec = recebe_mensagem(socket_local, msg_recebida, 1000);

		printf("Atuacao>>> %s\n", outputStr);
		strcpy(outputStr, "ani");


		/* calculate next shot */
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
				t.tv_nsec -= NSEC_PER_SEC;
				t.tv_sec++;
		}
    }
}





