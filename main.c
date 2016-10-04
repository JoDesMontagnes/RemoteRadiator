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

#define MAX_USART_BUFF 100
//====================================================================
typedef enum {FALSE, TRUE}BOOL;

typedef struct CMD_T{
	int lenght;
	int id;
	BOOL cmd_available;
	char data[MAX_USART_BUFF];
  struct CMD_T *next;
	struct CMD_T *prev;
}Cmd_t;

typedef struct BUFF_T{
	int nb_Cmd;
	Cmd_t *first;
	Cmd_t *last;
}Buff_t;

//====================================================================
//Systeme
void initSystem(void);
void initApp(void);
void initUSART1(void);
void initUSART2(void);
void delay(volatile uint32_t ms);

//USART
Cmd_t* createCmdStruct(Cmd_t* prev);
void allocateUsartBuff(Cmd_t* tempCmd, Buff_t *usartBuff);
void taskUsart1Handler(void);
void taskUsart2Handler(void);
void freeUsartBuff(Buff_t *buff);
void usartSendChar(USART_TypeDef *usart, char c);
void usartSendString(USART_TypeDef *usart, char *s);
void usartSendUint32(USART_TypeDef *usart, uint32_t data);
char* usartGetString(Cmd_t* cmd, Buff_t *buff);


//ESP8266
BOOL sendAtCmd(char *at);

//=====================================================================


 static Buff_t _usart1Buff, _usart2Buff;
 Cmd_t *_usart1Cmd, *_usart2Cmd;
 static volatile uint32_t TimingDelay;

int main(void){
	char *recep;
	initSystem();
	initApp();
	initUSART1();
	initUSART2();
	
	
	usartSendString(USART1, "Configuration du module wifi:\r\n");
	
	usartSendString(USART2, "AT+CWMODE_CUR=2\r\n");
	delay(1000);
	usartSendString(USART2, "AT+CWSAP=\"ESP8266\",\"1234567890\",6,3,1,0\r\n");
	delay(1000);
	usartSendString(USART2, "AT+CWDHCP_CUR=3\r\n");
	delay(1000);
	usartSendString(USART2, "AT+CIPAP_CUR?\r\n");

	while(1){
		taskUsart1Handler();
		taskUsart2Handler();
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
	_usart1Buff.last = _usart1Buff.first = NULL;
	_usart1Cmd = createCmdStruct(NULL);

	
	_usart2Buff.nb_Cmd = 0;
	_usart2Buff.last = _usart2Buff.first = NULL;
	_usart2Cmd = createCmdStruct(NULL);
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

char* usartGetString(Cmd_t* cmd, Buff_t *buff){
	char *temp;
	while(buff->nb_Cmd <= 0){
		allocateUsartBuff(cmd,buff);
	}
	temp = malloc(sizeof( *buff->first->data)*strlen( buff->first->data));
	strcpy(temp, buff->first->data);
	freeUsartBuff(buff);	
	return(temp);
}

void USART1_IRQHandler(void){
	if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
		
		//Save char in the buffer
		_usart1Cmd->data[_usart1Cmd->id] = USART_ReceiveData(USART1);
		if(_usart1Cmd->data[_usart1Cmd->id] == '\n'){
			_usart1Cmd->data[_usart1Cmd->id+1] = '\0';
			_usart1Cmd->cmd_available = TRUE;
		}
		_usart1Cmd->id++;
		
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void USART2_IRQHandler(void){
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET){
		_usart2Cmd->data[_usart2Cmd->id] = USART_ReceiveData(USART2);
		if(_usart2Cmd->data[_usart2Cmd->id] == '\n'){
			_usart2Cmd->data[_usart2Cmd->id+1] = '\0';
			_usart2Cmd->cmd_available = TRUE;
		}
		_usart2Cmd->id++;
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
	
}

void SysTick_Handler(void){
	 if (TimingDelay != 0x00)
   {
		TimingDelay--;
   }else{
			SysTick->CTRL = 0;
		 usartSendChar(USART1, 'a');
	 }
}

void delay(volatile uint32_t ms){
	TimingDelay = ms;
	SysTick_Config(SystemCoreClock/1000);
	while(TimingDelay != 0);
}

	
Cmd_t* createCmdStruct(Cmd_t* prev){
	Cmd_t* temp = malloc(sizeof(Cmd_t));
	temp->id = 0;
	temp->cmd_available = FALSE;
	temp->lenght = 0;
	temp->next = NULL;
	temp->prev = prev;
	return(temp);
}

void allocateUsartBuff(Cmd_t* tempCmd, Buff_t *usartBuff){
		
		if(tempCmd->cmd_available == TRUE){
				Cmd_t *newCmd = createCmdStruct(NULL);
				memcpy(newCmd,tempCmd,sizeof(*tempCmd));
				if(usartBuff->first == NULL){
					usartBuff->first = usartBuff->last = newCmd;
				}else{
					usartBuff->last->next = newCmd;
					usartBuff->last->prev = usartBuff->last;
					usartBuff->last = newCmd;
				}
				tempCmd->cmd_available = FALSE;
				tempCmd->id = 0;
				tempCmd->next = tempCmd->prev = NULL;
				usartBuff->last->cmd_available = TRUE;
				usartBuff->nb_Cmd++;
		}
}

void freeUsartBuff(Buff_t *buff){
	if(buff->first->next != NULL){
	  buff->first = buff->first->next;
		free(buff->first->prev);
		buff->first->prev = NULL;			
	}else{
		free(buff->first);
		buff->first = NULL;
		buff->last = NULL;
	}
	buff->nb_Cmd--;
}

void taskUsart1Handler(void){
	while(_usart1Buff.nb_Cmd > 0){
			if(_usart1Buff.first->cmd_available == TRUE){
				
				//Traitement buffer
				usartSendString(USART2, _usart1Buff.first->data);
				
				
				//Libération mémoire
				freeUsartBuff(&_usart1Buff);
			}
	}
	allocateUsartBuff(_usart1Cmd, &_usart1Buff);
		
}

void taskUsart2Handler(void){
	while(_usart2Buff.nb_Cmd > 0){
			if(_usart2Buff.first->cmd_available == TRUE){
				
				//Traitement buffer
				usartSendString(USART1, _usart2Buff.first->data);
				
				
				freeUsartBuff(&_usart2Buff);
			}
		}
	allocateUsartBuff(_usart2Cmd, &_usart2Buff);
}


BOOL sendAtCmd(char *at){
	BOOL err = TRUE;
	char *rep;
	do{
		usartSendString(USART2, at);
		while((rep = usartGetString(_usart2Cmd, &_usart2Buff)) != NULL){
			if(strncpy(rep, "OK", 2))
				err = FALSE;
		}
	}while(err == TRUE);
}
