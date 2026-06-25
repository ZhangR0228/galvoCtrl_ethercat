/*
* This source file is part of the EtherCAT Slave Stack Code licensed by Beckhoff Automation GmbH & Co KG, 33415 Verl, Germany.
* The corresponding license agreement applies. This hint shall not be removed.
*/

/**
\addtogroup EL9800_HW EL9800 Platform (Serial ESC Access)
@{
*/

/**
\file    el9800hw.c
\author EthercatSSC@beckhoff.com
\brief Implementation
Hardware access implementation for EL9800 onboard PIC18/PIC24 connected via SPI to ESC

\version 5.12

<br>Changes to version V5.11:<br>
V5.12 EL9800 1: improve the SPI access<br>
<br>Changes to version V5.10:<br>
V5.11 ECAT10: change PROTO handling to prevent compiler errors<br>
V5.11 EL9800 2: change PDI access test to 32Bit ESC access and reset AL Event mask after test even if AL Event is not enabled<br>
<br>Changes to version V5.01:<br>
V5.10 ESC5: Add missing swapping<br>
V5.10 HW3: Sync1 Isr added<br>
V5.10 HW4: Add volatile directive for direct ESC DWORD/WORD/BYTE access<br>
           Add missing swapping in mcihw.c<br>
           Add "volatile" directive vor dummy variables in enable and disable SyncManger functions<br>
           Add missing swapping in EL9800hw files<br>
<br>Changes to version V5.0:<br>
V5.01 HW1: Invalid ESC access function was used<br>
<br>Changes to version V4.40:<br>
V5.0 ESC4: Save SM disable/Enable. Operation may be pending due to frame handling.<br>
<br>Changes to version V4.30:<br>
V4.40 : File renamed from spihw.c to el9800hw.c<br>
<br>Changes to version V4.20:<br>
V4.30 ESM: if mailbox Syncmanger is disabled and bMbxRunning is true the SyncManger settings need to be revalidate<br>
V4.30 EL9800: EL9800_x hardware initialization is moved to el9800.c<br>
V4.30 SYNC: change synchronisation control function. Add usage of 0x1C32:12 [SM missed counter].<br>
Calculate bus cycle time (0x1C32:02 ; 0x1C33:02) CalcSMCycleTime()<br>
V4.30 PDO: rename PDO specific functions (COE_xxMapping -> PDO_xxMapping and COE_Application -> ECAT_Application)<br>
V4.30 ESC: change requested address in GetInterruptRegister() to prevent acknowledge events.<br>
(e.g. reading an SM config register acknowledge SM change event)<br>
GENERIC: renamed several variables to identify used SPI if multiple interfaces are available<br>
V4.20 MBX 1: Add Mailbox queue support<br>
V4.20 SPI 1: include SPI RxBuffer dummy read<br>
V4.20 DC 1: Add Sync0 Handling<br>
V4.20 PIC24: Add EL9800_4 (PIC24) required source code<br>
V4.08 ECAT 3: The AlStatusCode is changed as parameter of the function AL_ControlInd<br>
<br>Changes to version V4.02:<br>
V4.03 SPI 1: In ISR_GetInterruptRegister the NOP-command should be used.<br>
<br>Changes to version V4.01:<br>
V4.02 SPI 1: In HW_OutputMapping the variable u16OldTimer shall not be set,<br>
             otherwise the watchdog might exceed too early.<br>
<br>Changes to version V4.00:<br>
V4.01 SPI 1: DI and DO were changed (DI is now an input for the uC, DO is now an output for the uC)<br>
V4.01 SPI 2: The SPI has to operate with Late-Sample = FALSE on the Eva-Board<br>
<br>Changes to version V3.20:<br>
V4.00 ECAT 1: The handling of the Sync Manager Parameter was included according to<br>
              the EtherCAT Guidelines and Protocol Enhancements Specification<br>
V4.00 APPL 1: The watchdog checking should be done by a microcontroller<br>
                 timer because the watchdog trigger of the ESC will be reset too<br>
                 if only a part of the sync manager data is written<br>
V4.00 APPL 4: The EEPROM access through the ESC is added

*/


