#include "util.h"

//apontador para o ficheiro onde será escrito o relatório da simulação
FILE *relatorio;

//constantes
int ASCII = 65;

//variáveis globais
int tempoInicial, numProdutos, quantidadeProdutos;
int clientesFila = 0, clientesFilaPrio = 0, empregados = 1, clientesTotal = 0, clientesTotalPrio = 0, desistencias = 0, clientesTrocaram = 0, esperaram = 0, esperaramPrio = 0, somaEsperaram = 0, somaEsperaramPrio = 0;
int corre = 0, modo = 0, sair = 0;
//array de produtos
int* produtos;
int* produtosComprados;
char media[20];

//função que inicializa o nº de produtos dinamicamente
void criarProdutos() 
{
	int i;
	produtos = (int*) malloc(numProdutos * sizeof(int));
	for(i = 0; i < numProdutos; i++)
		produtos[i] = quantidadeProdutos;
	
	produtosComprados = (int*) malloc(numProdutos * sizeof(int));
	for(i = 0; i < numProdutos; i++)
		produtosComprados[i] = 0;
}

//tipo: 1 - média clientes normais, 2 - média clientes prioritários
void getMedia(int tipo) 
{
	int i, aux;
	
	if(tipo == 1) //normais
		aux = somaEsperaram / esperaram;
	else //if(tipo == 2)
		aux = somaEsperaramPrio / esperaramPrio;
	
	for(i = 0; aux >= 60; i++, aux-=60);
	
	if(i > 0)
		sprintf(media, "%d.%d horas.", i, aux);
	else
		sprintf(media, "%d minutos.", aux);
	
	//return buffer;
}

void mostraEstatistica()
{
	printf("\nEstatísticas da loja\n");
	if(corre == 1)
		printf("\n1. Estado atual: Simulação a decorrer.\n");
	else
	{
		printf("\n1. Estado atual: Simulação terminada.\n");
	}
	if(sair == 0)
	{
		printf("2. Clientes na fila: %d\n", clientesFila);
		printf("3. Clientes na fila prioritária: %d\n", clientesFilaPrio);
		printf("4. Empregados no balcão: %d\n", empregados);
	}
	
	int i, j;
	if(sair == 1)
	{
		for(i = 0, j = 2; i < numProdutos; i++, j++) 
			printf("%d. Quantidade produtos comprados %c: %d\n", j, i+ASCII, produtosComprados[i]);
	}
	else //if(sair == 0)
	{
		for(i = 0, j = 5; i < numProdutos; i++, j++) 
			printf("%d. Quantidade produto %c: %d\n", j, i+ASCII, produtos[i]);
	}
	
	printf("%d. Total de clientes: %d\n", j+1, clientesTotal);
	printf("%d. Total de clientes prioritários: %d\n", j+2, clientesTotalPrio);
	printf("%d. Desistências: %d\n", j+3, desistencias);
	printf("%d. Clientes que trocaram de produto: %d\n", j+4, clientesTrocaram);
	
	if(esperaram > 0)
	{
		getMedia(1);
		printf("%d. Tempo médio de espera de clientes na fila: %s\n", j+5, media);
	}
	else
		printf("%d. Tempo médio de espera de clientes na fila: 0 minutos\n", j+5);
	if(esperaramPrio > 0)
	{
		getMedia(2);
		printf("%d. Tempo médio de espera de clientes na fila prioritária: %s\n", j+6, media);
	}
	else 
		printf("%d. Tempo médio de espera de clientes na fila prioritária: 0 minutos\n", j+6);
	
	if(sair == 1)
		printf("\nAcabou a simulação. Digite 4 para sair.\n\n");
	else
		printf("\nModo estatística. Digite 3 para retormar ao modo simulação.\n\n");
}

