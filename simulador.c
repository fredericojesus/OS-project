#include "util.h"
#include <signal.h>

//SOCKET
struct sockaddr_un serv_addr;
int sockfd, servlen;

//SEMAFOROS
sem_t fila_normal;
sem_t fila_prioritaria;

//TRINCOS
pthread_mutex_t mutex_cliente;
pthread_mutex_t mutex_empregado;
pthread_mutex_t mutex_stock;

//CONFIGURAÇÃO
struct configuracao configs;

//ESTRUTURA DE DADOS PARA GUARDAR AS INFORMAÇÕES CLIENTE/EMPREGADO
typedef struct
{
	int clienteId;
	int tempoChegadaAtendimento;
	int produto;
	int trocouProduto; //1 se sim, 0 se não
	sem_t espera_atendimento;
	sem_t espera;
} dadosEmpregado;

//VARIÁVEIS GLOBAIS
int idClienteCount = 1, clientesNormais = 0, clientesPrioritarios = 0, clientesLoja = 0, numeroEmpregados = 0, empregadoDisponivel = 0, sairEmpregado = 0;
int soma_1; //se o número de produto for ímpar é 1 se for par é 0. Ajuda a decidir que a primeira metade dos produtos tem o tempo um...
int* stockProdutos;
dadosEmpregado* empregado;

//CONSTANTES
int UM_SEGUNDO = 1000000, RONDA_GERENTE = 15000000;

//VARIÁVEIS DE CONTROLO
int corre = 0, sair = 0;

// Função que inicializa o array de estruturas que vão guardar os dados de atendimento dos empregados dinamicamente
void initDadosEmpregados()
{
	int i;
	empregado = (dadosEmpregado*) malloc(configs.numero_max_empregados * sizeof(dadosEmpregado));
	for(i = 0; i < configs.numero_max_empregados; i++) 
	{
		empregado[i].clienteId = -1;
		empregado[i].trocouProduto = 0;
		sem_init(&empregado[i].espera_atendimento, 0, 0);
		sem_init(&empregado[i].espera, 0, 0);
	}
}

// Função que inicializa o nº de produtos dinamicamente
void criarProdutos() 
{
	int i;
	stockProdutos = (int*) malloc(configs.numero_produtos * sizeof(int));
	for(i = 0; i < configs.numero_produtos; i++)
		stockProdutos[i] = configs.stock_inicial_produtos;
}