/*--------------------------------------------------------------------------------------
------
------    Includes
------
--------------------------------------------------------------------------------------*/
#include "ecat_def.h"
#include "ecatslv.h"

#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"
#include "SPIDriver.h"
#include "my_bsp_ec.h"




#define    _EL9800HW_ 1
#include "el9800hw.h"
#undef    _EL9800HW_
/*remove definition of _EL9800HW_ (#ifdef is used in el9800hw.h)*/

#include "ecatappl.h"




/*--------------------------------------------------------------------------------------
------
------    internal Types and Defines
------
--------------------------------------------------------------------------------------*/

typedef union
{
    unsigned short    Word;
    unsigned char    Byte[2];
} UBYTETOWORD;

typedef union 
{
    UINT8           Byte[2];
    UINT16          Word;
}
UALEVENT;

BOOL bEscInterrupt = 0;
BOOL bSync0Interrupt = 0;
BOOL bSync1Interrupt = 0;
BOOL bTimer5Interrupt = 0;

#define DESELECT_SPI         HAL_GPIO_WritePin(FLASH_SPI_CS_PORT, FLASH_SPI_CS_PIN, GPIO_PIN_SET)
#define SELECT_SPI           HAL_GPIO_WritePin(FLASH_SPI_CS_PORT, FLASH_SPI_CS_PIN, GPIO_PIN_RESET)

/*-----------------------------------------------------------------------------------------
------
------    SPI defines/macros
------
-----------------------------------------------------------------------------------------*/
//#define SPI1_SEL                        _LATB2
//#define SPI1_IF                            _SPI1IF
//#define SPI1_BUF                        SPI1BUF
//#define SPI1_CON1                        SPI1CON1
//#define SPI1_STAT                        SPI1STAT
//#define    WAIT_SPI_IF                        while( !SPI1_IF)
//#define    SELECT_SPI                        {(SPI1_SEL) = (SPI_ACTIVE);}
//#define    DESELECT_SPI                    {(SPI1_SEL) = (SPI_DEACTIVE);}
//#define    INIT_SSPIF                        {(SPI1_IF)=0;}
//#define SPI1_STAT_VALUE                    0x8000
//#define SPI1_CON1_VALUE                    0x027E
//#define SPI1_CON1_VALUE_16BIT            0x047E

#define SPI_DEACTIVE                     1
#define SPI_ACTIVE                        0



/*-----------------------------------------------------------------------------------------
------
------    Global Interrupt setting
------
-----------------------------------------------------------------------------------------*/

#define 		DISABLE_GLOBAL_INT           __disable_irq() //set CPU priority to level 4 (disable interrupt level 1 - 4)
#define 		ENABLE_GLOBAL_INT            __enable_irq()	

#define     DISABLE_AL_EVENT_INT        	DISABLE_GLOBAL_INT
#define     ENABLE_AL_EVENT_INT            ENABLE_GLOBAL_INT


/*-----------------------------------------------------------------------------------------
------
------    ESC Interrupt
------
-----------------------------------------------------------------------------------------*/


#define    INIT_ESC_INT           INIT_ISR_EXTI3();	//PD3		
#define    EcatIsr                EXTI3_IRQHandler
#define    ACK_ESC_INT         		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);



/*-----------------------------------------------------------------------------------------
------
------    SYNC0 Interrupt
------
-----------------------------------------------------------------------------------------*/
#define    INIT_SYNC0_INT                  INIT_SYNC0_EXTI4();	//PD4
#define    DISABLE_SYNC0_INT               NVIC_DisableIRQ(EXTI4_IRQn);	  // {(_INT3IE)=0;}//disable interrupt source INT3
#define    ENABLE_SYNC0_INT                NVIC_EnableIRQ(EXTI4_IRQn);	// {(_INT3IE) = 1;} //enable interrupt source INT3
#define    ACK_SYNC0_INT                   __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);


