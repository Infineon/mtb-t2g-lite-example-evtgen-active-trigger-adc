/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the T2G MCU event generator 
*              (EVTGEN) trigger ADC conversion example for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/******************************************************************************
* Include header files
******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* ADC channel number definition */
#define ADC_LOGICAL_CHANNEL     (0u)

/* Assign the ADC interrupt number and priority */
#define ADC_IRQ_NUM             (NvicMux2_IRQn)
#define ADC_INTR_NUM            (((ADC_IRQ_NUM << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | ADC_CH0_IRQ))
#define ADC_INTR_PRIORITY       (7u)

/* Event generator comparator structure number */
#if defined (CY_IP_M7CPUSS)
#define EVTGEN_COMP_STRUCT_NUM  (12u)
#define EVTGEN_COMP_CONFIG      EVTGEN_comp12_config
#else
#define EVTGEN_COMP_STRUCT_NUM  (0u)
#define EVTGEN_COMP_CONFIG      EVTGEN_comp0_config
#endif

/* Assign the EVTGEN interrupt number and priority */
#define EVTGEN_IRQ_NUM          (NvicMux3_IRQn)
#define EVTGEN_INTR_NUM         (((EVTGEN_IRQ_NUM << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | EVTGEN_IRQ))
#define EVTGEN_INTR_PRIORITY    (7u)

/*******************************************************************************
* Function Prototypes
********************************************************************************/
/* Event generator interrupt handler */
void evtgen_isr (void);

/* ADC interrupt handler */
void adc_int_handler (void);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* ADC interrupt configuration */
const cy_stc_sysint_t irq_cfg_sar =
{
    .intrSrc  = ADC_INTR_NUM,
    .intrPriority  = ADC_INTR_PRIORITY,
};

/* Event generator (EVTGEN) interrupt configuration */
const cy_stc_sysint_t irq_cfg_evtgen =
{
    .intrSrc  = EVTGEN_INTR_NUM,
    .intrPriority  = EVTGEN_INTR_PRIORITY,
};

/* ADC conversion complete flag */
volatile uint8_t adc_done_flag = 0;

/* ADC conversion result */
uint16_t adc_result;


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It does...
*    1. Initialize the UART block for uart logging.
*    2. Initialize the ADC and register ADC interrupt handler.
*    3. Initialize the event generator and comparator structure to trigger ADC conversion.
*    Do Forever loop:
*    4. Check if ADC conversion completes and print ADC result by UART.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("********************************************************************************\r\n");
    printf("Event generator trigger ADC conversion\r\n");
    printf("********************************************************************************\r\n");

    /* Initialize ADC */
    Cy_SAR2_Init(ADC_HW, &ADC_config);
    /* Set ADC group done interrupt */
    Cy_SAR2_Channel_SetInterruptMask(ADC_HW, ADC_LOGICAL_CHANNEL, CY_SAR2_INT_GRP_DONE);

    /* Register ADC interrupt handler and enable interrupt */
    Cy_SysInt_Init(&irq_cfg_sar, &adc_int_handler); 
    NVIC_ClearPendingIRQ(ADC_IRQ_NUM); 
    NVIC_EnableIRQ((IRQn_Type)ADC_IRQ_NUM);

    /* Register event generator interrupt handler and enable interrupt */
    Cy_SysInt_Init(&irq_cfg_evtgen, &evtgen_isr);
    Cy_EvtGen_ClearInterrupt(EVTGEN_HW, 0xFFu);
    NVIC_ClearPendingIRQ(EVTGEN_IRQ_NUM);
    NVIC_EnableIRQ((IRQn_Type)EVTGEN_IRQ_NUM);

    /* Initialize event generator */
    Cy_EvtGen_Init(EVTGEN_HW, &EVTGEN_config);
    /* Start event generator */
    Cy_EvtGen_Enable(EVTGEN_HW);
    /* Delay a bit and wait for the counter completed initialization */
    Cy_SysLib_DelayUs(625);
    /* Check if the ratio status is valid during hardware control ratio */
    if((CY_EVTGEN_RATIO_CONTROL_HW == EVTGEN_config.ratioControlMode) && (!Cy_EvtGen_GetRatioStatus(EVTGEN_HW)))
    {
        CY_ASSERT(0);
    }
    /* Check if the event generator counter status is valid */
    if(CY_EVTGEN_COUNTER_STATUS_VALID != Cy_EvtGen_GetCounterStatus(EVTGEN_HW))
    {
        CY_ASSERT(0);
    }
    /* Initialize the event generator comparator structure */
    Cy_EvtGen_InitStruct(EVTGEN_HW, EVTGEN_COMP_STRUCT_NUM, &EVTGEN_COMP_CONFIG);

    for (;;)
    {
        /* Check ADC conversion done flag */
        if(adc_done_flag != 0)
        {
            adc_done_flag = 0;
            /* Print out the ADC conversion result by UART */
            printf("ADC conversion complete, result: %d\r\n", adc_result);
        }
    }
}

/*******************************************************************************
* Function Name: evtgen_isr
********************************************************************************
* Summary:
* This is the interrupt handler function for the event generator interrupt 
* to update active comparator value.
*
* Parameters:
*  none
*
*******************************************************************************/
void evtgen_isr (void)
{
    /* Get active comparator interrupt status */
    if(Cy_EvtGen_GetStructInterrupt(EVTGEN_HW, EVTGEN_COMP_STRUCT_NUM))
    {
        /* Clear interrupt */
        Cy_EvtGen_ClearStructInterrupt(EVTGEN_HW, EVTGEN_COMP_STRUCT_NUM);
        /* Update active comparator value */
        Cy_EvtGen_UpdateActiveCompValue(EVTGEN_HW, EVTGEN_COMP_STRUCT_NUM, 
                                        EVTGEN_COMP_CONFIG.valueActiveComparator);
    }
}

/*******************************************************************************
* Function Name: adc_int_handler
********************************************************************************
* Summary:
* This is the interrupt handler function for the ADC interrupt.
*
* Parameters:
*  none
*
*******************************************************************************/
void adc_int_handler (void)
{
    uint32_t adc_status = 0;

    /* Get interrupt status */
    uint32_t intrSource = Cy_SAR2_Channel_GetInterruptStatusMasked(ADC_HW, ADC_LOGICAL_CHANNEL);
    if(CY_SAR2_INT_GRP_DONE == (intrSource & CY_SAR2_INT_GRP_DONE))
    {
        /* Get the ADC conversion result and status */
        adc_result = Cy_SAR2_Channel_GetResult(ADC_HW, ADC_LOGICAL_CHANNEL, &adc_status);
        if(CY_SAR2_STATUS_VALID == (adc_status & CY_SAR2_STATUS_VALID))
        {
            adc_done_flag = 1;
        }
        /* Clear interrupt source */
        Cy_SAR2_Channel_ClearInterrupt(ADC_HW, ADC_LOGICAL_CHANNEL, CY_SAR2_INT_GRP_DONE);
    }
}

/* [] END OF FILE */