// Função que trata a tarefa dos clientes. tipo = 1 clientes normais, tipo = 2 clientes prioritarios
void *tarefa_cliente(void *ptr)
{
	clientesLoja++;
	int tipo, idCliente, esperaZero = 0;
	int randCliente = rand()%100 + 1; //número random entre 1 e 100
	if(randCliente <= configs.prob_cliente_ser_prio)
		tipo = 2;  //cliente prioritário
	else
		tipo = 1;  //cliente normal

	int tempoChegada, tempoEsperaFila, produto_2 = -1; 
	char buffer[BUFFER_SIZE];
	
	pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
	idCliente = idClienteCount++;
	if(tipo == 1)
		clientesNormais++;
	else //if(tipo == 2)
		clientesPrioritarios++;
	pthread_mutex_unlock(&mutex_cliente); //############################################# UNLOCK #############################################
	
	tempoChegada = (int)time(0);
	//escreve e envia o socket do evento 2 (Entrada de cliente na loja)
	sprintf(buffer, "%d 2 %d %d\n", (int)time(0), idCliente, tipo);
	send(sockfd, buffer, sizeof(buffer), 0);
	
	//verifica se o cliente quer desistir
	if(clienteDesiste(tipo))
	{
		//escreve e envia o socket do evento 5 (Cliente desiste porque acha que a fila está muito grande)
		sprintf(buffer, "%d 5 %d %d %d\n", (int)time(0), idCliente, tipo);
		send(sockfd, buffer, sizeof(buffer), 0);
		clientesLoja--;
		return;
	}
	
	if(tipo == 1)
		sem_wait(&fila_normal); //############################################### WAIT ###############################################
	
	sem_wait(&fila_prioritaria); //############################################### WAIT ###############################################
	
	//não é preciso proteger com um trinco porque está na tarefa empregado, o empregadoDisponivel só vai mudar depois do cliente mudar os dados do atendimento
	int idEmpregado = empregadoDisponivel;
	
	//retira o cliente da fila
	pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
	if(tipo == 2)
		clientesPrioritarios--;
	else
		clientesNormais--;
	pthread_mutex_unlock(&mutex_cliente); //############################################# UNLOCK #############################################
	
	//cliente é atendido
	tempoEsperaFila = (int)time(0) - tempoChegada;
	//cliente escolhe um produto entre os disponíveis
	empregado[idEmpregado].produto = rand()%configs.numero_produtos; //número random entre 0 e configs.numero_produtos-1
	empregado[idEmpregado].tempoChegadaAtendimento = tempoChegada + tempoEsperaFila;
	empregado[idEmpregado].clienteId = idCliente;
	//avisa o empregado que já preencheu a estrutura
	sem_post(&empregado[idEmpregado].espera); //############################################### POST ###############################################
	
	//escreve e envia o socket do evento 3 (Cliente escolhe produto)
	sprintf(buffer, "%d 3 %d %d %d %d %d\n", (int)time(0), idCliente, empregado[idEmpregado].produto, tipo, tempoEsperaFila, idEmpregado+1);
	send(sockfd, buffer, sizeof(buffer), 0);
	
	while(1)
	{
		sem_wait(&empregado[idEmpregado].espera_atendimento); //############################################### WAIT ###############################################
		
		int randAlterarProduto = rand()%100 + 1; //número random entre 1 e 100
		
		//o cliente só pode trocar de produto uma vez
		if(produto_2 == -1)
		{
			produto_2 = 0;
			
			//se rand <= ... cliente vai trocar de produto
			if(randAlterarProduto <= configs.prob_cliente_alterar_produto)
			{
				//cliente escolhe um produto entre os disponíveis
				produto_2 = rand()%configs.numero_produtos; //número random entre 0 e configs.numero_produtos-1
				
				//ciclo para prevenir que o cliente troque de produto para o que já tinha escolhido antes
				while(produto_2 == empregado[idEmpregado].produto)
				{
					//cliente escolhe um produto entre os disponíveis
					produto_2 = rand()%configs.numero_produtos; //número random entre 0 e configs.numero_produtos-1
				}
				
				int tempoTrocouDeProduto = (int)time(0);
				
				//escreve e envia o socket do evento 6 (Cliente trocou de produto)
				sprintf(buffer, "%d 6 %d %d %d\n", tempoTrocouDeProduto, idCliente, empregado[idEmpregado].produto, produto_2);
				send(sockfd, buffer, sizeof(buffer), 0);
				
				empregado[idEmpregado].produto = produto_2;
				empregado[idEmpregado].tempoChegadaAtendimento = tempoTrocouDeProduto;
				empregado[idEmpregado].trocouProduto = 1;
				
				//o cliente avisa o empregado que quer trocar de produto
				sem_post(&empregado[idEmpregado].espera); //############################################### POST ###############################################
				continue;
			}
		}
		
		//o cliente avisa o empregado que está satisfeito com o produto e vai sair da loja(não quis trocar de produto)
		sem_post(&empregado[idEmpregado].espera); //############################################### POST ###############################################
		
		break;
	}
	
	//escreve e envia o socket do evento 4 (Cliente é atendido e sai da loja)
	sprintf(buffer, "%d 4 %d %d %d\n", (int)time(0), idCliente, tipo);
	send(sockfd, buffer, sizeof(buffer), 0);
	
	clientesLoja--;
}

