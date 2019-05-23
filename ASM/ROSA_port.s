 ;******************** (C) COPYRIGHT 2016 RUAL ********************
;* File Name          : ROSA_STM32_port.s
;* Author             : RUAL
;* Version            : V0.1beta
;* Date               : 11-02-2016
;* Description        : STM32 порт Русской событийной ОС (РОСА-с) 
;						STM32 port Russian Event OS	(ROSA-e)
;*                      Здесь выполняются специфичные для STM32:
;*                      - Запуск планировщика 
;*                      - Постановка обработчика в ожидание либо в кандидаты на запуск
;*                      - Сохранение контекста прерванных процессов
;*                      - Вызов обработчика
;*                      - Восстановление контекста прерванных процессов                      
;*******************************************************************************
; Свободно распространяемое программное обеспечение. Код можете использовать
; как угодно не вступая в противоречие с законом. Автор будет МОРАЛЬНО 
; признателен за упоминание ))) ПО поставляется "как есть".
; This software is supplied "AS IS" without warranties of any kind.
;*******************************************************************************
 AREA    xxx, DATA, READWRITE	
 
 EXPORT ROSA_CS_Flag
 	
ROSA_CS_Flag\
	DCB 1								;	флаг критической секции 
 
 AREA    xxx, DATA, READONLY 	
 
 EXPORT ROSA_Port_R
 EXPORT ROSA_Port_E
 	
ROSA_Port_R\
	DCB "STM32(v0.12beta)"		;	имя порта РОСА на МК 
ROSA_Port_E\
	EQU ROSA_Port_R
	
	
;/* вызов обработчика через SVC */
 AREA    |.text|, CODE, READONLY      
 
SVC_Handler PROC
	PRESERVE8
	;/// надо вствить проверку стека на переполнение !!! 
	
	EXPORT  SVC_Handler 
	
	IMPORT _Event_List
	IMPORT _CurRun_Event
	IMPORT _LastRun_Event
	IMPORT Event_Beginner
	IMPORT Event_Closer
	
	LDR		R1, =ROSA_CS_Flag		; проверим на критическую секцию	
	LDRB	R1, [R1]
	TST		R1, #1					; если да, 
	BXNE	LR						; то не будем менять процесс, выйдем из SVC	
 
	CPSID	I						; запретим прерывания 
	
	PUSH	{R4-R6,LR}				; сохраним регистры в стеке			
	
	LDR		R1, =0xE000ED24			; снимем бит активности SVC в ICSR
	LDR		R2,	=0xFFFFFF7F			; прекратим обработку SVC !!!!!!
	LDREX	R3,	[R1]
	AND 	R2, R3, R2
	STREX	R3, R2, [R1]				
	
__hendler_start		
	BL		Event_Beginner

	CMP		R0, #255				; выйдем из прерывание если 255
	BEQ		_ret_svc
	
	MOV		R4,	R0					; сохраним номер обработчика
	
	LDR		R1,	=_Event_List 		; получим адрес обработчика	
	LDR 	R0, [R1,R0,lsl #2]

	LDR		R1, =_LastRun_Event		; получим номер старого обработчика
	LDRB	R5,	[R1]
	
	CPSIE	I						; запустим обработчик 
	BLX		R0								
	CPSID	I
	
	MOV		R0,	R4					; восстановим номер обработчика
	MOV		R1,	R5					; восстановим номер старого обработчика

	BL		Event_Closer			; запустим закрывальщик
	
	CMP		R0,	#255				; если нужно 
	BNE		__hendler_start			; запустим следующий ожидающий обработчик 

_ret_svc							; пустой возврат		
	LDR		R1, =0xE000ED24			; восстановим признак прерывания SVC в ICSR
	LDREX	R2,	[R1]
	ORR 	R2, #0x80
	STREX	R3, R2, [R1] 		

	POP		{R4-R6,LR}				; восстановим регистры в стеке	
		
	CPSIE	I	
	
	BX		LR						; выходим как из прерывания		
	
	ALIGN	

	ENDP


;/* запрос обработчика */
ROSA_CallEvent PROC
	EXPORT ROSA_CallEvent	
	IMPORT Event_Planner
	
	CPSID	I
	
	PUSH	{R4,LR}		
	
	BL		Event_Planner	
		
	CMP		R0,	#255	

	LDR		R1, =0xE000ED24			;  вызовем обработку SVC !!!!!! pending SVC
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
	
	LDR		R1, =0xE000ED24			;  вызовем обработку SVC !!!!!! pending SVC
	LDR		R2,	=0x00008000		
	LDREX	R3,	[R1]				
	ORR		R2,	R3,R2
	STREX	R3, R2, [R1]  
	
	BX		LR
	ALIGN	

	ENDP	
				
				
				
	END		

				
                 