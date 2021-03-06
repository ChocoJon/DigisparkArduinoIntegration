/*
 * Based on Obdev's AVRUSB code and under the same license.
 *
 * TODO: Make a proper file header. :-)
 * Modified for Digispark by Digistump
 * And now modified by Sean Murphy (duckythescientist) from a keyboard device to a mouse device
 * Most of the credit for the joystick code should go to Rapha�l Ass�nat
 * And now mouse credit is due to Yiyin Ma and Abby Lin of Cornell
 */
#ifndef __DigiMouse_h__
#define __DigiMouse_h__
 
#define REPORT_SIZE	4

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/delay.h>
#include <string.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "usbdrv.h"
//#include "devdesc.h"
#include "oddebug.h"
#include "usbconfig.h"
 
static uchar *rt_usbHidReportDescriptor=NULL;
static uchar rt_usbHidReportDescriptorSize=0;
static uchar *rt_usbDeviceDescriptor=NULL;
static uchar rt_usbDeviceDescriptorSize=0;

// TODO: Work around Arduino 12 issues better.
//#include <WConstants.h>
//#undef int()

typedef uint8_t byte;

/* What was most recently read from the controller */
unsigned char last_built_report[REPORT_SIZE];

/* What was most recently sent to the host */
unsigned char last_sent_report[REPORT_SIZE];

uchar    reportBuffer[REPORT_SIZE];

char must_report = 0;
unsigned char    idleRate;           // in 4 ms units 
unsigned char idleCounter = 0;



PROGMEM unsigned char mouse_usbHidReportDescriptor[] = { /* USB report descriptor */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE_PAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //   USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //   REPORT_COUNT (3)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x05,                    //   REPORT_SIZE (5)
    0x81, 0x01,                    //   Input(Cnst)
    0x05, 0x01,                    //   USAGE_PAGE(Generic Desktop)
    0x09, 0x30,                    //   USAGE(X)
    0x09, 0x31,                    //   USAGE(Y)
    0x15, 0x81,                    //   LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //   LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x81, 0x06,                    //   INPUT (Data,Var,Rel)
	0x09, 0x38,                     //   Usage (Wheel)
    0x95, 0x01,                     //   Report Count (1),
    0x81, 0x06,                     //   Input (Data, Variable, Relative)
    0xc0,                           // END_COLLECTION
    0xc0                           // END_COLLECTION
};


#define USBDESCR_DEVICE         1

unsigned char usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
#if USB_CFG_VENDOR_NAME_LEN
    1,          /* manufacturer string index */
#else
    0,          /* manufacturer string index */
#endif
#if USB_CFG_DEVICE_NAME_LEN
    2,          /* product string index */
#else
    0,          /* product string index */
#endif
#if USB_CFG_SERIAL_NUMBER_LENGTH
    3,          /* serial number string index */
#else
    0,          /* serial number string index */
#endif
    1,          /* number of configurations */
};


void buildReport(unsigned char *reportBuf)
{
	if (reportBuf != NULL)
		memcpy(reportBuf, last_built_report, REPORT_SIZE);
	
	memcpy(last_sent_report, last_built_report, REPORT_SIZE);	
}

void clearMove()
{//because we send deltas in movement, so when we send them, we clear them
	last_built_report[1] = 0;
	last_built_report[2] = 0;
	last_built_report[3] = 0;
	last_sent_report[1] = 0;
	last_sent_report[2] = 0;
	last_sent_report[3] = 0;
}
	




 
class DigiMouseDevice {
 public:
  DigiMouseDevice () {
	  /* configure timer 0 for a rate of 16M5/(1024 * 256) = 62.94 Hz (~16ms) */
	  TCCR0A = 5;      /* timer 0 prescaler: 1024 */

	
		
    TIMSK &= !(1<TOIE0);//interrupt off
    cli();
    usbDeviceDisconnect();
    _delay_ms(250);
    usbDeviceConnect();
	
	rt_usbHidReportDescriptor = mouse_usbHidReportDescriptor;
	rt_usbHidReportDescriptorSize = sizeof(mouse_usbHidReportDescriptor);
	rt_usbDeviceDescriptor = usbDescrDevice;
	rt_usbDeviceDescriptorSize = sizeof(usbDescrDevice);


    usbInit();
      
    sei();
  }
    