// Função que devolve 1(true) se o cliente vai desistir, e 0(false) se não
int clienteDesiste(int tipo)
{
	int auxClientesNormais = clientesNormais, auxNumeroEmpregados = numeroEmpregados;
	if(auxNumeroEmpregados == 0)
		auxNumeroEmpregados = 1;
	
	//se o cliente for prioritário só vai ter em conta o nº de clientes prioritários na fila
	if(tipo == 2)
	{
		auxClientesNormais = 0;
		auxNumeroEmpregados = 1;
	}
	
	if(((auxClientesNormais + clientesPrioritarios) / auxNumeroEmpregados) > configs.tamanho_fila_desistir)
	{
		int randDesistir = rand()%100 + 1; //número random entre 1 e 100
		//se rand <= ... cliente vai desistir porque acha que a fila está muito grande
		if(randDesistir <= configs.prob_cliente_desistir)
		{
			//retira o cliente da fila
			pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
			if(tipo == 2)
				clientesPrioritarios--;
			else
				clientesNormais--;
			pthread_mutex_unlock(&mutex_cliente); //############################################# UNLOCK #############################################
			return 1;
		}
	}
	
	return 0;
}

// Função que trata a tarefa dos empregados.
void *tarefa_empregado(void *ptr)
{
	int tempoSaidaAtendimento;
	int idEmpregado, produto;
	char buffer[BUFFER_SIZE];
	
	pthread_mutex_lock(&mutex_empregado); //############################################### LOCK ###############################################
	idEmpregado = numeroEmpregados++;
	pthread_mutex_unlock(&mutex_empregado); //############################################# UNLOCK #############################################
	
	while(1)
	{
		pthread_mutex_lock(&mutex_empregado); //############################################### LOCK ###############################################
		empregadoDisponivel = idEmpregado;
		//reset à variável que vai dizer se o cliente decidiu trocar de produto(1) ou não(-1)
		empregado[idEmpregado].trocouProduto = 0;
		
		pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
		//se não tem clientes prioritários à espera, assinalamos o semáforo da fila normal
		if(clientesPrioritarios == 0)
			sem_post(&fila_normal); //############################################### POST ###############################################
		pthread_mutex_unlock(&mutex_cliente); //############################################# UNLOCK #############################################
		//assinala o semáforo da fila prioritária, ou seja, vai atender um cliente
		sem_post(&fila_prioritaria); //############################################### POST ###############################################

		//espera que o cliente introduza as suas informações na estrutura
		sem_wait(&empregado[idEmpregado].espera); //############################################### WAIT ###############################################
		pthread_mutex_unlock(&mutex_empregado); //############################################# UNLOCK #############################################
		
		while(1)
		{
			produto = empregado[idEmpregado].produto;
			//reset à variável que vai dizer se o cliente decidiu trocar de produto(1) ou não(-1)
			empregado[idEmpregado].trocouProduto = 0;
			
			//a primeira metade dos produtos têm um tempo de atendimento, e a outra metade outro
			//se o nº de produtos diferentes for ímpar, será a primeira metade + 1, ex: 5 produtos, 3 primeiros têm um tempo (primeira metade), 2 últimos têm outro tempo
			if(produto < ((configs.numero_produtos/2) + soma_1))
				tempoSaidaAtendimento = empregado[idEmpregado].tempoChegadaAtendimento + configs.tempo_produto_um;
			else
				tempoSaidaAtendimento = empregado[idEmpregado].tempoChegadaAtendimento + configs.tempo_produto_dois;
			
			while(1)
			{
				if((int)time(0) >= tempoSaidaAtendimento) 
				{
					//assinala o semáforo que está a por o cliente à espera de ser atendido, ou seja, o cliente já foi atendido.
					sem_post(&empregado[idEmpregado].espera_atendimento); //############################################### POST ###############################################
					break;
				}
			}
			
			//espera que o cliente decida se vai trocar de produto ou não, se não quer dizer que este vai sair da loja satisfeito
			sem_wait(&empregado[idEmpregado].espera); //############################################### WAIT ###############################################
			
			//se o cliente decidiu trocar de produto, o empregado vai voltar a atende-lo
			if(empregado[idEmpregado].trocouProduto == 1)
				continue;
			
			pthread_mutex_lock(&mutex_stock); //############################################### LOCK ###############################################
			stockProdutos[produto]--; //retira produto do stock
			pthread_mutex_unlock(&mutex_stock); //############################################# UNLOCK #############################################
			break;
		}
		
		pthread_mutex_lock(&mutex_stock); //############################################### LOCK ###############################################
		//repõe stock do produto comprado pelo cliente se necessário
		if(stockProdutos[produto] <= configs.min_stock_produto)
		{
			int i;
			//repõe o stock do produto para o inicial
			for(i = 0; stockProdutos[produto] < configs.stock_inicial_produtos; i++)
				stockProdutos[produto]++;
		
			//escreve e envia o socket do evento 8 (Reposição de produto)
			sprintf(buffer, "%d 8 %d %d %d\n", (int)time(0), produto, i, idEmpregado+1);
			send(sockfd, buffer, sizeof(buffer), 0);
		}
		pthread_mutex_unlock(&mutex_stock); //############################################# UNLOCK #############################################
		//tempo que o empregado demora a repor um produto
		usleep(configs.tempo_reposicao * UM_SEGUNDO);
		
		//verifica se o gerente o mandou sair
		pthread_mutex_lock(&mutex_empregado); //############################################### LOCK ###############################################
		if(idEmpregado == numeroEmpregados-1 && sairEmpregado == 1)
		{
			sairEmpregado = 0;
			numeroEmpregados--;
			pthread_mutex_unlock(&mutex_empregado); //############################################# UNLOCK #############################################
			return;
		}
		pthread_mutex_unlock(&mutex_empregado); //############################################# UNLOCK #############################################
	}
}

