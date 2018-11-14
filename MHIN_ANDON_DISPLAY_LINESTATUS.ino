
#include <EEPROM.h>
#include <digitalWriteFast.h>
#include <TimerOne.h>
#include <SPI.h>
#include <DMDMegaMulti.h>
#include <Arial_black_16.h>
#include "config.h"


#define __SERIAL_DEBUG__

#define DISPLAYS_ACROSS 12
#define DISPLAYS_DOWN 1
#define DISPLAYS_BPP 1
#define WHITE 0xFF
#define BLACK 0
#define STATICMSGBUFFER_SIZE  25

#define STATION_LINE 2

#define __BCC_XOR__

//#define __DEVICE_UNDER_TEST__
//#define __FACTORY_CONFIGURATION__

enum
{
	PARSE_IGNORE = 0,
	PARSE_SUCCESS = 1,
	PARSE_FAILURE = 2
};

enum
{
	COM_DEVICE_ADDRESS_INDEX = 0,
	COM_TX_CODE_INDEX = 1,
	COM_DISPLAY_CODE_INDEX = 2,
	COM_TX_DATA_START_INDEX = 3,
	COM_RX_DATA_START_INDEX = 2
};
//state for communication
enum
{
	COM_RESET = 0,
	COM_START = 1,
	COM_IN_PACKET_COLLECTION = 2,
	COM_IN_PARSE_DATA = 3
};

#define BUFFSIZE 25*40

typedef struct _COMM
{
	UINT8 state;
	UINT8 prevState;

	UINT8 rxData[BUFFSIZE];
	UINT8 rxDataIndex;
	UINT8 txData[BUFFSIZE];

}COMM;

DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);

typedef enum
{

	IDLE= 0,
	GRN = 1,
	YEL = 2,
	GRN_YEL = 3,
	RED = 4,
	GRN_RED = 5,
	YEL_RED = 6,
	GRN_YEL_RED = 7,
	DIGIT = 8
	
}STATION_STATE;

typedef struct LINE
{
	UINT8 ID[3];
	STATION_STATE State;
	STATION_STATE PrevState;
	int GREEN;
	int YELLOW;
	int RED;
	UINT8 ND1;
	UINT8 Offset1;
	UINT8 ND2;
	UINT8 Offset2;

	bool StateChanged;

};



#if STATION_LINE == 1

#define DISPLAY_ADDRESS 0x01
#define MAX_STATIONS 9
#define ID_LENGTH 2
UINT8* RESOLVED_ID = (UINT8*)" ";


LINE LINES[MAX_STATIONS + 1] = {
	{},
	{ "1",IDLE,IDLE,A0,A1,A2,5,12,6,12 },
	{ "2",IDLE,IDLE,A3,A4,A5,4,22,7,2 },
	{ "3",IDLE,IDLE,A6,A7,A8,4,2,7,22 },
	{ "4",IDLE,IDLE,A9,A10,A11,3,10,8,10 },
	{ "5",IDLE,IDLE,A12,A13,A14,2,22,9,2 },
	{ "6",IDLE,IDLE,A15,48,46,2,2,9,22 },
	{ "7",IDLE,IDLE,44,42,40,1,12,10,12 },
	{ "8",IDLE,IDLE,38,36,34,0,22,11,2 },
	{ "9",IDLE,IDLE,32,30,28,0,2,11,22 }

};



char StationStatusIndication[DISPLAYS_ACROSS * 3 + 1];


#elif  STATION_LINE == 2

#define ID_LENGTH 3
#define DISPLAY_ADDRESS 0x02
#define MAX_STATIONS 6

UINT8* RESOLVED_ID = (UINT8*)"  ";



LINE LINES[MAX_STATIONS + 1] = {
	{},
	{ "10",IDLE,IDLE,A0,A1,A2,5,8,6,8 },
	{ "11",IDLE,IDLE,A3,A4,A5,4,8,7,8 },
	{ "12",IDLE,IDLE,A6,A7,A8,3,8,8,8 },
	{ "13",IDLE,IDLE,A9,A10,A11,2,8,9,8 },
	{ "14",IDLE,IDLE,A12,A13,A14,1,8,10,8 },
	{ "15",IDLE,IDLE,48,46,44,0,8,11,8 },
	

};


#define HOOTER_DURATION 10000
const int HooterControl = 42;

