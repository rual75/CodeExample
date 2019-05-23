#include "lw_system.h"
#include "hw_config.h"		

static void EndRX(void);

/* ��������� ���������� ��� ������������ ������ */
static uint8_t PackByte[3];
static uint8_t PackSts = 0;
static volatile uint32_t LW_timer;
static uint16_t PulseCnt = 0;
static uint16_t RXTimeCount = 0; 	// ������� ������� ������ 
/* ����������  �������� */
void LW_system_TX(void)
{
	uint8_t nPackBit,nPackByte;
	
	if (RXTimeCount++ > GetIRPlsPeruS(2600)) // ����� ����� ������ ������
	{
		RXTimeCount = 0;
		EndRX();
	}
	
	LW_timer++;  		// ���������� ��������	

	if (PulseCnt--) return; // ����� �� �����
	
 	if (PackSts == 0) 	// ��������� ��������
	{	
		Pulse_OFF();
		PulseCnt =GetIRPlsPeruS(2400);
		return;
	}		

	if (PackSts == 0x01) 	// ��������� �������
	{
		Pulse_ON();
		PulseCnt= GetIRPlsPeruS(2400);		
	}		
	else if (PackSts&0x01) // �������� �������
	{ 
		Pulse_ON();
		
		nPackBit	= (PackSts/2)-1; 	// ���������� ����� ���� � ������
		nPackByte	= nPackBit/8; 		// ���������� ����� ����� � ������
		nPackBit	= nPackBit%8; 		// ���������� ����� ���� � �����
		
		if((PackByte[nPackByte]<<nPackBit)&0x80) 
			PulseCnt = GetIRPlsPeruS(1200); // ��� 1
		else PulseCnt = GetIRPlsPeruS(600);	// ��� 0
	} 
	else // �����
	{
		Pulse_OFF(); 
		PulseCnt = GetIRPlsPeruS(600);
	}


	if(PackSts++>MAX_PACK_BITS) // ��������� �������� ������
	{
		PackSts = 0;
		Pulse_OFF();
	}
}

/* ����������  ������  */
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
	
	if (RxBit) { // �������
		
		if (tick > GetIRPlsPeruS(2200)){  // ��������� �������
			Shift = 0x80;	
			temp_data = 0;
		}	
		
		else if(tick > GetIRPlsPeruS(1000)) {
			temp_data |= Shift; // ���. "1"
			Shift >>= 1; 			// ������� ��������� ������� ����
			RX_BitCount += 1;	// �������� ������� �����		
		}
		
		else if(tick < GetIRPlsPeruS(200))  // ���. "0"
			EndRX(); // ������� �������� ������� - ������, ���� � ������			
		
		else {	
			Shift >>= 1; 			// ������� ��������� ������� ����
			RX_BitCount += 1;	// �������� ������� �����		
		}
		
	}	else { // ����� 
		
		if (tick > GetIRPlsPeruS(1000))  EndRX(); // �������� �����, ��������� ����� ������					
		
		else if (tick < GetIRPlsPeruS(200)) EndRX(); // �������� �����, ��������� ����� ������
		
	}		
	
	if (Shift == 0) { // ������ ������ ����
		IR_Rx_buf[(RX_BitCount - 1) / 8] = temp_data;
		Shift = 0x80;
		temp_data = 0;
	}
	
	if (RXTimeCount > MAX_PACK_BITS) EndRX(); // ������������ ������ ������, ��������� ����� ������
}

/* ���������� ������ */
static void EndRX(void) 
{
	if (RX_BitCount%8) IR_Rx_buf[RX_BitCount/8] = temp_data;  // �������� ��������� ���� � �����
	if (RX_BitCount > 6) 
		LW_RX_Collbeck(IR_Rx_buf,RX_BitCount);
	RX_BitCount = 0;  
	Shift = 0x80;	
	temp_data = 0;
	return;	
}


/* ���������� ������ */
uint8_t LWSendIRPack(uint8_t byte1,uint8_t byte2,uint8_t byte3)
{
	if (PackSts) return PackSts; // �������� ���������� �������� ������

	PackByte[0] = byte1;
	PackByte[1] = byte2;
	PackByte[2] = byte3;
	PackSts = 1;
	PulseCnt= 1; 
	Pulse_ON();
	return 0;
}

/* ��������� �������� */
uint32_t LW_millis(void)
{
	return LW_timer;
}
	
/* ������� � �������� LASERWAR � �������� */
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

/* ���� � ��� */
void LW_sleep(void)
{
	/* ��������� */
	LW_delay(6000);
	/* ����������� */
	SystemInit();		// �������������� �����
	LW_Init();			// ��������� ��������� LW
}
	
	
	