// Função que trata de chamar ou retirar empregados.
void *tarefa_gerente(void *ptr)
{
	pthread_t thread;
	char buffer[BUFFER_SIZE];
	int precisaEmpregado = configs.max_clientes_fila;
	
	while(1)
	{
		//o gerente faz uma ronda à loja de x em x tempo
		usleep(configs.ronda_gerente * UM_SEGUNDO);
		
		if(sair == 1)
			return;
		
		pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
		//verifica se é necessário chamar um empregado extra ao balcão
		if((clientesNormais + clientesPrioritarios) >= precisaEmpregado && numeroEmpregados < configs.numero_max_empregados)
		{
			precisaEmpregado += configs.max_clientes_fila;
			
			//chama um novo empregado
			pthread_create(&thread, NULL, &tarefa_empregado, &sockfd);
			
			//escreve e envia o socket do evento 7 (Novo empregado ao balcão)
			sprintf(buffer, "%d 7\n", (int)time(0));
			send(sockfd, buffer, sizeof(buffer), 0);
			pthread_mutex_unlock(&mutex_cliente); //############################################### UNLOCK ###############################################
			continue;
		}
		pthread_mutex_unlock(&mutex_cliente); //############################################### UNLOCK ###############################################
		
		pthread_mutex_lock(&mutex_cliente); //############################################### LOCK ###############################################
		//verifica se é necessário ter tantos empregados, se não retira um
		if(((clientesNormais + clientesPrioritarios) < (precisaEmpregado - configs.max_clientes_fila)) && precisaEmpregado > configs.max_clientes_fila) {
			sairEmpregado = 1;
			precisaEmpregado -= configs.max_clientes_fila;
			
			//escreve e envia o socket do evento 9 (Retirado empregado do balcão)
			sprintf(buffer, "%d 9\n", (int)time(0));
			send(sockfd, buffer, sizeof(buffer), 0);
		}
		pthread_mutex_unlock(&mutex_cliente); //############################################### UNLOCK ###############################################
	}
}