unsigned char SWITCH_ON_HOOTER = 0;

char StationStatusIndication[DISPLAYS_ACROSS * 3 + 1];

#endif // STATION_LINE_1

COMM comm = { 0 };
char SOP = 0xAA;
char EOP = 0xBB;

UINT32 curAppTime = 0;
UINT32 prevAppTime = 0;
UINT8 timeout = 500;


UINT8 StationStatus[MAX_STATIONS * ID_LENGTH+1] ="" ;

/*--------------------------------------------------------------------------------------
Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
called at the period set in Timer1.initialize();
--------------------------------------------------------------------------------------*/
void ScanDMD()
{
	dmd.scanDisplayBySPI();
}

void setup()
{


char stnInfo[200];


	Timer1.initialize(2000 / DISPLAYS_BPP);           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
	Timer1.attachInterrupt(ScanDMD);   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()

									   //clear/init the DMD pixels held in RAM
	dmd.selectFont(Arial_Black_16);   // BOLD FONT

	dmd.clearScreen(WHITE);
	
	
	

  /* add setup code here */
	pinMode(A0, OUTPUT); 
	pinMode(A1, OUTPUT); 
	pinMode(A2, OUTPUT); 
	pinMode(A3, OUTPUT); 
	pinMode(A4, OUTPUT); 
	pinMode(A5, OUTPUT); 
	pinMode(A6, OUTPUT); 
	pinMode(A7, OUTPUT); 
	pinMode(A8, OUTPUT); 
	pinMode(A9, OUTPUT); 
	pinMode(A10, OUTPUT);
	pinMode(A11, OUTPUT);
	pinMode(A12, OUTPUT);
	pinMode(A13, OUTPUT);
	pinMode(A14, OUTPUT);
	pinMode(A15, OUTPUT);

	pinMode(48, OUTPUT); 
	pinMode(46, OUTPUT); 
	pinMode(44, OUTPUT); 
	pinMode(42, OUTPUT); 
	pinMode(40, OUTPUT); 
	pinMode(38, OUTPUT); 
	pinMode(36, OUTPUT); 
	pinMode(34, OUTPUT); 
	pinMode(32, OUTPUT); 
	pinMode(30, OUTPUT); 
	pinMode(28, OUTPUT); 
	pinMode(26, OUTPUT); 

	//POST();
	

	dmd.clearScreen(WHITE);

	COM_init();

#ifdef __SERIAL_DEBUG__
	Serial.println("Application Started");
	Serial1.println("Application Started");
	Serial2.println("Application Started");
	for (int i = 1; i < MAX_STATIONS + 1; i++)
	{

		sprintf(stnInfo, "ID:%s\tG:%d\tY:%d\tR:%d\tN1:%d\tO1:%d\tN2:%d\tO2:%d\n",
			(const char*)LINES[i].ID, LINES[i].GREEN, LINES[i].YELLOW, LINES[i].RED, LINES[i].ND1, LINES[i].Offset1,
			LINES[i].ND2, LINES[i].Offset2);

		Serial.println(stnInfo);
	}
#endif
}

int stationIndex = 1;
bool dir = true;

void loop()
{

	int i, j;
	int lineID;
	if (ComService() == true)
	{
  #ifdef __SERIAL_DEBUG__
  Serial.println("Data Received");
  #endif
		switch (comm.rxData[0])
		{
		case 0x90:
  #ifdef __SERIAL_DEBUG__
		Serial.println("COMMAND : Update Line State");
  #endif      
			
#if STATION_LINE == 1
		lineID = comm.rxData[1];
		if ((STATION_STATE)comm.rxData[2] != LINES[lineID].PrevState)
				{
					LINES[lineID].PrevState = LINES[lineID].State;
					LINES[lineID].State = (STATION_STATE)comm.rxData[2];
					LINES[lineID].StateChanged = true;
				}
#ifdef __SERIAL_DEBUG__
		Serial.print("Line:");
		Serial.println(lineID);
		Serial.print("State:");
		Serial.println(LINES[lineID].State);
#endif       


#else
		lineID = comm.rxData[1];

		if (lineID == 99)
		{
			digitalWrite(HooterControl, HIGH);
			delay(HOOTER_DURATION);
			digitalWrite(HooterControl, LOW);
			return;
		}

		lineID -= 9;
		if ((STATION_STATE)comm.rxData[2] != LINES[lineID].PrevState)
		{
			LINES[lineID].PrevState = LINES[lineID].State;
			LINES[lineID].State = (STATION_STATE)comm.rxData[2];
			LINES[lineID].StateChanged = true;
		}
#ifdef __SERIAL_DEBUG__
		Serial.print("Line:");
		Serial.println(lineID);
		Serial.print("State:");
		Serial.println(LINES[lineID].State);
#endif       
			
#endif		
	
		break;

		case 0x99:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : POST");
#endif      
			POST();
			break;
		}
	}

	updateIndication();

#ifdef __DEVICE_UNDER_TEST__

	for (int i = 1; i <= stationIndex; i++)
	{
		Stations[i].State = (dir == true) ? RAISED:0;
	}
	delay(1500);
	stationIndex++;
	if (stationIndex > MAX_STATIONS)
	{
		stationIndex = 1;
		dir  = !dir;
	}

#endif // __DEVICE_UNDER_TEST__
}

