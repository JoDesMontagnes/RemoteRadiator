/*
Probleme : Reception de salut salut salu
alors qu'on ne fait que 2 getString et on envoit que deux "salut"

*/
#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//====================================================================
#ifndef HSE_VALUE
	#define HSE_VALUE 8000000
#endif

#define MAX_USART_BUFF 256
//====================================================================
typedef enum {FALSE, TRUE}BOOL;

typedef struct{
	BOOL full;
	char data[MAX_USART_BUFF];
	int nb_Cmd;
	int id_read;
	int id_write;
}Buff_t;


//====================================================================
//Systeme
void initSystem(void);
void initApp(void);
void initUSART1(void);
void initUSART2(void);
void delay(volatile uint32_t ms);
void addCharToBuffer(Buff_t *buff, USART_TypeDef *usart);


void usartSendChar(USART_TypeDef *usart, char c);
void usartSendString(USART_TypeDef *usart, char *s);
void usartSendUint32(USART_TypeDef *usart, uint32_t data);
BOOL usartGetString(Buff_t* buff, char resp[MAX_USART_BUFF]);


//ESP8266
BOOL sendAtCmd(char *at);

//=====================================================================


 Buff_t _wifiBuff, _consolBuff;
 static volatile uint32_t TimingDelay;

int main(void){
	char recep[MAX_USART_BUFF];
	initSystem();
	initApp();
	initUSART1();
	initUSART2();
	
	
	usartSendString(USART1, "Configuration du module wifi...");
	sendAtCmd("AT+RST\r\n");
	sendAtCmd("ATE0\r\n");
	sendAtCmd("AT+CWMODE_CUR=2\r\n");
	sendAtCmd( "AT+CWSAP=\"ESP8266\",\"1234567890\",6,3,1,0\r\n");
	sendAtCmd( "AT+CWDHCP_CUR=2,1\r\n");
	sendAtCmd( "AT+CIPAP_CUR?\r\n");
	
	usartSendString(USART1, "OK\r\n");
	
	while(1){
		if(usartGetString(&_consolBuff, recep) == TRUE)
			usartSendString(USART2, recep);
		
		if(usartGetString(&_wifiBuff, recep) == TRUE)
			usartSendString(USART1, recep);
		
		if(_consolBuff.full == TRUE){
			_consolBuff.nb_Cmd++;
		}
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
	_wifiBuff.full = FALSE;
	_wifiBuff.id_read = _wifiBuff.id_write = _wifiBuff.nb_Cmd = 0 ;

	
	_consolBuff.full = FALSE;
	_consolBuff.id_read = _consolBuff.id_write = _consolBuff.nb_Cmd = 0 ;
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


BOOL usartGetString(Buff_t *buff, char resp[MAX_USART_BUFF]){
	unsigned int size ;
	TimingDelay = 1000;
	SysTick_Config(SystemCoreClock/1000);
	while(buff->nb_Cmd <= 0){
		if( TimingDelay == 0){
			resp[0]= '\0'; //Retourne chaine vide
			return(FALSE);
		}
	}
	//Vérification retour au début
  size = strlen(&buff->data[buff->id_read]) + 1;
	if(size+buff->id_read == MAX_USART_BUFF){
		strncpy(resp, &buff->data[buff->id_read], size);
		buff->id_read = 0;
		size = strlen(&buff->data[buff->id_read]) + 1;
		strncat(resp,&buff->data[buff->id_read], size);
	}else{
		strncpy(resp, &buff->data[buff->id_read], size);
	}
	
	buff->nb_Cmd--;
	buff->id_read+=size;
	return(TRUE);
}

void USART1_IRQHandler(void){
	if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
		addCharToBuffer(&_consolBuff,USART1);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void USART2_IRQHandler(void){
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET){
		addCharToBuffer(&_wifiBuff,USART2);
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
	
}

void addCharToBuffer(Buff_t *buff, USART_TypeDef *usart){
	if(buff->id_write >= MAX_USART_BUFF-1){
			buff->id_write = 0;
		}
		
		if(buff->id_write >= buff->id_read || buff->id_write+2 < buff->id_read){
			buff->full = FALSE;
			buff->data[buff->id_write] = USART_ReceiveData(usart);
			if(buff->data[buff->id_write] == '\n'){
				buff->id_write++;
				buff->data[buff->id_write] = '\0';
				buff->nb_Cmd++;
			}
			buff->id_write++;
		}else{
			buff->full = TRUE;
		}
}

void SysTick_Handler(void){
	if (TimingDelay != 0x00){
		TimingDelay--;
		
  }else{
		SysTick->CTRL = 0;
	}
}

void delay(volatile uint32_t ms){
	TimingDelay = ms;
	SysTick_Config(SystemCoreClock/1000);
	while(TimingDelay != 0);
}

BOOL sendAtCmd(char *at){
	uint8_t cpt = 3;
	char rep[MAX_USART_BUFF];
	do{
		usartSendString(USART1, at);
		usartSendString(USART1, " : ");
		usartSendString(USART2, at);
		while(usartGetString(&_wifiBuff,rep) == TRUE){
			usartSendString(USART1, rep);
			if(strncmp(rep, "OK", 2) == 0){
				return(TRUE);				
			}else if (strncmp(rep, "ERROR", 2) == 0){
				delay(10);
				sendAtCmd("AT\r\n");
			  break;
			}
		}
		cpt--;
	}while(cpt > 0);
	return(FALSE);				
}