  void update() {
    usbPoll();
	
	
	if(TIFR & (1<<TOV0)){   /* 16 ms timer */
			TIFR = 1<<TOV0;
			if(idleRate != 0){
				if(idleCounter > 3){
					idleCounter -= 4;   /* 16 ms in units of 4 ms */
				}else{
					idleCounter = idleRate;
					must_report = 1;
				}
			}
	}
	
	if(memcmp(last_built_report, last_sent_report, REPORT_SIZE)) must_report = 1;
	
	if(must_report)
		{
			if (usbInterruptIsReady())
			{ 	
				must_report = 0;
				buildReport(reportBuffer);//put data into reportBuffer
				clearMove();//clear deltas
				usbSetInterrupt(reportBuffer, REPORT_SIZE);
			}
	}
	
  }
  
  void moveX(char deltaX)
  {
	  if (deltaX == -128) deltaX = -127;
	  last_built_report[1] = *(reinterpret_cast<unsigned char *>(&deltaX));
  }
  void moveY(char deltaY)
  {
	  if (deltaY == -128) deltaY = -127;
	  last_built_report[2] = *(reinterpret_cast<unsigned char *>(&deltaY));
  }
  void scroll(char deltaS)
  {
	  if (deltaS == -128) deltaS = -127;
	  last_built_report[3] = *(reinterpret_cast<unsigned char *>(&deltaS));  
  }
  
  void move(char deltaX, char deltaY, char deltaS)
  {
	  if (deltaX == -128) deltaX = -127;
	  if (deltaY == -128) deltaY = -127;
	  if (deltaS == -128) deltaS = -127;
	  last_built_report[1] = *(reinterpret_cast<unsigned char *>(&deltaX));
	  last_built_report[2] = *(reinterpret_cast<unsigned char *>(&deltaY));
	  last_built_report[3] = *(reinterpret_cast<unsigned char *>(&deltaS));
  }  
  void setButtons(unsigned char buttons)
  {
	  last_built_report[0] = buttons;
  }
  
	  
    
  void setValues(unsigned char values[]) {
	  
    memcpy(last_built_report, values, REPORT_SIZE);
  }
  

  
    
  //private: TODO: Make friend?
     

};

DigiMouseDevice DigiMouse = DigiMouseDevice();





#ifdef __cplusplus
extern "C"{
#endif 
  // USB_PUBLIC uchar usbFunctionSetup
  
 
		
  uchar	usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (usbRequest_t *)data;

	usbMsgPtr = reportBuffer;
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
		if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
			/* we only have one report type, so don't look at wValue */
			//curGamepad->buildReport(reportBuffer);
			//return curGamepad->report_size;
			return REPORT_SIZE;
		}else if(rq->bRequest == USBRQ_HID_GET_IDLE){
			usbMsgPtr = &idleRate;
			return 1;
		}else if(rq->bRequest == USBRQ_HID_SET_IDLE){
			idleRate = rq->wValue.bytes[1];
		}
	}else{
	/* no vendor specific requests implemented */
	}
	return 0;
}

uchar   usbFunctionDescriptor(struct usbRequest *rq)
{
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_STANDARD)
		return 0;

	if (rq->bRequest == USBRQ_GET_DESCRIPTOR)
	{
		// USB spec 9.4.3, high byte is descriptor type
		switch (rq->wValue.bytes[1])
		{
			case USBDESCR_DEVICE:
				usbMsgPtr = rt_usbDeviceDescriptor;
				return rt_usbDeviceDescriptorSize;

			case USBDESCR_HID_REPORT:
				usbMsgPtr = rt_usbHidReportDescriptor;
				return rt_usbHidReportDescriptorSize;

		}
	}

	return 0;
}


  
  

#ifdef __cplusplus
} // extern "C"
#endif


#endif // __DigiKeyboard_h__