/*---------------------------------------------------------------------------------
void updateIndication(void)
--------------------------------------------------------------------------------*/
void updateIndication(void)
{
	UINT8 i;


#ifdef __SERIAL_DEBUG__

	//Serial.println("Updating Indication");

#endif

	for (i = 1; i < MAX_STATIONS + 1; i++)
	{
		if (LINES[i].StateChanged)
		{
			switch (LINES[i].State)
			{
			case 0:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)RESOLVED_ID, strlen((const char*)RESOLVED_ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)RESOLVED_ID, strlen((const char*)RESOLVED_ID), GRAPHICS_NORMAL);

				digitalWrite(LINES[i].RED, LOW);
				digitalWrite(LINES[i].YELLOW, LOW);
				digitalWrite(LINES[i].GREEN, LOW);
				break;
			case 1:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, LOW);
				digitalWrite(LINES[i].YELLOW, LOW);
				digitalWrite(LINES[i].GREEN, HIGH);
				break;

			case 2:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, LOW);
				digitalWrite(LINES[i].YELLOW, HIGH);
				digitalWrite(LINES[i].GREEN, LOW);
				break;

			case 3:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, LOW);
				digitalWrite(LINES[i].YELLOW, HIGH);
				digitalWrite(LINES[i].GREEN, HIGH);
				break;

			case 4:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, HIGH);
				digitalWrite(LINES[i].YELLOW, LOW);
				digitalWrite(LINES[i].GREEN, LOW);
				break;
			case 5:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, HIGH);
				digitalWrite(LINES[i].YELLOW, LOW);
				digitalWrite(LINES[i].GREEN, HIGH);
				break;

			case 6:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, HIGH);
				digitalWrite(LINES[i].YELLOW, HIGH);
				digitalWrite(LINES[i].GREEN, LOW);
				break;

			case 7:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].RED, HIGH);
				digitalWrite(LINES[i].YELLOW, HIGH);
				digitalWrite(LINES[i].GREEN, HIGH);
				break;

			case 8:
				dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
				digitalWrite(LINES[i].GREEN, LOW);
				digitalWrite(LINES[i].RED, LOW);
				digitalWrite(LINES[i].YELLOW, LOW);
				break;


			default:
				break;
			}
		}

#if STATION_LINE == 2
		if (SWITCH_ON_HOOTER)
		{
			digitalWrite(HooterControl, HIGH);
			delay(HOOTER_DURATION);
			digitalWrite(HooterControl, LOW);
		}
#endif	
	}

	
}

void POST(void)
{
  
#if STATION_LINE == 2
  digitalWrite(HooterControl, HIGH); 
  delay(1000);
  digitalWrite(HooterControl, LOW);
#endif
  
	for (int i = 1; i < MAX_STATIONS + 1; i++)
	{
			
		dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
		dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)LINES[i].ID, strlen((const char*)LINES[i].ID), GRAPHICS_NORMAL);
			
		digitalWrite(LINES[i].GREEN, HIGH);
		delay(500);
		digitalWrite(LINES[i].YELLOW, HIGH);
		delay(500);
		digitalWrite(LINES[i].RED, HIGH);
                delay(500);

		
	}
	delay(60000);

	for (int i = 1; i < MAX_STATIONS + 1; i++)
	{

		dmd.drawString(LINES[i].ND1 * 32 + LINES[i].Offset1, 0, (const char*)RESOLVED_ID, strlen((const char*)RESOLVED_ID), GRAPHICS_NORMAL);
		dmd.drawString(LINES[i].ND2 * 32 + LINES[i].Offset2, 0, (const char*)RESOLVED_ID, strlen((const char*)RESOLVED_ID), GRAPHICS_NORMAL);

		digitalWrite(LINES[i].GREEN, LOW);
		delay(500);
		digitalWrite(LINES[i].YELLOW, LOW);
		delay(500);
		digitalWrite(LINES[i].RED, LOW);
		delay(500);
	}

  
  
}