#define    INIT_SYNC1_INT                  INIT_SYNC1_EXTI7();//PD13
#define    DISABLE_SYNC1_INT               NVIC_DisableIRQ(EXTI9_5_IRQn);// {(_INT4IE)=0;}//disable interrupt source INT4
#define    ENABLE_SYNC1_INT                NVIC_EnableIRQ(EXTI9_5_IRQn); //{(_INT4IE) = 1;} //enable interrupt source INT4
#define    ACK_SYNC1_INT                   __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_7);


/*-----------------------------------------------------------------------------------------
------
------    Hardware timer
------
-----------------------------------------------------------------------------------------*/


#define 	 ECAT_TIMER_ACK_INT        		__HAL_TIM_CLEAR_IT(&htimx , TIM_IT_UPDATE);	
#define    ESC_TIME_ISR                    	TIM2_IRQHandler //	SysTick_Handler//						
#define    ENABLE_ECAT_TIMER_INT        NVIC_EnableIRQ(TIM2_IRQn) ;	//SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;//NVIC_EnableIRQ(TIM2_IRQn) ;	
#define    DISABLE_ECAT_TIMER_INT       NVIC_DisableIRQ(TIM2_IRQn) ;//SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;//NVIC_DisableIRQ(SysTick_IRQn/*TIM2_IRQn*/) ;

#define 	 INIT_ECAT_TIMER              INIT_ESC_TIME();  // SysTick_Config(SystemCoreClock/1000);  
#define 	 STOP_ECAT_TIMER              DISABLE_ECAT_TIMER_INT;/*disable timer interrupt*/ \

#define 	 START_ECAT_TIMER             ENABLE_ECAT_TIMER_INT


/*-----------------------------------------------------------------------------------------
------
------    Configuration Bits
------
-----------------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------------
------
------    LED defines
------
-----------------------------------------------------------------------------------------*/
// EtherCAT Status LEDs -> StateMachine
#define LED_ECATGREEN                  LATFbits.LATF1
#define LED_ECATRED                    LATFbits.LATF0

/*--------------------------------------------------------------------------------------
------
------    internal Variables
------
--------------------------------------------------------------------------------------*/
UALEVENT         EscALEvent;            //contains the content of the ALEvent register (0x220), this variable is updated on each Access to the Esc
/*--------------------------------------------------------------------------------------
------
------    internal functions
------
--------------------------------------------------------------------------------------*/

/*ECATCHANGE_START(V5.12) EL9800 1*/
//static UINT8 RxTxSpiData(UINT8 MosiByte)
//{
//    VARVOLATILE UINT8 MisoByte = 0;
//    
//    if((SPI1_STAT & 0x1) != 0)
//    {
//        /*read buffer to prevent buffer overrun error*/
//        MisoByte = SPI1_BUF;
//    }
//    
//    
//    SPI1_IF = 0;

//    SPI1_BUF = MosiByte;
//    
//    /* wait until the transmission of the byte is finished */
//    WAIT_SPI_IF;

//    MisoByte = SPI1_BUF;

//    /* reset transmission flag */
//    SPI1_IF = 0;
//    
//    return MisoByte;
//}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param Address     EtherCAT ASIC address ( upper limit is 0x1FFF )    for access.
 \param Command    ESC_WR performs a write access; ESC_RD performs a read access.

 \brief The function addresses the EtherCAT ASIC via SPI for a following SPI access.
*////////////////////////////////////////////////////////////////////////////////////////
//static void AddressingEsc( UINT16 Address, UINT8 Command )
//{
//    VARVOLATILE UBYTETOWORD tmp;
//    tmp.Word = ( Address << 3 ) | Command;



//    /* select the SPI */
//    SELECT_SPI;

//    /* send the first address/command byte to the ESC 
//       receive the first AL Event Byte*/
//    EscALEvent.Byte[0] = RxTxSpiData(tmp.Byte[1]);

//    EscALEvent.Byte[1] = RxTxSpiData(tmp.Byte[0]);
//}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief  The function operates a SPI access without addressing.

        The first two bytes of an access to the EtherCAT ASIC always deliver the AL_Event register (0x220).
        It will be saved in the global "EscALEvent"
