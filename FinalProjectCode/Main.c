/******************************************************************************/
/*                                                                            */
/* main.c -- Example program using the PmodDHB1 IP                            */
/*                                                                            */
/******************************************************************************/
/* Author: Arvin Tang                                                         */
/*                                                                            */
/******************************************************************************/
/* File Description:                                                          */
/*                                                                            */
/* This demo drives 2 motors in the 4 possible directions. When mounted on a  */
/* 2-wheel chassis, the motors will be driven such that the robot goes        */
/* forward, goes backward, turns left, and turns right.                       */
/*                                                                            */
/******************************************************************************/
/* Revision History:                                                          */
/*                                                                            */
/*    09/14/2017(atangzwj): Created                                           */
/*    02/03/2018(atangzwj): Validated for Vivado 2017.4                       */
/*                                                                            */
/******************************************************************************/

/************ Include Files ************/

#include "MotorFeedback.h"
#include "PmodDHB1.h"
#include "PWM.h"
#include "sleep.h"
#include "xil_cache.h"

//RTCC
#include "PmodRTCC.h"
#include "xparameters.h"

//Temp
#include "PmodTMP3.h"
#include "xil_printf.h"


/*************************** Type Declarations *****************************/

// Struct containing each field of the RTCC's time registers represented in
// 8-bit binary coded decimal - 0x30 in the minute field represents 30 minutes.
typedef struct RTCC_Time {
   u8 second;
   u8 minute;
   u8 hour;
   u8 ampm;
   u8 day;
   u8 date;
   u8 month;
   u8 year;
} RTCC_Time;

/************ Macro Definitions ************/

#define GPIO_BASEADDR     XPAR_PMODDHB1_0_AXI_LITE_GPIO_BASEADDR
#define PWM_BASEADDR      XPAR_PMODDHB1_0_PWM_AXI_BASEADDR
#define MOTOR_FB_BASEADDR XPAR_PMODDHB1_0_MOTOR_FB_AXI_BASEADDR

#ifdef __MICROBLAZE__
#define CLK_FREQ XPAR_CPU_M_AXI_DP_FREQ_HZ
#else
#define CLK_FREQ 100000000 // FCLK0 frequency not found in xparameters.h
#endif

#define PWM_PER              2
#define SENSOR_EDGES_PER_REV 4
#define GEARBOX_RATIO        48

#define SET_RTCC 1


/************ Function Prototypes ************/

void DemoInitializeMotor();

void DemoRunMotorLong();

void DemoRunMotorShort();

void DemoRun();

//RTCC
void DemoRunRTCC();
void DemoInitializeRTCC(u8 mode);

//Temp
void DemoInitializeTemp();
int DemoRunTemp();

void DemoCleanup();

void EnableCaches();

void DisableCaches();

RTCC_Time GetTime(PmodRTCC *InstancePtr, RTCC_Target src);
RTCC_Time IncrementTime(RTCC_Time time, int delta_seconds);
void SetTime(PmodRTCC *InstancePtr, RTCC_Target dest, RTCC_Time val);
void PrintTime(RTCC_Target src);
u8 bcd2int(u8 data);
u8 int2bcd(u8 data);


/************ Global Variables ************/

PmodDHB1 pmodDHB1;
MotorFeedback motorFeedback;

PmodRTCC myDevice;

PmodTMP3 myDeviceTMP;

const char *weekdays[7] = {
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday",
   "Sunday"
};




/************ Function Definitions ************/

int main(void) {
   DemoInitializeMotor();
   DemoInitializeTemp();
   DemoInitializeRTCC(SET_RTCC);
   DemoRun();
   DemoCleanup();
   return 0;
}

void DemoInitializeMotor() {
   EnableCaches();
   DHB1_begin(&pmodDHB1, GPIO_BASEADDR, PWM_BASEADDR, CLK_FREQ, PWM_PER);
   MotorFeedback_init(
      &motorFeedback,
      MOTOR_FB_BASEADDR,
      CLK_FREQ,
      SENSOR_EDGES_PER_REV,
      GEARBOX_RATIO
   );
   DHB1_motorDisable(&pmodDHB1);
}