#pragma region Communication





void COM_init()
{
	Serial.begin(9600);
	Serial1.begin(9600);
	Serial2.begin(57600);
	Serial3.begin(57600);
}

/*--------------------------------------------------------------------------------
bool ComService(void)
--------------------------------------------------------------------------------**/
bool ComService(void)
{
	bool result = false;
	volatile char serialData = 0;
	curAppTime = millis();

	unsigned char i, j;

	if (prevAppTime != curAppTime)
	{
		if (comm.prevState == comm.state && (comm.state == COM_IN_PACKET_COLLECTION))
		{
			--timeout;
			if (timeout == 0)
			{
				ComReset();
				return result;
			}

		}

		prevAppTime = curAppTime;
	}
	switch (comm.state)
	{
	case COM_START:

		if (Serial1.available() > 0)
		{
			serialData = Serial1.read();
			if (serialData == SOP)
			{
				comm.rxDataIndex = 0;
				comm.state = COM_IN_PACKET_COLLECTION;
			}
		}
		break;
	case COM_IN_PACKET_COLLECTION:

		if (Serial1.available() > 0)
		{
			serialData = Serial1.read();
			if (serialData == EOP)
			{
				char parseResult = 0;
				parseResult = ParsePacket();

				switch (parseResult)
				{
				case PARSE_IGNORE:
					ComReset();
					//Serial2.write(comm.rxData, comm.rxDataIndex);

					break;

				case PARSE_SUCCESS:
					result = true;
					break;

				default:
					break;

				}
				comm.state = COM_START;
			}
			else
			{
				comm.rxData[comm.rxDataIndex] = serialData;
				comm.rxDataIndex++;
			}

		}
		break;
	case COM_RESET:

		ComReset();
		break;

	default:
		ComReset();
		break;
	}
	comm.prevState = comm.state;
	return result;
}


/*--------------------------------------------------------------------------------
void ComReset(void)
--------------------------------------------------------------------------------*/
void ComReset(void)
{
	comm.rxDataIndex = 0;
	comm.state = COM_START;
}
/*--------------------------------------------------------------------------------
char ParsePacket(void)
--------------------------------------------------------------------------------*/
char ParsePacket(void)
{
	char receivedChecksum = comm.rxData[comm.rxDataIndex - 1];
	char genChecksum = 0;

	//Serial.println("inside parse");
	//	if (comm.rxData[COM_DISPLAY_CODE_INDEX] != DISPLAY_ADDRESS)
	//	return PARSE_IGNORE;
	//else 
	return PARSE_SUCCESS;
	/*
	genChecksum = checksum(comm.rxData, comm.rxDataIndex - 1);
	//Serial.println(genChecksum,HEX);
	//Serial.println(receivedChecksum,HEX);
	if (receivedChecksum == genChecksum)
	{
	--comm.rxDataIndex;
	comm.rxData[comm.rxDataIndex] = '\0'; //remove checksum from packet

	return PARSE_SUCCESS;
	}
	else
	{
	//*respCode = COM_RESP_CHECKSUM_ERROR;
	return PARSE_FAILURE;
	}
	*/
}

/*---------------------------------------------------------------------------------
unsigned char checksum(unsigned char *buffer, unsigned char length)

summry: take bufer value and give checksum value

--------------------------------------------------------------------------------*/
unsigned char checksum(unsigned char *buff, unsigned char length)
{

	char bcc = 0;
	char i, j;

#ifdef __BCC_LRC__

	for (i = 0; i < length; i++)
	{
		bcc += buff[i];
	}
	return bcc;

#elif defined __BCC_XOR__

	for (i = 0; i < length; i++)
	{
		bcc ^= buff[i];
	}
	return bcc;

#endif
}


#pragma endregion




