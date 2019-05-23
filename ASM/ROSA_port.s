 ;******************** (C) COPYRIGHT 2016 RUAL ********************
;* File Name          : ROSA_STM32_port.s
;* Author             : RUAL
;* Version            : V0.1beta
;* Date               : 11-02-2016
;* Description        : STM32 ���� ������� ���������� �� (����-�) 
;						STM32 port Russian Event OS	(ROSA-e)
;*                      ����� ����������� ����������� ��� STM32:
;*                      - ������ ������������ 
;*                      - ���������� ����������� � �������� ���� � ��������� �� ������
;*                      - ���������� ��������� ���������� ���������
;*                      - ����� �����������
;*                      - �������������� ��������� ���������� ���������                      
;*******************************************************************************
; �������� ���������������� ����������� �����������. ��� ������ ������������
; ��� ������ �� ������� � ������������ � �������. ����� ����� �������� 
; ����������� �� ���������� ))) �� ������������ "��� ����".
; This software is supplied "AS IS" without warranties of any kind.
;*******************************************************************************
 AREA    xxx, DATA, READWRITE	
 
 EXPORT ROSA_CS_Flag
 	
ROSA_CS_Flag\
	DCB 1								;	���� ����������� ������ 
 
 AREA    xxx, DATA, READONLY 	
 
 EXPORT ROSA_Port_R
 EXPORT ROSA_Port_E
 	
ROSA_Port_R\
	DCB "STM32(v0.12beta)"		;	��� ����� ���� �� �� 
ROSA_Port_E\
	EQU ROSA_Port_R
	
	
;/* ����� ����������� ����� SVC */
 AREA    |.text|, CODE, READONLY      
 
SVC_Handler PROC
	PRESERVE8
	;/// ���� ������� �������� ����� �� ������������ !!! 
	
	EXPORT  SVC_Handler 
	
	IMPORT _Event_List
	IMPORT _CurRun_Event
	IMPORT _LastRun_Event
	IMPORT Event_Beginner
	IMPORT Event_Closer
	
	LDR		R1, =ROSA_CS_Flag		; �������� �� ����������� ������	
	LDRB	R1, [R1]
	TST		R1, #1					; ���� ��, 
	BXNE	LR						; �� �� ����� ������ �������, ������ �� SVC	
 
	CPSID	I						; �������� ���������� 
	
	PUSH	{R4-R6,LR}				; �������� �������� � �����			
	
	LDR		R1, =0xE000ED24			; ������ ��� ���������� SVC � ICSR
	LDR		R2,	=0xFFFFFF7F			; ��������� ��������� SVC !!!!!!
	LDREX	R3,	[R1]
	AND 	R2, R3, R2
	STREX	R3, R2, [R1]				
	
__hendler_start		
	BL		Event_Beginner

	CMP		R0, #255				; ������ �� ���������� ���� 255
	BEQ		_ret_svc
	
	MOV		R4,	R0					; �������� ����� �����������
	
	LDR		R1,	=_Event_List 		; ������� ����� �����������	
	LDR 	R0, [R1,R0,lsl #2]

	LDR		R1, =_LastRun_Event		; ������� ����� ������� �����������
	LDRB	R5,	[R1]
	
	CPSIE	I						; �������� ���������� 
	BLX		R0								
	CPSID	I
	
	MOV		R0,	R4					; ����������� ����� �����������
	MOV		R1,	R5					; ����������� ����� ������� �����������

	BL		Event_Closer			; �������� ������������
	
	CMP		R0,	#255				; ���� ����� 
	BNE		__hendler_start			; �������� ��������� ��������� ���������� 

_ret_svc							; ������ �������		
	LDR		R1, =0xE000ED24			; ����������� ������� ���������� SVC � ICSR
	LDREX	R2,	[R1]
	ORR 	R2, #0x80
	STREX	R3, R2, [R1] 		

	POP		{R4-R6,LR}				; ����������� �������� � �����	
		
	CPSIE	I	
	
	BX		LR						; ������� ��� �� ����������		
	
	ALIGN	

	ENDP


;/* ������ ����������� */
ROSA_CallEvent PROC
	EXPORT ROSA_CallEvent	
	IMPORT Event_Planner
	
	CPSID	I
	
	PUSH	{R4,LR}		
	
	BL		Event_Planner	
		
	CMP		R0,	#255	

	LDR		R1, =0xE000ED24			;  ������� ��������� SVC !!!!!! pending SVC
	LDR		R2,	=0x00008000		
	LDREX	R3,	[R1]				
	ORR		R2,	R3,R2
	STREXNE	R3, R2, [R1]  
	CLREXEQ

	POP		{R4,LR}	
	CPSIE	I

	BX		LR
	ALIGN	

	ENDP		

_PendSVC PROC
	EXPORT _PendSVC
	
	LDR		R1, =0xE000ED24			;  ������� ��������� SVC !!!!!! pending SVC
	LDR		R2,	=0x00008000		
	LDREX	R3,	[R1]				
	ORR		R2,	R3,R2
	STREX	R3, R2, [R1]  
	
	BX		LR
	ALIGN	

	ENDP	
				
				
				
	END		

				
                 