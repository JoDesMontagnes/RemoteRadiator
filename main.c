#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//====================================================================
#ifndef HSE_VALUE
	#define HSE_VALUE 8000000
#endif

#define MAX_USART_BUFF 256
#define MAX_HOST_NAME_LENGTH 50
//====================================================================


typedef enum {FALSE, TRUE}BOOL;
typedef enum {HOST_TYPE_UNKNOWN, HOST_TYPE_PROBE, HOST_TYPE_DRIVER, HOST_TYPE_PC}HOST_TYPE;
typedef enum {HOST_CMD_VOID, HOST_CMD_NAME, HOST_CMD_TEMP}HOST_CMD;

typedef struct{
	BOOL full;
	char data[MAX_USART_BUFF];
	int nb_Cmd;
	int id_read;
	int id_write;
}Buff_t;

typedef struct{
	HOST_TYPE type;
	char name[MAX_HOST_NAME_LENGTH];
	HOST_CMD next_cmd;
}Host_t;

const char *hostCmdList[] = {"", "name", "temp"};

//====================================================================
//Systeme
void initSystem(void);
void initUSART1(void);
void initUSART2(void);
void initWifi(void);
	
void delay(volatile uint32_t ms);
void addCharToBuffer(Buff_t *buff, USART_TypeDef *usart);


void usartSendChar(USART_TypeDef *usart, char c);
void usartSendString(USART_TypeDef *usart, char *s);
void usartSendUint32(USART_TypeDef *usart, uint32_t data);
BOOL usartGetString(Buff_t* buff, char resp[MAX_USART_BUFF]);


//ESP8266
BOOL sendAtCmd(char *at);

//=====================================================================


 Buff_t _wifiBuff = {FALSE,"",0,0,0}, _consolBuff= {FALSE,"",0,0,0};
 //Utilisé pour faire des temporisations avec  Systick
 static volatile uint32_t TimingDelay;
 Host_t _hostList[5] = {
	{ HOST_TYPE_UNKNOWN, "", HOST_CMD_VOID},
	{ HOST_TYPE_UNKNOWN, "", HOST_CMD_VOID},
	{ HOST_TYPE_UNKNOWN, "", HOST_CMD_VOID},
	{ HOST_TYPE_UNKNOWN, "", HOST_CMD_VOID},
	{ HOST_TYPE_UNKNOWN, "", HOST_CMD_VOID}
 };

int main(void){
	char recep[MAX_USART_BUFF];
	int nbCmd = 0;
	initSystem();
	initUSART1();
	initUSART2();
	
	usartSendString(USART1, "Configuration du module wifi...");
	initWifi();
	usartSendString(USART1, "OK\r\n");
	
	while(1){
		
		if(usartGetString(&_consolBuff, recep) == TRUE){
			usartSendString(USART2, recep);
		}
		
		if(usartGetString(&_wifiBuff, recep) == TRUE){

			usartSendString(USART1, recep);
			if( (strncmp("0,CONNECT", recep, 9)) == 0){
				_hostList[0].next_cmd = HOST_CMD_NAME;
				nbCmd++;
			}else if( (strncmp("0,CONNECT", recep, 9)) == 0){
				sendAtCmd("AT+CIPSEND=0,1\r\n");
				sendAtCmd("name\r\n");
			}else if((strncmp("+IPD",recep,4)) == 0){
				
				int id = recep[5]-'0';
				char *cmd = &recep[5];
				while(*cmd != ':')
					cmd++;
				cmd++;
				
				
				if((strncmp(hostCmdList[HOST_CMD_TEMP],cmd,4))==0){
					int temp = atoi(&cmd[5]);
					usartSendString(USART1, "La temperature recu est : ");
					usartSendUint32(USART1, temp);
					usartSendString(USART1, "\r\n");
					
				}else if((strncmp(hostCmdList[HOST_CMD_NAME],cmd,4)) == 0){
					strcpy(_hostList[id].name,&cmd[5]);
					usartSendString(USART1, "Le peripherique s'appel : ");
					usartSendString(USART1, _hostList[id].name);
					_hostList[id].next_cmd = HOST_CMD_TEMP;
					nbCmd++;
				}
			}	
		}
		
		
		
		
		if(_consolBuff.full == TRUE){
			_consolBuff.nb_Cmd++;
		}
		
		if((nbCmd > 0) && (_wifiBuff.nb_Cmd == 0) && (_consolBuff.nb_Cmd == 0)){
			int i;
			for(i=0;i<4;i++){
				if(_hostList[i].next_cmd != HOST_CMD_VOID){
					usartSendString(USART2, "AT+CIPSEND=");
					usartSendUint32(USART2, i);
					usartSendChar(USART2, ',');
					usartSendUint32(USART2, strlen(hostCmdList[_hostList[i].next_cmd]));
					sendAtCmd("\r\n");
					delay(100);
					sendAtCmd((char *)hostCmdList[_hostList[i].next_cmd]);
					
					nbCmd--;
					_hostList[i].next_cmd = HOST_CMD_VOID;
				}
			}
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

void initWifi(){
	//Reset du module
	sendAtCmd("AT+RST\r\n");
	delay(2000);
	//Supprime l'echo
	sendAtCmd("ATE0\r\n");
	//Wifi en mode station
	sendAtCmd("AT+CWMODE_CUR=2\r\n");
	//Config du point d'acces
	sendAtCmd( "AT+CWSAP=\"ESP8266\",\"1234567890\",6,3,1,0\r\n");
	//Active le DHCP. Ne marche pas ?
	//sendAtCmd( "AT+CWDHCP_CUR=2,1\r\n");
	//multiple connexion
	sendAtCmd("AT+CIPMUX=1\r\n");
	//Creation du server TCP
	sendAtCmd( "AT+CIPSERVER=1,8452\r\n");
	//Demande l'Ip
	sendAtCmd( "AT+CIPAP_CUR?\r\n");
	sendAtCmd( "AT+CIPSTATUS\r\n");
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
	  if(buff->full == TRUE && buff->id_read != buff->id_write){
			buff->full = FALSE;
		}
		
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
			buff->data[buff->id_write] = '\0';
			//On force la lecture pour vider
			if(buff->nb_Cmd == 0)
				buff->nb_Cmd = 1;
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
