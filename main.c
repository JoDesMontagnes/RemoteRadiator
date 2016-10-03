/*
Description : Exemple d'un buffer tournant sous STM32.
							Le programme commence par initialiser la clock et les périphs essentiels
							Ensuite on initialise les variables sytemes
							Puis on configure l'USART1 relier à un PC avec les Interruptions sur RX.
	Le programme montre une assez bonne robustesse. Par contre L'ajout d'une tempo trop longue
*/



#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//====================================================================
#if !defined HSE_VALUE
	#define HSE_VALUE 8000000
#endif

#define MAX_USART_BUFF 255
//====================================================================
typedef enum {FALSE, TRUE}BOOL;

typedef struct{
	int lenght;
	int id;
	char *data;
  struct Cmd_t *next;
	struct Cmd_t *prev;
}Cmd_t;

typedef struct{
	int nb_Cmd;
	Cmd_t *first;
	Cmd_t *last;
}Buff_t;

//====================================================================
void initSystem(void);
void initApp(void);
void initUSART1(void);
void initUSART2(void);

Cmd_t* createCmdStruct(Cmd_t* prev);

void usartSendChar(USART_TypeDef *usart, char c);
void usartSendString(USART_TypeDef *usart, char *s);
void usartSendUint32(USART_TypeDef *usart, uint32_t data);
char* usartGetString(Buff_t *buff);
void clearBuffer(Buff_t *buff,unsigned char size);

//=====================================================================


static Buff_t _usart1Buff;
static Buff_t _usart2Buff;

int main(void){
	int i;
	char* rep;
	RCC_ClocksTypeDef clk;
	
	initSystem();
	initApp();
	initUSART1();
	initUSART2();
	
	clearBuffer(&_usart1Buff, MAX_USART_BUFF);
	clearBuffer(&_usart2Buff, MAX_USART_BUFF);
	
	usartSendString(USART1, "Init ... done\r\n");
	RCC_GetClocksFreq(&clk);
	usartSendString(USART1, "System frequency : ");
	usartSendUint32(USART1, clk.SYSCLK_Frequency);
	usartSendString(USART1, "\r\n");
	
	
	usartSendString(USART1, "Configuration du module wifi:\r\n");
	usartSendString(USART2, "AT+CWMODE_CUR=1\r\n");
	usartSendString(USART2, "AT+CWJAP_CUR=\"SFR-d178\", \"DELAE5LW7U4A\"\r\n");
	usartSendString(USART2, "AT+CWDHCP_CUR=3\r\n");
	usartSendString(USART2, "AT+CIPAP_CUR?\r\n");
	
	clearBuffer(&_usart1Buff, MAX_USART_BUFF);
	clearBuffer(&_usart2Buff, MAX_USART_BUFF);
	
	while(1){
		
	}
	
	
	return(0);
}


void initSystem(void){
	//Activation de la clock sur le gestionnaire de flash et de la ram
	//Normalement fait au reset mais on le fait dans le code pour être sure.
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FLITF | RCC_AHBPeriph_SRAM, ENABLE);

	//On utilise le quartz de 8Mhz associé à la PLL avec un multiplieur de 9, soit 72Mhz
	RCC_HSEConfig(RCC_HSE_ON); //On active le mode quartz externe haute vitesse
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9); //Freq = 8*9=72Mhz
	RCC_PLLCmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	//On utilise la fréquence max soit 72Mhz pour les périphériques
	RCC_HCLKConfig(RCC_HCLK_Div1);
  RCC_PCLK2Config (RCC_HCLK_Div1) ; 	
	RCC_PCLK1Config(RCC_HCLK_Div1);
}

void initApp(void){
	_usart1Buff.nb_Cmd = 0;
	_usart1Buff.first = NULL;
	_usart1Buff.last = NULL;

	
	_usart2Buff.nb_Cmd = 0;
	_usart2Buff.first = NULL;
	_usart2Buff.last = NULL;
}

void initUSART1(void){
	USART_InitTypeDef usart1InitStruct;
	GPIO_InitTypeDef gpioaInitStruct;
	
	//On active la clock sur le périphérique
	//A faire avant la config
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	
	//Configuration de Tx
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_9;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);
	
	//Configuration de RX
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_10;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);

	USART_Cmd(USART1, ENABLE);
	//Config
	usart1InitStruct.USART_BaudRate = 115200;
	usart1InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart1InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart1InitStruct.USART_Parity = USART_Parity_No;
	usart1InitStruct.USART_StopBits = USART_StopBits_1;
	usart1InitStruct.USART_WordLength = USART_WordLength_8b;

	
	USART_Init(USART1, &usart1InitStruct);
	
	//Autorisation des Interruptions
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART1_IRQn);
}

void initUSART2(void){
	USART_InitTypeDef usart2InitStruct;
	GPIO_InitTypeDef gpioaInitStruct;
	
	//On active la clock sur le périphérique
	//A faire avant la config
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	//Configuration de Tx
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_2;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);
	
	//Configuration de RX
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_3;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);

	USART_Cmd(USART2, ENABLE);
	//Config
	usart2InitStruct.USART_BaudRate = 115200;
	usart2InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart2InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart2InitStruct.USART_Parity = USART_Parity_No;
	usart2InitStruct.USART_StopBits = USART_StopBits_1;
	usart2InitStruct.USART_WordLength = USART_WordLength_8b;

	
	USART_Init(USART2, &usart2InitStruct);
	
	//Autorisation des Interruptions
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART2_IRQn);
}


void usartSendChar(USART_TypeDef *usart, char c){
		while(USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET);
		USART_SendData(usart, c);
	
}

void usartSendString(USART_TypeDef *usart, char *s){
	while(*s != '\0'){
		usartSendChar(usart, *s++);
	}
}

void  usartSendUint32(USART_TypeDef *usart, uint32_t data){
	char buffer[11];
	sprintf(buffer, "%zu",data);
	usartSendString(usart, buffer);
}

char* usartGetString(Buff_t *buff){
	
}

void USART1_IRQHandler(void){
	if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
		if(_usart1Buff.nb_Cmd == 0){
			_usart1Buff.first = _usart1Buff.last = createCmdStruct(NULL);
		}
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void USART2_IRQHandler(void){
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET){
			
		
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}


Cmd_t* createCmdStruct(Cmd_t* prev){
	Cmd_t* temp = malloc(sizeof(Cmd_t));
	temp->id = 0;
	temp->lenght = 0;
	temp->data = malloc(sizeof(temp->data)*MAX_USART_BUFF);
	temp->next = NULL;
	temp->prev = (struct Cmd_t*)prev;
}