void DemoInitializeRTCC(u8 mode) {
   RTCC_Time time;

   EnableCaches();

   RTCC_begin(&myDevice, XPAR_PMODRTCC_0_AXI_LITE_IIC_BASEADDR, 0x6F);

   // Print the power-fail time-stamp
   xil_printf("Lost Power at: ");
   PrintTime(RTCC_TARGET_PWRD);
   xil_printf("\r\n");

   xil_printf("Power was back at: ");
   PrintTime(RTCC_TARGET_PWRU);
   xil_printf("\r\n");

   if (!RTCC_checkVbat(&myDevice) || mode) {
      // Set the real time clock to Tuesday 2/6/18 12:24:36 PM
      RTCC_stopClock(&myDevice);

      time.second = 0x36;
      time.minute = 0x24;
      time.hour   = 0x12;
      time.ampm   = RTCC_PM;
      time.day    = 0x01;
      time.date   = 0x06;
      time.month  = 0x02;
      time.year   = 0x18;

      time = IncrementTime(time, 0); // TEST
      SetTime(&myDevice, RTCC_TARGET_RTCC, time);

      RTCC_startClock(&myDevice);
      xil_printf("The time has been set \r\n");
      // Set vbat high
      RTCC_enableVbat(&myDevice);
   } else {
      time = GetTime(&myDevice, RTCC_TARGET_RTCC);
   }

   // Sset alarm 0 for 30 seconds from now
   time = IncrementTime(time, 30);
   SetTime(&myDevice, RTCC_TARGET_ALM0, time);

   // Sset alarm 1 for 1 minute from now
   time = IncrementTime(time, 30);
   SetTime(&myDevice, RTCC_TARGET_ALM1, time);

   // Pprint current time
   xil_printf("Current time is: ");
   PrintTime(RTCC_TARGET_RTCC);
   xil_printf("\r\n");

   // Print alarm 0
   xil_printf("Alarm 0 is set to : ");
   PrintTime(RTCC_TARGET_ALM0);
   xil_printf("\r\n");

   // Print alarm 1
   xil_printf("Alarm 1 is set to : ");
   PrintTime(RTCC_TARGET_ALM1);
   xil_printf("\r\n");

   // Enables alarm 0
   // Set configuration bits to:
   //    RTCC_ALM_POL | RTCC_ALMC2 | RTCC_ALMC1 | RTCC_ALMC0
   // This will drive the MPF pin high when the alarm triggered
   // It also sets the alarm to be triggered when the alarm matches
   // Seconds, Minutes, Hour, Day, Date, Month of the RTCC
   RTCC_enableAlarm(&myDevice, RTCC_TARGET_ALM0,
         RTCC_ALM_POL | RTCC_ALMC2 | RTCC_ALMC1 | RTCC_ALMC0);

   // Enable alarm 1
   // Set configuration bits to RTCC_ALM_POL
   // This will drive the MPF pin high when the alarm triggered
   // It also sets the alarm to be triggered when the alarm matches
   // Seconds of the RTCC
   RTCC_enableAlarm(&myDevice, RTCC_TARGET_ALM1,
         RTCC_ALM_POL | RTCC_ALMC2 | RTCC_ALMC1 | RTCC_ALMC0);

   // Enable back up battery
   RTCC_enableVbat(&myDevice);

   RTCC_clearPWRFAIL(&myDevice);
}

void DemoInitializeTemp() {
   EnableCaches();

   xil_printf("\x1B[H");  // Move terminal cursor to top left
   xil_printf("\x1B[1K"); // Clear terminal
   xil_printf("Connected to PmodTMP3 Demo over UART\n\r");

   TMP3_begin(&myDeviceTMP, XPAR_PMODTMP3_0_AXI_LITE_IIC_BASEADDR, TMP3_ADDR);
   xil_printf("Connected to PmodTMP3 over IIC on JB\n\r\n\r");
}

void DemoRunMotorLong() {
   DHB1_setMotorSpeeds(&pmodDHB1, 30, 50);
   DHB1_motorEnable(&pmodDHB1);
   sleep(30);
   DHB1_motorDisable(&pmodDHB1);
}

void DemoRunMotorShort() {
   DHB1_setMotorSpeeds(&pmodDHB1, 30, 50);
   DHB1_motorEnable(&pmodDHB1);
   sleep(20);
   DHB1_motorDisable(&pmodDHB1);
}