//Função que trata dos pedidos vindos do Monitor 
void *recebe_comandos_monitor(void *arg)
{
	struct sockaddr_un cli_addr;
	int n, id;

	int sockfd=*((int *) arg), clilen=sizeof(cli_addr);
	
	char buffer[2];
	
	//ciclo que fica à espera dos pedidos dos Monitor, para lhe dar resposta adequada
	while(1)
	{	
		if((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
		{
			if(n < 0) 
				perror("recv error");
		}
		buffer[n] = '\0';
		
		switch(atoi(buffer)) {
			case 1: //Começar
				corre = 1;
				break;
			case 4: //Sair
				corre = 0;
				sair = 1;
				exit(1);
			default:
				break;
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	if(argc == 3)
	{
		if(atoi(argv[1]) > 0)
		{
			printf("Insira o ficheiro de configuração seguindo do tempo de simulação.\n");
			return 0;
		}
		else if(atoi(argv[2]) < 1)
		{
			printf("O tempo de simulação tem que ser maior que 0.\n");
			return 0;
		}
		else
			configs.tempo_simulacao = atoi(argv[2]); //é recebido em horas
	}
	else
	{
		printf("Insiriu mal os argumentos.\n");
		return 0;
	}
	
	//leitura e armazenamento dos dados do ficheiro de configuração
	int *conf = ler_config(argv[1]);
	configs.tempo_medio_chegada_clientes 		= conf[0];
	configs.numero_produtos						= conf[1];
	configs.stock_inicial_produtos				= conf[2];
	configs.tempo_produto_um					= conf[3];
	configs.tempo_produto_dois					= conf[4];
	configs.max_clientes_fila 					= conf[5];
	configs.min_stock_produto					= conf[6];
	configs.prob_cliente_desistir			 	= conf[7];
	configs.tamanho_fila_desistir				= conf[8];
	configs.prob_cliente_alterar_produto 		= conf[9];
	configs.prob_cliente_ser_prio				= conf[10];
	configs.numero_max_empregados				= conf[11];
	configs.tempo_reposicao						= conf[12];
	configs.ronda_gerente						= conf[13];
	if(configs.numero_produtos%2 == 0)
		soma_1 = 0;
	else
		soma_1 = 1;
	
	//inicialização dos semáforos
	sem_init(&fila_normal, 0, 0);
	sem_init(&fila_prioritaria, 0, 0);
	initDadosEmpregados();
	//inicializa stock de produtos
	criarProdutos();
	
	//criação do socket e ligação
	if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		perror("Simulador: cant open socket stream");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, UNIXSTR_PATH);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
	if(connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
		perror("connect error");
			
	//criação da tarefa que irá tratar dos pedidos enviados pelo Monitor
	pthread_t thread;
	pthread_create(&thread, NULL, &recebe_comandos_monitor, &sockfd);
	
	while(!corre);
	
	//TEMPO
	int inicioSimulacao = (int)time(0);
	int fimSimulacao = (int)time(0) + (configs.tempo_simulacao * 60);
	char buffer[BUFFER_SIZE];

	//envia o socket com a informação do começo da simulação (evento 1)
	sprintf(buffer, "%d 1 1 %d %d\n", (int)time(0), configs.numero_produtos, configs.stock_inicial_produtos);
	send(sockfd, buffer, sizeof(buffer), 0);
	
	//gerente
	pthread_create(&thread, NULL, &tarefa_gerente, &sockfd);
	//primeiro empregado
	pthread_create(&thread, NULL, &tarefa_empregado, &sockfd);
	
	//Ciclo de simulação
	while((int)time(0) < fimSimulacao)
	{	
		//entra um cliente na loja
		pthread_create(&thread, NULL, &tarefa_cliente, &sockfd);
		int randChegaCliente = rand()%configs.tempo_medio_chegada_clientes; //número random entre 0 e tempo_medio_chegada_clientes-1
		usleep(randChegaCliente * UM_SEGUNDO);
	}
	
	//envia o socket com a informação de que a loja vai fechar (evento 1)
	sprintf(buffer, "%d 1 2\n", (int)time(0));
	send(sockfd, buffer, sizeof(buffer), 0);
	while(clientesLoja > 0);
	usleep(UM_SEGUNDO);
	
	sair = 1;
	//envia o socket com a informação do fim da simulação (evento 1)
	sprintf(buffer, "%d 1 0\n", (int)time(0));
	send(sockfd, buffer, sizeof(buffer), 0);
	
	while(corre);
	close(sockfd);
	
	return 0;
} 