*////////////////////////////////////////////////////////////////////////////////////////
static void GetInterruptRegister(void)
{
       #if AL_EVENT_ENABLED
    DISABLE_AL_EVENT_INT;
#endif
	
    /* select the SPI */
    //SELECT_SPI;
	  SPI_ON;
	
	 HW_EscReadIsr((MEM_ADDR *)&EscALEvent.Word, 0x220, 2);
	/* if the SPI transmission rate is higher than 15 MBaud, the Busy detection shall be
       done here */
	
  //DESELECT_SPI;
	SPI_OFF;
 #if AL_EVENT_ENABLED
    ENABLE_AL_EVENT_INT;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief  The function operates a SPI access without addressing.
        Shall be implemented if interrupts are supported else this function is equal to "GetInterruptRegsiter()"

        The first two bytes of an access to the EtherCAT ASIC always deliver the AL_Event register (0x220).
        It will be saved in the global "EscALEvent"
*////////////////////////////////////////////////////////////////////////////////////////
static void ISR_GetInterruptRegister(void)
{
     /* SPI should be deactivated to interrupt a possible transmission */
    
	SPI_OFF;

    /* select the SPI */
    
	__NOP();__NOP();__NOP();
	SPI_ON;

		 HW_EscReadIsr((MEM_ADDR *)&EscALEvent.Word, 0x220, 2);

  /* if the SPI transmission rate is higher than 15 MBaud, the Busy detection shall be
       done here */

    
	SPI_OFF;
}
/*ECATCHANGE_END(V5.12) EL9800 1*/

/*--------------------------------------------------------------------------------------
------
------    exported hardware access functions
------
--------------------------------------------------------------------------------------*/


/////////////////////////////////////////////////////////////////////////////////////////
/**
\return     0 if initialization was successful

 \brief    This function intialize the Process Data Interface (PDI) and the host controller.
*////////////////////////////////////////////////////////////////////////////////////////
UINT8 HW_Init(void)
{
     UINT16 intMask;
    UINT32 data;
	
	RST_L;
	
	HAL_Delay(50);
	
	RST_H;
	HAL_Delay(50);
	
	mem_test();// ²âÊÔPDI½Ó¿Ú
	

    do
    {
        intMask = 0x93;
        HW_EscWriteWord(intMask, ESC_AL_EVENTMASK_OFFSET);
       
        intMask = 0;
        HW_EscReadWord(intMask, ESC_AL_EVENTMASK_OFFSET);
    } while (intMask != 0x93);

   
    //IRQ enable,IRQ polarity, IRQ buffer type in Interrupt Configuration register.
    //Wrte 0x54 - 0x00000101
    data = 0x00000101;
 
    SPIWriteDWord (0x54,data);
    
    //Write in Interrupt Enable register -->
    //Write 0x5c - 0x00000001
    data = 0x00000001;
    SPIWriteDWord (0x5C, data);
    
    SPIReadDWord(0x58);	

		intMask = 0x00;
	  
    HW_EscWriteDWord(intMask, ESC_AL_EVENTMASK_OFFSET);
		

		INIT_ESC_INT;
		ENABLE_ESC_INT();

		INIT_SYNC0_INT;
		INIT_SYNC1_INT;

		ENABLE_SYNC0_INT;
		ENABLE_SYNC1_INT;

		INIT_ECAT_TIMER;
		HAL_TIM_Base_Start_IT(&htimx);
		START_ECAT_TIMER;

    /* enable all interrupts */
    ENABLE_GLOBAL_INT;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    This function shall be implemented if hardware resources need to be release
        when the sample application stops
*////////////////////////////////////////////////////////////////////////////////////////
void HW_Release(void)
{
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    first two Bytes of ALEvent register (0x220)

 \brief  This function gets the current content of ALEvent register
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 HW_GetALEventRegister(void)
{
    GetInterruptRegister();
    return EscALEvent.Word;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    first two Bytes of ALEvent register (0x220)

 \brief  The SPI PDI requires an extra ESC read access functions from interrupts service routines.
        The behaviour is equal to "HW_GetALEventRegister()"
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 HW_GetALEventRegister_Isr(void)
{
     ISR_GetInterruptRegister();
    return EscALEvent.Word;
}



/*ECATCHANGE_START(V5.12) EL9800 1*/
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param pData        Pointer to a byte array which holds data to write or saves read data.
 \param Address     EtherCAT ASIC address ( upper limit is 0x1FFF )    for access.
 \param Len            Access size in Bytes.

 \brief  This function operates the SPI read access to the EtherCAT ASIC.
*////////////////////////////////////////////////////////////////////////////////////////
void HW_EscRead( MEM_ADDR *pData, UINT16 Address, UINT16 Len )
{
     /* HBu 24.01.06: if the SPI will be read by an interrupt routine too the
                     mailbox reading may be interrupted but an interrupted
                     reading will remain in a SPI transmission fault that will
                     reset the internal Sync Manager status. Therefore the reading
                     will be divided in 1-byte reads with disabled interrupt */
    UINT16 i;
    UINT8 *pTmpData = (UINT8 *)pData;

    /* loop for all bytes to be read */
    while ( Len > 0 )
    {
        if (Address >= 0x1000)
        {
            i = Len;
        }
        else
        {
            i= (Len > 4) ? 4 : Len;

            if(Address & 01)
            {
               i=1;
            }
            else if (Address & 02)
            {
               i= (i&1) ? 1:2;
            }
            else if (i == 03)
            {
                i=1;
            }
        }

        DISABLE_AL_EVENT_INT;

       SPIReadDRegister(pTmpData,Address,i);
				
       ENABLE_AL_EVENT_INT;

        Len -= i;
        pTmpData += i;
        Address += i;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param pData        Pointer to a byte array which holds data to write or saves read data.
 \param Address     EtherCAT ASIC address ( upper limit is 0x1FFF )    for access.
 \param Len            Access size in Bytes.

\brief  The SPI PDI requires an extra ESC read access functions from interrupts service routines.
        The behaviour is equal to "HW_EscRead()"
*////////////////////////////////////////////////////////////////////////////////////////
void HW_EscReadIsr( MEM_ADDR *pData, UINT16 Address, UINT16 Len )
{
    UINT16 i;
   UINT8 *pTmpData = (UINT8 *)pData;

    /* send the address and command to the ESC */

    /* loop for all bytes to be read */
   while ( Len > 0 )
   {

        if (Address >= 0x1000)
        {
            i = Len;
        }
        else
        {
            i= (Len > 4) ? 4 : Len;

            if(Address & 01)
            {
               i=1;
            }
            else if (Address & 02)
            {
               i= (i&1) ? 1:2;
            }
            else if (i == 03)
            {
                i=1;
            }
        }

        SPIReadDRegister(pTmpData, Address,i);

        Len -= i;
        pTmpData += i;
        Address += i;
    }
    
    /* there has to be at least 15 ns + CLK/2 after the transmission is finished
       before the SPI1_SEL signal shall be 1 */
    //DESELECT_SPI
}

/*******************************************************************************

 \param RunLed            desired EtherCAT Run led state
 \param ErrLed            desired EtherCAT Error led state

  \brief    This function updates the EtherCAT run and error led
  *****************************************************************************/
void HW_SetLed(UINT8 RunLed,UINT8 ErrLed)
{
    /* Here RunLed is not used. Because on chip supported RUN Led is available*/    
//    LED_ECATRED   = ErrLed;
	  HAL_GPIO_WritePin(GPIOD, Lan9252_E_LED_Pin, ErrLed>0? GPIO_PIN_RESET:GPIO_PIN_SET);
}


/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param pData        Pointer to a byte array which holds data to write or saves write data.
 \param Address     EtherCAT ASIC address ( upper limit is 0x1FFF )    for access.
 \param Len            Access size in Bytes.

  \brief  This function operates the SPI write access to the EtherCAT ASIC.
*////////////////////////////////////////////////////////////////////////////////////////
void HW_EscWrite( MEM_ADDR *pData, UINT16 Address, UINT16 Len )
{
   UINT16 i;
    UINT8 *pTmpData = (UINT8 *)pData;

    /* loop for all bytes to be written */
    while ( Len )
    {

        if (Address >= 0x1000)
        {
            i = Len;
        }
        else
        {
            i= (Len > 4) ? 4 : Len;

            if(Address & 01)
            {
               i=1;
            }
            else if (Address & 02)
            {
               i= (i&1) ? 1:2;
            }
            else if (i == 03)
            {
                i=1;
            }
        }

        DISABLE_AL_EVENT_INT;
       
        /* start transmission */

        SPIWriteRegister(pTmpData, Address, i);


        ENABLE_AL_EVENT_INT;

       
   
        /* next address */
        Len -= i;
        pTmpData += i;
        Address += i;

    }
}


/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param pData        Pointer to a byte array which holds data to write or saves write data.
 \param Address     EtherCAT ASIC address ( upper limit is 0x1FFF )    for access.
 \param Len            Access size in Bytes.

 \brief  The SPI PDI requires an extra ESC write access functions from interrupts service routines.
        The behaviour is equal to "HW_EscWrite()"
*////////////////////////////////////////////////////////////////////////////////////////
void HW_EscWriteIsr( MEM_ADDR *pData, UINT16 Address, UINT16 Len )
{
   UINT16 i ;
    UINT8 *pTmpData = (UINT8 *)pData;

  
    /* loop for all bytes to be written */
    while ( Len )
    {

        if (Address >= 0x1000)
        {
            i = Len;
        }
        else
        {
            i= (Len > 4) ? 4 : Len;

            if(Address & 01)
            {
               i=1;
            }
            else if (Address & 02)
            {
               i= (i&1) ? 1:2;
            }
            else if (i == 03)
            {
                i=1;
            }
        }
        
       /* start transmission */


       SPIWriteRegister(pTmpData, Address, i);

       
       /* next address */
        Len -= i;
        pTmpData += i;
        Address += i;
    }

    /* there has to be at least 15 ns + CLK/2 after the transmission is finished
       before the SPI1_SEL signal shall be 1 */
    //DESELECT_SPI
}
/*ECATCHANGE_END(V5.12) EL9800 1*/




/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    Interrupt service routine for the PDI interrupt from the EtherCAT Slave Controller
*////////////////////////////////////////////////////////////////////////////////////////

//void __attribute__ ((__interrupt__, no_auto_psv)) EscIsr(void)
//{
//     PDI_Isr();

//    /* reset the interrupt flag */
//    ACK_ESC_INT;

//}



///////////////////////////////////////////////////////////////////////////////////////////
///**
// \brief    Interrupt service routine for the interrupts from SYNC0
//*////////////////////////////////////////////////////////////////////////////////////////
//void __attribute__((__interrupt__, no_auto_psv)) Sync0Isr(void)
//{
//    Sync0_Isr();

//    /* reset the interrupt flag */
//    ACK_SYNC0_INT;
//}
///////////////////////////////////////////////////////////////////////////////////////////
///**
// \brief    Interrupt service routine for the interrupts from SYNC1
//*////////////////////////////////////////////////////////////////////////////////////////
//void __attribute__((__interrupt__, no_auto_psv)) Sync1Isr(void)
//{
//    Sync1_Isr();

//    /* reset the interrupt flag */
//    ACK_SYNC1_INT;
//}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin==GPIO_PIN_3)
  {		
     PDI_Isr();
     /* reset the interrupt flag */
		 ACK_ESC_INT; 		
		     return;
	}
  else if(GPIO_Pin==GPIO_PIN_7)
  {	
    Sync1_Isr();
    /* reset the interrupt flag */

    ACK_SYNC1_INT;		
		     return;
	}
  else if(GPIO_Pin==GPIO_PIN_4)
  {	
    Sync0_Isr();
    /* reset the interrupt flag */

    ACK_SYNC0_INT;		
		     return;
	}	
}

void ESC_TIME_ISR(void)
{
	static UINT16 number_led_tick=0;
			DISABLE_ESC_INT();
		
		  ECAT_CheckTimer();

			ECAT_TIMER_ACK_INT;
		
			ENABLE_ESC_INT();
	if(number_led_tick++>=500)
	{
		number_led_tick=0;
			HAL_GPIO_TogglePin(SYS_RUN_GPIO_Port,SYS_RUN_Pin);
	}

}

/** @} */
