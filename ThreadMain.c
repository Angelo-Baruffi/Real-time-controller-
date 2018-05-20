// This controller have threads

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
#include <pthread.h>

#define FALHA 1

#define	TAM_MEU_BUFFER	1000

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

#define AMOSTRAS_TO_GET 10000

#define N_MAX 10000


/* -lrt */

//Threads parameters
pthread_mutex_t SP_H_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t SP_T_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t emH = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cmd = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t H_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t T_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_t T_thread;
pthread_t H_thread;
pthread_t print_thread;
pthread_t SP_thread;
pthread_t alert_thread;
pthread_t write_thread;

pthread_cond_t condH = PTHREAD_COND_INITIALIZER;

char* arg[3];

double tempos_H[N_MAX];
struct timespec tempo_H;
double tempos_T[N_MAX];
struct timespec tempo_T;
FILE *fH;
float Href;
float Tref;

double T;
double H;

//Vars to monitor
int iH;
int iHl;


int cria_socket_local(void)
{
	int socket_local;		/* Socket usado na comunicacao*/

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		return socket_local;
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

/*_______________________________________________________*/
/*_______________________________________________________*/
void insereH(double value){
	pthread_mutex_lock(&emH);
	if(iH < N_MAX){
		tempos_H[iH] = value;
		iH++;
		pthread_cond_signal( &condH ); 

	}	
	pthread_mutex_unlock(&emH);
}
void escreveH(void){
	float aux;
	pthread_mutex_lock(&emH);
	while(iHl >= iH){
		pthread_cond_wait( & condH, &emH );
	}
	aux = tempos_H[iHl];
	iHl++;	
	pthread_mutex_unlock(&emH);
	fprintf(fH, "%lf\n", aux/1000);

}

void h_cntroller(void){ // Ni e Nf
// Thread to control the H
	struct timespec t;
	int interval = 50000000; /* 50ms*/
	
	/*Variaveis de comunicação*/

	int porta_destino = atoi( arg[2]);

	int socket_local = cria_socket_local();

	struct sockaddr_in endereco_destino = cria_endereco_destino(arg[1], porta_destino);

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
	float output = 0;
	char outputStr[] = "ani";
	char outputStr2[20];

	int amostras = 0;
	
	//Aux vars
	double Haux;
	float Href_aux;
	
	double aux_tmp;


	clock_gettime(CLOCK_MONOTONIC ,&t);
	//t.tv_sec++;

	while(amostras != AMOSTRAS_TO_GET){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		envia_mensagem(socket_local, endereco_destino, "sh-0");
		nrec = recebe_mensagem(socket_local, msg_recebida, 1000);
		msg_recebida[nrec]='\0';

		str_cut(msg_recebida, 0, 3);
		
		pthread_mutex_lock(&H_mtx);
		H = atof(msg_recebida);	
		Haux = H;	
		pthread_mutex_unlock(&H_mtx);
				
		pthread_mutex_lock(&SP_H_mtx);
		Href_aux = Href;
		pthread_mutex_unlock(&SP_H_mtx);
				
		error = Href_aux - Haux;
		

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
		strcpy(outputStr, "ani");

		//Calcula tempo de execucao
		clock_gettime(CLOCK_MONOTONIC ,&tempo_H);
		aux_tmp = (double) difftime(tempo_H.tv_nsec, t.tv_nsec);
		if(aux_tmp > 0)insereH(aux_tmp);

		/* calculate next shot */
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
				t.tv_nsec -= NSEC_PER_SEC;
				t.tv_sec++;
		}
	}

}

void t_controller(void){ // Q e o Na
// Thread to control the T
	struct timespec t;
	int interval = 70000000; /* 70ms*/
	
	/*Variaveis de comunicação*/

	int porta_destino = atoi( arg[2]);

	int socket_local = cria_socket_local();

	struct sockaddr_in endereco_destino = cria_endereco_destino(arg[1], porta_destino);

	char msg_enviada[1000];  
	char msg_recebida[1000];
	int nrec;

	/*Variaveis de controle*/
	float error_prior = 0;
	float error = 0;
	float integral = 0;
	float derivative = 0;
	float bias = 0.0000001;
	float KP = 100000000;
	float KI = 5000000;
	float output = 0;
	char outputStr[] = "aq-";
	char outputStr2[20];

	int amostras = 0;

	int intToPrint = 0;
	
	//Aux vars
	double Taux;
	float Tref_aux;


	clock_gettime(CLOCK_MONOTONIC ,&t);
	//t.tv_sec++;

	while(amostras != AMOSTRAS_TO_GET){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		envia_mensagem(socket_local, endereco_destino, "st-0");
		nrec = recebe_mensagem(socket_local, msg_recebida, 1000);
		msg_recebida[nrec]='\0';


		str_cut(msg_recebida, 0, 3);
		
		pthread_mutex_lock(&T_mtx);
		T = atof(msg_recebida);
		Taux = T;
		pthread_mutex_unlock(&T_mtx);
		
		pthread_mutex_lock(&SP_T_mtx);
		Tref_aux = Tref;
		pthread_mutex_unlock(&SP_T_mtx);

		error = Tref_aux - Taux;

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
		strcpy(outputStr, "aq-");

		//Calcula tempo de execucao
		clock_gettime(CLOCK_MONOTONIC ,&tempo_T);
		tempos_T[amostras] = (double) difftime(tempo_T.tv_nsec, t.tv_nsec);
		if(tempos_T[amostras] > 0)amostras++;

  

		/* calculate next shot */
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
				t.tv_nsec -= NSEC_PER_SEC;
				t.tv_sec++;
		}
	}


}

