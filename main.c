
#include <alarmClock.h>
#include <MyI2C.h>
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
//#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "inc/tm4c123gh6pm.h"
#include "debug.h"
#include "PCF8574A_LCD_123GH6PM.h"
#include "utils/ustdlib.h"
#include "driverlib/hibernate.h"
#include "driverlib/timer.h"

//aaaaaaaaaaaa

void Hibernate_Int(void);
void Initialzation(void);
void Init_UART1(void);
void Init_UART0_DBG(void);

int main(void)
{
    Initialzation();

    int countChar=0;
    char charGet;
    typeUARTSignal UARTSignal;
while(1) {

    if(UARTCharsAvail(UART1_BASE)){
        charGet = UARTCharGet(UART1_BASE);
        UARTCharPut(UART0_BASE, charGet);
        if (charGet=='s'){
               UARTSignal= synSignal;
           }
           else if (charGet=='a'){
               UARTSignal= alarmSignal;
           }
           else  if (charGet=='x') {
               countChar=0;
               alarmTime.hour=100;     //this is not a valid hour value
               alarmTime.minute=100;   //this is not a valid minute value
               HibernateDataSet((uint32_t *)(&alarmTime), 2);
               DBG("Off Alarm\n");
               DisplayLCD_OffAlarm();
               ClearUARTSynTime();
               HibernateRTCMatchSet(0,HibernateRTCGet()+10);
           }
           else if ((charGet=='e')&&(UARTSignal== synSignal)) {
               countChar=0;
               AdjustUARTSynTime();
               synTime = UARTToStructCalendar();
               HibernateRTCSet(StructCalenderToCounter(synTime)); //save synTime into RTC
               DBG("Syn Day and Time %u %u/%u/%u %u:%u:%u\n",synTime.wday, synTime.day,synTime.month, synTime.year,
                   synTime.hour, synTime.minute, synTime.second);
              DisplayLCD_SynTime(synTime) ;
               ClearUARTSynTime();
               HibernateRTCMatchSet(0,HibernateRTCGet()+10);
           }
           else if ((charGet=='e')&&(UARTSignal== alarmSignal)) {
               countChar=0;
               AdjustUARTAlarmTime();
               alarmTime = UARTToStructAlarm();
               HibernateDataSet((uint32_t *)(&alarmTime), 2);//save alarmTime into hibernation memory
               DBG("Set Alarm %u:%u\n",alarmTime.hour,alarmTime.minute);
               DisplayLCD_OnAlarm(alarmTime);
               ClearUARTAlarmTime();
               HibernateRTCMatchSet(0,HibernateRTCGet()+10);
           }
           else if(UARTSignal== synSignal){
               UARTSynTime[countChar]= charGet;
               countChar++;
           }
           else if (UARTSignal== alarmSignal){
               UARTAlarmTime[countChar]= charGet;
               countChar++;
           }
    }
}
}

void Hibernate_Int(void){
//        TimerEnable(TIMER0_BASE, TIMER_A);
    HibernateIntClear(HIBERNATE_INT_RTC_MATCH_0);
    synTime = CounterToStructCalender(HibernateRTCGet());
    DBG("%u %u/%u/%u %u:%u:%u\n",synTime.wday, synTime.day,synTime.month, synTime.year,
        synTime.hour, synTime.minute, synTime.second);
    HibernateDataGet((uint32_t *)(&alarmTime), 2);
    /*********************************************/
    DisplayLCD_DayTime(synTime);

    if  (CheckAlarm(alarmTime, synTime)) {
        AlarmAction();
    }

    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);

    HibernateRTCMatchSet(0,HIB_RTCC_R+20);

    HibernateIntDisable(HIBERNATE_INT_RTC_MATCH_0);
    IntMasterDisable();
//    TimerDisable(TIMER0_BASE, TIMER_A);
    HibernateRequest();
}

void Initialzation(void){

    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_INT);
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
//    TimerConfigure(TIMER0_BASE, TIMER_CFG_ONE_SHOT_UP);
//    TimerLoadSet(TIMER0_BASE, TIMER_A, 0xFfFfFfFf);  // set value overflow 1
//    TimerEnable(TIMER0_BASE, TIMER_A);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);
    InitI2C0();

    SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);
    HibernateEnableExpClk(0);  //Enables the Hibernation module, the argument is not helpful
    HibernateRTCDisable();
    HibernateGPIORetentionEnable();  // using VDD3ON mode
    HibernateRTCEnable();
    HibernateWakeSet(HIBERNATE_WAKE_PIN | HIBERNATE_WAKE_RTC);
    HibernateIntRegister(Hibernate_Int);
    HibernateIntEnable(HIBERNATE_INT_RTC_MATCH_0);// set bit in IM enable HIB to send a INT signal
    IntMasterEnable();  // this is really not helpful
//    IntTrigger(HIBERNATE_INT_RTC_MATCH_0);

//    TimerDisable(TIMER0_BASE, TIMER_A);

    HibernateRTCTrimSet(0x7fff);
    HibernateRTCSSMatchSet(0, 0);
    HibernateRTCMatchSet(0,HibernateRTCGet()+15);
//    SysCtlDelay(SysCtlClockGet()/3/2); //delay 0.5s to ensure a stable system after wake up
//    Lcd_init();
//    SysCtlDelay(SysCtlClockGet()/3); //delay 0.5s to ensure a stable system after wake up
    Lcd_init();
    synTime = CounterToStructCalender(HibernateRTCGet());

    DisplayLCD_DayTime(synTime);

    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3); //On green LED, informing user that the system is in awake
    Init_UART1();
    Init_UART0_DBG();

}

void Init_UART1(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB0_U1RX);
//    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 9600,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTEnable(UART1_BASE);
}
void Init_UART0_DBG(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
//    IntEnable(INT_UART0); //enable the UART interrupt
//    UARTIntEnable(UART0_BASE, UART_INT_RX|UART_INT_RT);

    UARTEnable(UART0_BASE);
    UARTStdioConfig(0, 115200, SysCtlClockGet());
}