//função que fica à escuta das mensagens do simulador
void *escuta_comunicacao(void *arg)
{
	int sockfd = *((int*) arg);
	int n, numComandos;
	int tempo, evento, cod1, cod2, cod3, cod4, cod5;
	char buffer[BUFFER_SIZE];

	//ciclo que fica à espera de mensagens do simulador
	while(1)
	{
		if((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
		{
			if(n < 0)
				perror("recv");
		}
		else
		{
			buffer[n] = '\0';
			numComandos = sscanf(buffer, "%d %d %d %d %d %d %d", &tempo, &evento, &cod1, &cod2, &cod3, &cod4, &cod5);
			
			if(numComandos > 0) 
			{
				switch(evento)
				{
					case 1:
						if(cod1 == 1) 
						{
							numProdutos = cod2;
							quantidadeProdutos = cod3;
							criarProdutos();
							tempoInicial = tempo;
							printf("%d:%d - A loja abriu aos clientes.\n", getMinutos(tempo), getSegundos(tempo));
							sprintf(buffer, "%d:%d - A loja abriu aos clientes.\n", getMinutos(tempo), getSegundos(tempo));
						}
						else if(cod1 == 2) 
						{
							printf("\n%d:%d - A loja vai fechar. Não podem entrar mais clientes.\n\n", getMinutos(tempo), getSegundos(tempo));
							sprintf(buffer, "\n%d:%d - A loja vai fechar. Não podem entrar mais clientes.\n\n", getMinutos(tempo), getSegundos(tempo));
						}
						else if(cod1 == 0) 
						{
							printf("%d:%d - A loja fechou.\n", getMinutos(tempo), getSegundos(tempo));
							sprintf(buffer, "%d:%d - A loja fechou.\n", getMinutos(tempo), getSegundos(tempo));
							sair = 1;
							system("clear");
							mostraEstatistica();
						}
							
						break;
					case 2:
						if(cod2 == 1)
						{
							if(modo == 0)
								printf("%d:%d - Entrou um cliente na loja. (Cliente nº %d)\n", getMinutos(tempo), getSegundos(tempo), cod1);
							sprintf(buffer, "%d:%d - Entrou um cliente na loja. (Cliente nº %d)\n", getMinutos(tempo), getSegundos(tempo), cod1);
							clientesFila++;
						}
						else //if(cod2 == 2)
						{
							if(modo == 0)
								printf("%d:%d - Entrou um cliente prioritário na loja. (Cliente nº %d)\n", getMinutos(tempo), getSegundos(tempo), cod1);
							sprintf(buffer, "%d:%d - Entrou um cliente prioritário na loja. (Cliente nº %d)\n", getMinutos(tempo), getSegundos(tempo), cod1);
							clientesFilaPrio++;
							clientesTotalPrio++;
						}
						clientesTotal++;
						break;
					case 3:
						if(modo == 0)
							printf("%d:%d - O cliente %d escolheu o produto %c e vai ser atendido pelo empregado %d.\n", getMinutos(tempo), getSegundos(tempo), cod1, cod2+ASCII, cod5);
						sprintf(buffer, "%d:%d - O cliente %d escolheu o produto %c e vai ser atendido pelo empregado %d .\n", getMinutos(tempo), getSegundos(tempo), cod1, cod2+ASCII, cod5);
						produtos[cod2]--;
						produtosComprados[cod2]++;
						if(cod3 == 1)
						{
							esperaram++;
							somaEsperaram += cod4;
						}
						else //if(cod3 == 2)
						{
							esperaramPrio++;
							somaEsperaramPrio += cod4;
						}
						if(cod3 == 1)
						  clientesFila--;
						else //if(cod3 == 2)
						  clientesFilaPrio--;
						break;
					case 4:
						if(modo == 0)
							printf("%d:%d - O cliente %d foi atendido e saiu da loja.\n", getMinutos(tempo), getSegundos(tempo), cod1);
						sprintf(buffer, "%d:%d - O cliente %d foi atendido e saiu da loja.\n", getMinutos(tempo), getSegundos(tempo), cod1);
						break;
					case 5:
						if(modo == 0)
							printf("%d:%d - O cliente %d desistiu por achar que a fila está muito grande.\n", getMinutos(tempo), getSegundos(tempo), cod1);
						sprintf(buffer, "%d:%d - O cliente %d desistiu por achar que a fila está muito grande.\n", getMinutos(tempo), getSegundos(tempo), cod1);
						desistencias++;
						break;
					case 6:
						if(modo == 0)
							printf("%d:%d - O cliente %d decidiu trocar o produto %c pelo produto %c.\n", getMinutos(tempo), getSegundos(tempo), cod1, cod2+ASCII, cod3+ASCII);
						sprintf(buffer, "%d:%d - O cliente %d decidiu trocar o produto %c pelo produto %c.\n", getMinutos(tempo), getSegundos(tempo), cod1, cod2+ASCII, cod3+ASCII);
						produtos[cod2]++;
						produtosComprados[cod2]--;
						produtos[cod3]--;
						produtosComprados[cod3]++;
						clientesTrocaram++;
						break;
					case 7:
						if(modo == 0)
							printf("%d:%d - O gerente chamou um novo empregado para o balcão.\n", getMinutos(tempo), getSegundos(tempo));
						sprintf(buffer, "%d:%d - O gerente chamou um novo empregado para o balcão.\n", getMinutos(tempo), getSegundos(tempo));
						empregados++;
						break;
					case 8:
						if(modo == 0)
							printf("%d:%d - O empregado %d foi fazer reposição do produto %c.\n", getMinutos(tempo), getSegundos(tempo), cod3, cod1+ASCII);
						sprintf(buffer, "%d:%d - O empregado %d foi fazer reposição do produto %c.\n", getMinutos(tempo), getSegundos(tempo), cod3, cod1+ASCII);
						produtos[cod1] += cod2;
						break;
					case 9:
						if(modo == 0)
							printf("%d:%d - O gerente retirou um empregado do balcão.\n", getMinutos(tempo), getSegundos(tempo));
						sprintf(buffer, "%d:%d - O gerente retirou um empregado do balcão.\n", getMinutos(tempo), getSegundos(tempo));
						empregados--;
						break;
					default:
						break;
				}
				fputs(buffer, relatorio);
			}
		}
	}
	return NULL;
}

int getMinutos(int tempo)
{
	return (tempo - tempoInicial) / 60;
}

int getSegundos(int tempo)
{
	int segundos = tempo - tempoInicial;
	
	while(segundos >= 60)
		segundos -= 60;
	
	return segundos;
}

int main(int argc, char **argv)
{	
	struct sockaddr_un serv_addr, cli_addr;
	int sockfd, servlen, newsockfd;
	int n, numComandos;

	int clilen = sizeof(cli_addr);
	char opcao[3];
	
	//Criacao do socket UNIX
	if((sockfd=socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		perror("cant open socket stream");
	serv_addr.sun_family=AF_UNIX;
	strcpy(serv_addr.sun_path, UNIXSTR_PATH);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
	unlink(UNIXSTR_PATH);
	if(bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
		perror("cant bind local address");
	listen(sockfd, 1);

	if((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0)
		perror("accept error");
		
	//criacao da tarefa que ira tratar da comunicação com o simulador
	pthread_t thread;
	pthread_create(&thread, NULL, &escuta_comunicacao, &newsockfd);
		
	unlink("relatorio.log");
	relatorio = fopen("relatorio.log", "wb");
	
	system("clear");
	printf("\nSimulador Loja\n\n");
	printf("Para começar e durante a simulação pode usar as seguintes opções:\n");
	printf("1 - Começar Simulação \n");
	printf("2 - Mostrar modo estatística \n");
	printf("3 - Retomar modo simulação \n");
	printf("4 - Sair \n\n");
	
	do
	{	
		fgets(opcao, sizeof(opcao), stdin);

		if(send(newsockfd, opcao, sizeof(opcao), 0) == -1)
		{
			perror("send");
			exit(1);
		}
	
		switch(atoi(opcao)) 
		{
			case 1:
				corre = 1;
				break;
			case 2:
				modo = 1;
				mostraEstatistica();
				break;
			case 3:
				modo = 0;
				break;
			case 4:
				if(sair == 1)
					exit(0);
				sair = 1;
				break;
			default:
				printf("Inseriu uma opção errada.\nOpções: 1-Começar; 2-Mostrar estatística; 3-Retomar simulação; 4-Sair \n\n");
		}
	}while(!sair);

	system("clear");
	mostraEstatistica();
	fclose(relatorio);
	close(sockfd);
	return 0;
}