int DemoRunTemp() {
   double temp  = 0.0;
   double temp2 = 0.0;
   double temp3 = 0.0;
      temp  = TMP3_getTemp(&myDeviceTMP);
      temp2 = TMP3_CtoF(temp);
      temp3 = TMP3_FtoC(temp2);

      int temp2_round = 0;
      int temp2_int   = 0;
      int temp2_frac  = 0;
      // Round to nearest hundredth, multiply by 100
      if (temp2 < 0) {
         temp2_round = (int) (temp2 * 1000 - 5) / 10;
         temp2_frac  = -temp2_round % 100;
      } else {
         temp2_round = (int) (temp2 * 1000 + 5) / 10;
         temp2_frac  = temp2_round % 100;
      }
      temp2_int = temp2_round / 100;

      int temp3_round = 0;
      int temp3_int   = 0;
      int temp3_frac  = 0;
      if (temp3 < 0) {
         temp3_round = (int) (temp3 * 1000 - 5) / 10;
         temp3_frac  = -temp3_round % 100;
      } else {
         temp3_round = (int) (temp3 * 1000 + 5) / 10;
         temp3_frac  = temp3_round % 100;
      }
      temp3_int = temp3_round / 100;

      xil_printf("Temperature: %d.%d in Fahrenheit\n\r", temp2_int, temp2_frac);
      xil_printf("Temperature: %d.%d in Celsius\n\r", temp3_int, temp3_frac);
      return  temp2_int;
}


void DemoRunRTCC() {
   while (1) {
      sleep(1);

      // Print current time
      xil_printf("Current time is : ");
      PrintTime(RTCC_TARGET_RTCC);
      xil_printf("\r\n");

      // Check if alarm 0 is triggered
      if (RTCC_checkFlag(&myDevice, RTCC_TARGET_ALM0)) {
         // Alarm 0 has been triggered
         xil_printf("ALARM 0!!!");
         // Disable alarm 0
         RTCC_disableAlarm(&myDevice, RTCC_TARGET_ALM0);
         xil_printf("\r\n");
      }

      // Check if alarm 1 is triggered
      if (RTCC_checkFlag(&myDevice, RTCC_TARGET_ALM1)) {
         // Alarm 1 has been triggered
         xil_printf("ALARM 1!!!");
         // Disable alarm
         RTCC_disableAlarm(&myDevice, RTCC_TARGET_ALM1);
         xil_printf("\r\n");
      }
   }
}
void DemoRun(){
	RTCC_Time Timer;
	int temp;
	while(1)
	{
		sleep(1);
	    // Print current time
	    xil_printf("Current time is : ");
	    PrintTime(RTCC_TARGET_RTCC);
	    Timer = GetTime(&myDevice, RTCC_TARGET_RTCC);
	    if (Timer.hour == 0x12 && Timer.minute == 0x26 && Timer.second ==  0x00 && Timer.ampm == 0x01 )
	    {
	    	xil_printf("\r\n");
	    	print ("Activate Sprinkler Schedule 1");
	    	xil_printf("\r\n");
	    	temp = DemoRunTemp();
	    	xil_printf("\r\n");
	    	if (temp > 80)
	    	{
		    	print ("Tempeture is High!, Running Sprinkler for a longer time");
		    	xil_printf("\r\n");
		    	DemoRunMotorLong();
	    	}
	    	else
	    	{
		    	print ("Tempeture is normal, Running Sprinkler");
		    	xil_printf("\r\n");
		    	DemoRunMotorShort();
	    	}
	    }
	    else if (Timer.hour == 0x12 && Timer.minute == 0x28 && Timer.second ==  0x00 && Timer.ampm == 0x01 )
	    {
	    	xil_printf("\r\n");
	    	print ("Activate Sprinkler Schedule 2");
	    	xil_printf("\r\n");
	    	temp = DemoRunTemp();
	    	xil_printf("\r\n");
	    	if (temp > 80)
	    	{
		    	print ("Tempeture is High!, Running Sprinkler for a longer time");
		    	xil_printf("\r\n");
		    	DemoRunMotorLong();
	    	}
	    	else
	    	{
		    	print ("Tempeture is normal, Running Sprinkler");
		    	xil_printf("\r\n");
		    	DemoRunMotorShort();
	    	}
	    }
	    else if (Timer.hour == 0x12 && Timer.minute == 0x30 && Timer.second ==  0x00 && Timer.ampm == 0x01)
	    {
	    	xil_printf("\r\n");
	    	print ("Activate Sprinkler Schedule 3");
	    	xil_printf("\r\n");
	    	temp = DemoRunTemp();
	    	xil_printf("\r\n");
	    	if (temp > 80)
	    	{
		    	print ("Tempeture is High!, Running Sprinkler for a longer time");
		    	xil_printf("\r\n");
		    	DemoRunMotorLong();
	    	}
	    	else
	    	{
		    	print ("Tempeture is normal, Running Sprinkler");
		    	xil_printf("\r\n");
		    	DemoRunMotorShort();
	    	}
	    }
	    else
	    {
	    	xil_printf("\r\n");
	    }
	}
}

