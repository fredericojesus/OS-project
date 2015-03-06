#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include "unix.h"

#define NAME_SIZE 100
#define BUFFER_SIZE 256
#define NUMBER_OF_CONFIGS 13

struct configuracao
{
	int tempo_simulacao;
	int tempo_medio_chegada_clientes;
	int numero_produtos;
	int stock_inicial_produtos;
	int tempo_produto_um;
	int tempo_produto_dois;
	int max_clientes_fila;
	int min_stock_produto;
	int prob_cliente_desistir;
	int tamanho_fila_desistir;
	int prob_cliente_alterar_produto;
	int prob_cliente_ser_prio;
	int numero_max_empregados;
	int tempo_reposicao;
	int ronda_gerente;
};

int* ler_config(char* file)
{
	int* a = (int*) malloc(sizeof(int) * NUMBER_OF_CONFIGS);
	FILE* fp = fopen (file, "rb");
	
	if(fp == NULL)
	{
		printf("Não e possível abrir o ficheiro de configuração.\n");
		abort();
	}
	
	int num, i = 0;
	char name[NAME_SIZE], buff[BUFFER_SIZE];
	
	while(fgets(buff, sizeof buff, fp) != NULL) 
	{
		if(sscanf(buff, "%[^=]=%d", name, &num) == 2)
			a[i++] = num;
	}
	
	fclose(fp);
	return a;
}
