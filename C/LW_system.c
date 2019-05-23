#include "lw_system.h"
#include "hw_config.h"		

static void EndRX(void);

/* обработка перрывания для формирования пакета */
static uint8_t PackByte[3];
static uint8_t PackSts = 0;
static volatile uint32_t LW_timer;
static uint16_t PulseCnt = 0;
static uint16_t RXTimeCount = 0; 	// счетчик времени приема 
/* обработчик  передачи */
void LW_system_TX(void)
{
	uint8_t nPackBit,nPackByte;
	
	if (RXTimeCount++ > GetIRPlsPeruS(2600)) // вышло время приема пакета
	{
		RXTimeCount = 0;
		EndRX();
	}
	
	LW_timer++;  		// обработчик задержек	

	if (PulseCnt--) return; // время не вышло
	
 	if (PackSts == 0) 	// состояние останова
	{	
		Pulse_OFF();
		PulseCnt =GetIRPlsPeruS(2400);
		return;
	}		

	if (PackSts == 0x01) 	// стартовый импульс
	{
		Pulse_ON();
		PulseCnt= GetIRPlsPeruS(2400);		
	}		
	else if (PackSts&0x01) // байтовый импульс
	{ 
		Pulse_ON();
		
		nPackBit	= (PackSts/2)-1; 	// определяем номер бита в пакете
		nPackByte	= nPackBit/8; 		// определяем номер байта в пакете
		nPackBit	= nPackBit%8; 		// определяем номер бита в байте
		
		if((PackByte[nPackByte]<<nPackBit)&0x80) 
			PulseCnt = GetIRPlsPeruS(1200); // лог 1
		else PulseCnt = GetIRPlsPeruS(600);	// лог 0
	} 
	else // пауза
	{
		Pulse_OFF(); 
		PulseCnt = GetIRPlsPeruS(600);
	}


	if(PackSts++>MAX_PACK_BITS) // последний интервал пакета
	{
		PackSts = 0;
		Pulse_OFF();
	}
}

/* обработчик  приема  */
const uint16_t IR_MAXPULSE                = 2600;                 // Sony (Miles) header is 2400us, 1= 1200us, 0= 600us
const uint16_t IR_MAXPULSEGAP             = 750;                  // this is the timeout for the gaps between the bits and at the end, needs to be 600+tolerance
static uint8_t RX_BitCount = 0;
static uint8_t Shift = 0x80;
static uint8_t IR_Rx_buf[8];
static uint8_t temp_data;

void LW_system_RX(uint8_t RxBit)
{
	uint16_t tick = RXTimeCount;
	RXTimeCount = 0;
	
	if (RxBit) { // импульс
		
		if (tick > GetIRPlsPeruS(2200)){  // стартовый импульс
			Shift = 0x80;	
			temp_data = 0;
		}	
		
		else if(tick > GetIRPlsPeruS(1000)) {
			temp_data |= Shift; // лог. "1"
			Shift >>= 1; 			// СДВИНЕМ следующую позицию бита
			RX_BitCount += 1;	// увеличим счетчик битов		
		}
		
		else if(tick < GetIRPlsPeruS(200))  // лог. "0"
			EndRX(); // слишком короткий импульс - ошибка, идем к началу			
		
		else {	
			Shift >>= 1; 			// СДВИНЕМ следующую позицию бита
			RX_BitCount += 1;	// увеличим счетчик битов		
		}
		
	}	else { // пауза 
		
		if (tick > GetIRPlsPeruS(1000))  EndRX(); // длиннная пауза, завершаем прием пакета					
		
		else if (tick < GetIRPlsPeruS(200)) EndRX(); // короткая пауза, завершаем прием пакета
		
	}		
	
	if (Shift == 0) { // принят полный байт
		IR_Rx_buf[(RX_BitCount - 1) / 8] = temp_data;
		Shift = 0x80;
		temp_data = 0;
	}
	
	if (RXTimeCount > MAX_PACK_BITS) EndRX(); // переполнение буфера приема, завершаем прием пакета
}

/* завершение приема */
static void EndRX(void) 
{
	if (RX_BitCount%8) IR_Rx_buf[RX_BitCount/8] = temp_data;  // выгрузим последний байт в буфер
	if (RX_BitCount > 6) 
		LW_RX_Collbeck(IR_Rx_buf,RX_BitCount);
	RX_BitCount = 0;  
	Shift = 0x80;	
	temp_data = 0;
	return;	
}


/* подготовка пакета */
uint8_t LWSendIRPack(uint8_t byte1,uint8_t byte2,uint8_t byte3)
{
	if (PackSts) return PackSts; // проверим завершение передачи пакета

	PackByte[0] = byte1;
	PackByte[1] = byte2;
	PackByte[2] = byte3;
	PackSts = 1;
	PulseCnt= 1; 
	Pulse_ON();
	return 0;
}

/* получение миллисек */
uint32_t LW_millis(void)
{
	return LW_timer;
}
	
/* задежка с таймером LASERWAR в миллисек */
void LW_delay(uint32_t del)
{
	uint32_t t = LW_millis();
	while(LW_millis()-t < del*IR_freq/1000);
}

void LW_Init(void)
{	
	LW_GPIO_Init();
	LW_TIM_Init();
}

/* уход в сон */
void LW_sleep(void)
{
	/* засыпание */
	LW_delay(6000);
	/* пробуждение */
	SystemInit();		// восстановление такта
	LW_Init();			// настройка устройств LW
}
	
	
	