void alert(void){
	// Thread to alert a max temp
		struct timespec t;
	int interval = 10000000; /* 10ms*/
	
	/*Variaveis de comunicação*/

	int porta_destino = atoi( arg[2]);

	int socket_local = cria_socket_local();

	struct sockaddr_in endereco_destino = cria_endereco_destino(arg[1], porta_destino);

	char msg_enviada[1000];  
	char msg_recebida[1000];
	int nrec;

	/*Variaveis de controle*/
	
	double T = 0;

	clock_gettime(CLOCK_MONOTONIC ,&t);

	while(1){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		envia_mensagem(socket_local, endereco_destino, "st-0");
		nrec = recebe_mensagem(socket_local, msg_recebida, 1000);
		//printf("%c \n", msg_recebida);
		msg_recebida[nrec]='\0';
		str_cut(msg_recebida, 0, 3);
		T = atof(msg_recebida);
		
		if(T >= 50){
			pthread_mutex_lock(&cmd);

			printf("ALERT T= %lf\n", T);
			
			pthread_mutex_unlock(&cmd);

		}
		
		/* calculate next shot */
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
				t.tv_nsec -= NSEC_PER_SEC;
				t.tv_sec++;
		}
	}


}

void show_vars(void){
// Thread to print the vars
	
	double T_aux;
	double H_aux;
	
	while(1){
		usleep(1000*1000);
		// Mostrar cada resultado a aproximadamente 1s
		
		pthread_mutex_lock(&cmd);
		pthread_mutex_lock(&T_mtx);
		pthread_mutex_lock(&H_mtx);
		T_aux = T;
		H_aux = H;
		pthread_mutex_unlock(&cmd);
		pthread_mutex_unlock(&T_mtx);
		pthread_mutex_unlock(&H_mtx);
			
		printf("Temperatura do Sistema: %lf. Altura do Nivel: %lf. SP.T:%f. SP.H: %f. Save Values: %i\n", T_aux, H_aux, Tref, Href, iHl);

	}
	
}

void get_SP(void){
// Thread to get the SP

	float SP;
	char SPstr[20];
	char SPtype;
		
	char prev;
	
	
	while(1){
	    char c = getchar();

		if(c == '\n' && prev != '\n'){
			pthread_mutex_lock(&cmd);
			printf( "Enter a value with TX.X or HX.X:  ");
			scanf("%s", &SPstr);
			printf("The value is %s\n", SPstr);
			pthread_mutex_unlock(&cmd);
			prev = c;
			
			SPtype = SPstr[0];
			str_cut(SPstr, 0, 1);		
			SP = atof(SPstr);

			
			if(SPtype == 'T'){
				pthread_mutex_lock(&SP_T_mtx);
				Tref = SP;
				pthread_mutex_unlock(&SP_T_mtx);
			
			}else if(SPtype == 'H'){
				pthread_mutex_lock(&SP_H_mtx);
				Href = SP;
				pthread_mutex_unlock(&SP_H_mtx);

			}else{
				pthread_mutex_lock(&cmd);
				printf( "SP TYPE ERROR\n");
				pthread_mutex_unlock(&cmd);
			}		
	
		}else{
			prev = 'c';
		}

	}
	

}

void writeToDoc(void){
  while(iHl < N_MAX){
	  escreveH();

	}
	fclose(fH);

}

int main(int argc, char* argv[]){
	
    fH = fopen("timesH.txt", "w");
	iH = 0;
	iHl = 0;
	// Set points
	Href = 2;
	Tref = 20;
	arg[2] = argv[2];
	arg[1] = argv[1];
/*
	while(amostras != AMOSTRAS_TO_GET) {
		


		//printf("Atuacao>>> %s\n", outputStr);
		
		// Mostrar cada resultado a aproximadamente 1s
		if(intToPrint == 20){
			printf("Saida do sistema: %lf. Atuacao: %s. Amostras: %i\n", H,msg_recebida, amostras);
			intToPrint = 0;
		}else{
			intToPrint++;
		}

		
		
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
				t.tv_nsec -= NSEC_PER_SEC;
				t.tv_sec++;
		}
    }
    
    
    //Salva a lista de tempo2 das amostras
	for (i=0; i<AMOSTRAS_TO_GET; ++i)
        //fprintf(f, "%d, %d\n", i, i*i);
        fprintf(f, "%lf\n", tempos_H[i]/1000);
        
    
    
*/
	
	pthread_create(&T_thread, NULL,(void *) t_controller, NULL);
	pthread_create(&H_thread, NULL,(void *) h_cntroller, NULL);
	pthread_create(&print_thread, NULL,(void *) show_vars, NULL);
	pthread_create(&SP_thread, NULL,(void *) get_SP, NULL);
	pthread_create(&alert_thread, NULL,(void *) alert, NULL);
	pthread_create(&write_thread, NULL,(void *) writeToDoc, NULL);
        
    while(1){}
	fclose(fH);

}
