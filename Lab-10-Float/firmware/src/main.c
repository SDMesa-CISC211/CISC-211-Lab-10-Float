/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project. It is intended to
    be used as the starting point for CISC-211 Curiosity Nano Board
    programming projects. After initializing the hardware, it will
    go into a 0.5s loop that calls an assembly function specified in a separate
    .s file. It will print the iteration number and the result of the assembly 
    function call to the serial port.
    As an added bonus, it will toggle the LED on each iteration
    to provide feedback that the code is actually running.
  
    NOTE: PC serial port should be set to 115200 rate.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdio.h>
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include <float.h>
#include "definitions.h"                // SYS function prototypes
#include "printFuncs.h"  // lab print funcs
#include "testFuncs.h"  // lab test funcs

#include "asmExterns.h"  // references to data defined in asmFloat.s

/* RTC Time period match values for input clock of 1 KHz */
#define PERIOD_50MS                             51
#define PERIOD_500MS                            512
#define PERIOD_1S                               1024
#define PERIOD_2S                               2048
#define PERIOD_4S                               4096

#define MAX_PRINT_LEN 1000

#define PLUS_INF ((0x7F800000))
#define NEG_INF  ((0xFF800000))
#define NAN_MASK  (~NEG_INF)

#ifndef NAN
#define NAN ((0.0/0.0))
#endif

#ifndef INFINITY
#define INFINITY ((1.0f/0.0f))
#define NEG_INFINITY ((-1.0f/0.0f))
#endif


static volatile bool isRTCExpired = false;
static volatile bool changeTempSamplingRate = false;
static volatile bool isUSARTTxComplete = true;
static uint8_t uartTxBuffer[MAX_PRINT_LEN] = {0};

// static char * pass = "PASS";
// static char * fail = "FAIL";
// static char * oops = "OOPS";

// PROF COMMENT:
// The ARM calling convention permits the use of up to 4 registers, r0-r3
// to pass data into a function. Only one value can be returned from a C
// function call. It will be stored in r0, which will be automatically
// used by the C compiler as the function's return value.
//
// Function signature
// For this lab, return the larger of the two floating point values passed in.
extern float * asmFmax(uint32_t, uint32_t);


// Change the value below to 1 if you want to use the debug array.
// You can change the debug array without fear of losing the original
// test cases.
// Leave it set to 0 if you want to use the exact array that will be used
// for grading
#define USE_DEBUG_TESTCASES 0

#if USE_DEBUG_TESTCASES
static float tc[][2] = { // Modify these if you want to debug
    {   1.175503179e-38, 1.10203478208e-38 },
    {    -0.2,                 -0.1}, 
    {     1.0,                  2.0}, 
    {    -3.1,                 -1.2}, 
    {     NAN,                  1.0}, 
    {    -1.0,                  NAN}, 
    {     0.1,                  0.99},  // 
    {     1.14437421182e-28,   785.066650391},  //
    { -4000.1,                   0.0,},  // 
    {    -1.9e-5,               -1.9e-5},  // 
    {     1.347e10,              2.867e-10},  // 
    {     1.4e-42,              -3.2e-43}, // subnormals
    {     -2.4e-42,              2.313e29}, // subnormals
    {    INFINITY,           NEG_INFINITY},
    {    NEG_INFINITY,           -6.24},
    {     1.0,                   0.0}
};
#else
static float tc[][2] = { // DO NOT MODIFY THESE!!!!!
    {   1.175503179e-38, 1.10203478208e-38 },
    {    -0.2,                 -0.1}, 
    {     1.0,                  2.0}, 
    {    -3.1,                  -1.2}, 
    {     NAN,                  1.0}, 
    {    -1.0,                  NAN}, 
    {     0.1,                  0.99},  // 
    {     1.14437421182e-28,   785.066650391},  //
    { -4000.1,                   0.0,},  // 
    {    -1.9e-5,               -1.9e-5},  // 
    {     1.347e10,              2.867e-10},  // 
    // PROF NOTE: Check subnormals: they seem to generate 0x00000000 as inputs
    {     1.4e-42,              -3.2e-43}, // subnormals
    {     -2.4e-42,              2.313e29}, // subnormals
    {    INFINITY,           NEG_INFINITY},
    {    NEG_INFINITY,           -6.24},
    {     1.0,                   0.0}
};
#endif

#define USING_HW 1

#if USING_HW
static void rtcEventHandler (RTC_TIMER32_INT_MASK intCause, uintptr_t context)
{
    if (intCause & RTC_MODE0_INTENSET_CMP0_Msk)
    {            
        isRTCExpired    = true;
    }
}
static void usartDmaChannelHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    {
        isUSARTTxComplete = true;
    }
}
#endif


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
int main ( void )
{
    
 
#if USING_HW
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, usartDmaChannelHandler, 0);
    RTC_Timer32CallbackRegister(rtcEventHandler, 0);
    RTC_Timer32Compare0Set(PERIOD_50MS);
    RTC_Timer32CounterSet(0);
    RTC_Timer32Start();

#else // using the simulator
    isRTCExpired = true;
    isUSARTTxComplete = true;
#endif //SIMULATOR

    int32_t passCount = 0;
    int32_t failCount = 0;
    int32_t totalPassCount = 0;
    int32_t totalFailCount = 0;
    int32_t totalTestCount = 0;
    int iteration = 0;   
    int maxIterations = sizeof(tc)/sizeof(tc[0]);
    
    while ( true )
    {
        if (isRTCExpired == true)
        {
            isRTCExpired = false;
            
            LED0_Toggle();
            
            // Set to true if you want to force specific values for debugging
            if (false)
            {
                tc[iteration][0] = reinterpret_uint_to_float(0x0080003F);
                tc[iteration][1] = reinterpret_uint_to_float(0x000FFF3F);
            }
            
            uint32_t ff1 = reinterpret_float_to_uint(tc[iteration][0]);
            uint32_t ff2 = reinterpret_float_to_uint(tc[iteration][1]);
            
            // Place to store the result of the call to the assy function
            float *max;
            
            // Set to true and set the test number if you want to break at
            // a particular test for debugging purposes.
            if (false && iteration == 11)
            {
                // This is a NO-OP, put here as a convenient place to
                // set a breakpoint for debugging a specific test case
                max = (float *) NULL;
            }
            
            // Make the call to the assembly function
            max = asmFmax(ff1,ff2);
            
            testResult(iteration,tc[iteration][0],tc[iteration][1],
                    max,
                    &fMax,
                    &passCount,
                    &failCount,
                    &isUSARTTxComplete);
            totalPassCount += passCount;        
            totalFailCount += failCount;        
            totalTestCount += failCount + passCount;        
             ++iteration;
            
            if (iteration >= maxIterations)
            {
                break; // tally the results and end program
            }
            
        }
    }

#if USING_HW
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= %s: ALL TESTS COMPLETE!\r\n"
            "tests passed: %ld \r\n"
            "tests failed: %ld \r\n"
            "total tests:  %ld \r\n"
            "score: %ld/20 points \r\n\r\n",
            (char *) nameStrPtr,
            totalPassCount,
            totalFailCount,
            totalTestCount,
            20*totalPassCount/totalTestCount); 
            isUSARTTxComplete = false;
#if 0            
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
                    (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
                    strlen((const char*)uartTxBuffer));
#endif
    
    printAndWait((char*)uartTxBuffer,&isUSARTTxComplete);

#else
            isRTCExpired = true;
            isUSARTTxComplete = true;
            if (iteration >= maxIterations)
            {
                break; // end program
            }

            continue;
#endif

    
    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}
/*******************************************************************************
 End of File
*/