RTCC_Time GetTime(PmodRTCC *InstancePtr, RTCC_Target src) {
   RTCC_Time val;

   if (src != RTCC_TARGET_PWRD && src != RTCC_TARGET_PWRU) {
      val.second = RTCC_getSec(&myDevice, src);
   }

   val.minute = RTCC_getMin(&myDevice, src);
   val.hour   = RTCC_getHour(&myDevice, src);
   val.ampm   = RTCC_getAmPm(&myDevice, src);
   val.day    = RTCC_getDay(&myDevice, src);
   val.date   = RTCC_getDate(&myDevice, src);
   val.month  = RTCC_getMonth(&myDevice, src);

   if (src == RTCC_TARGET_RTCC) {
      val.year = RTCC_getYear(&myDevice);
   } else {
      val.year = 0;
   }

   return val;
}

void SetTime(PmodRTCC *InstancePtr, RTCC_Target dest, RTCC_Time val) {
   if (dest != RTCC_TARGET_PWRD && dest != RTCC_TARGET_PWRU) {
      RTCC_setSec(&myDevice, dest, val.second);
   }

   RTCC_setMin(&myDevice, dest, val.minute);
   RTCC_setHour12(&myDevice, dest, val.hour, val.ampm);
   RTCC_setDay(&myDevice, dest, val.day);
   RTCC_setDate(&myDevice, dest, val.date);
   RTCC_setMonth(&myDevice, dest, val.month);

   if (dest == RTCC_TARGET_RTCC) {
      RTCC_setYear(&myDevice, val.year);
   }
}

void PrintTime(RTCC_Target src) {
   RTCC_Time time;

   // Fetch the time from the device
   time = GetTime(&myDevice, src);

   xil_printf("%s %x/%x", weekdays[time.day], time.month, time.date);

   // Year is only available for the RTCC
   if (src == RTCC_TARGET_RTCC) {
      xil_printf("/%02x", time.year);
   }

   xil_printf(" %x:%02x", time.hour, time.minute);

   // Second is not supported by the power fail registers
   if (src != RTCC_TARGET_PWRD && src != RTCC_TARGET_PWRU) {
      xil_printf(":%02x", time.second);
   }

   if (time.ampm) {
      xil_printf(" PM");
   } else {
      xil_printf(" AM");
   }
}

u8 bcd2int(u8 data) {
   return ((data >> 4) * 10) + (data & 0xF);
}

u8 int2bcd(u8 data) {
   return (((data / 10) & 0xF) << 4) + ((data % 10) & 0xF);
}

RTCC_Time IncrementTime(RTCC_Time time, int delta_seconds) {
   RTCC_Time result;
   int temp;
   result = time;
   temp = bcd2int(result.second) + delta_seconds;
   result.second = int2bcd(temp % 60);          // Convert seconds
   temp = bcd2int(result.minute) + temp / 60;   // Carry seconds -> minutes
   result.minute = int2bcd(temp % 60);          // Convert minutes
   temp = bcd2int(result.hour) + temp / 60 - 1; // Carry minutes -> hours
   result.hour = int2bcd((temp % 12) + 1);      // Convert hours
   return result;
}

void DemoCleanup() {
   DisableCaches();
}


void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#endif
}

void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}
