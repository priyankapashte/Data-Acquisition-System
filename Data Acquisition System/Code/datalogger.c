#include <at89c51ed2.h>  //also includes 8052.h and 8051.h
#include <mcs51reg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  unsigned short reserved_sectors;
  unsigned short fat_size_sectors;
  unsigned short root_dir_entries;
  unsigned char number_of_fats;
  unsigned char sector_size;
} __attribute((packed)) Fat16BootSector;

typedef struct
{
  unsigned char filename[8];
  unsigned char attributes;
  unsigned short starting_cluster;
  unsigned short root_dir_entries;
  unsigned long file_size;
} __attribute((packed)) Fat16Entry;
 

/********************* LCD Define *************************/
#define RS         P3_4          // Register Select
#define RW         P3_5          // Read write
#define BUSY       0x80          // To check the Busy bit on D7
#define ADDRESS_COUNTER ((xdata unsigned char *) 0x8000)
/**********************************************************/

/****************** SD Card Define ************************/
#define SS P1_1					// Chip Select
#define SD_CS P1_2
#define MISO P1_5				// Master In Slave Out
#define MOSI P1_7				// Master Out Slave In
#define SCK P1_6				// Serial Clock
/**********************************************************/

/********************* I2C Define *************************/
#define SCL P1_3				// Serial Clock
#define SDA P1_4				// Serial Data
#define SLAVE_WRITE 0x4E		// Slave Address + Write Bit
#define SLAVE_READ 0x4F			// Slave Address + Read Bit
/**********************************************************/

#define ultra P1_0				// Ultrasonic Input received at P1_0

#define ASCII_BASE 0x30			// Ascii Value of '0'

/**************************** Global variables *****************************************/
xdata unsigned char *address_ptr;                                                                    // Pointer to external data memory
char r='0',c='0',num=0,input1,input2,input3;
unsigned char packet[7],bytel,byteh,input4,temp_byte[7];
unsigned char col1[10],col2,disp_char,addr[10],address[10],m[2],sl,d[2],pos=0x80,col;
int cursor,cursor_check=1,address_check=1,cg=0,month,day,t=0,time,distance;
unsigned int sensor[4],humidity,temperature,tempC,rel_humidity,sensor_read,match=0;
volatile unsigned char serial_data;
unsigned char data_save;
bit transmit_completed= 0;
static  unsigned char CRC=0x95;

xdata unsigned long temp=0;
xdata unsigned short shorttm=0;
xdata unsigned long nofbytes=0;
xdata unsigned long starting_cluster=0;
xdata unsigned long i=0,j=0;
xdata unsigned short clusterbyte=0x0000;
xdata unsigned long addr=0x000000;
	

 Fat16BootSector bs;
 Fat16Entry entry;

/***************************************************************************************/

/********************************* Function Declaration ********************************/
void clear_scr(void);									// Clear the Terminal Screen
void serial_init(void);									// Initialize Serial Terminal
void isr_init();										// Initialize Timer 0 for distance calculation
void putchar (char c);									// Display a character on the terminal
char getchar();											// Receive a charcter from the terminal
void wait_ms(unsigned int time);						// Delay of 1 ms
void lcdbusywait();										// Check for the busy flag of LCD
void lcdinit();											// Initaialize LCD
void lcd_command(char command);							// Write a command to the LCD
void lcdgotoaddr(unsigned char address);				// Go to a specific address on the LCD 
void lcdgotoxy(int row, int column);					// Go to a LCD location using co-ordinates
void lcdputch(char cc);									// Display a character on LCD
void lcdputstr(char *ss);								// Display a string on LCD
void start();											// I2C start condition
void stop();											// I2C stop condition
unsigned char ack();									// Acknowledge
void write(unsigned char data1);						// I2C write
unsigned char read(unsigned char status);				// I2C Read
void measure_req();										// Send measurement request
void data_fetch();										// Send Data Fetch Request
void humidity_temp();									// Calculate humidity and temperature
void ultrasonic();										// Calculate distance
void spi_ready();										// Get SPI out of IDLE Mode
void spi_init();										// Initialize SPI registers
void CMD(unsigned char cmd);							// Send Command 0 and Command 1
void CMDx(unsigned char cmd,unsigned long addr);		// Send any command
void delay_spi(int time);								// Delay for 1 ms
void write_spi( unsigned char byte);					// Write a byte SPI
int response(void);										// SPI Read Response
void fat_init();										// Initialize for FAT File System
void skip_bytes(int num);								// Skip bytes
/***************************************************************************************/

/************************ External XRAM ****************************/
_sdcc_external_startup()
{
    AUXR |= 0x0C;        					//Internal XRAM set to size of 1KB by setting XRS1 and XRS0 bits to 1.
    return 0;
}
/*******************************************************************/


/************************ Main *************************************/
void main()
{
    int i,j;
    unsigned char byte2;
	   
	serial_init();
    isr_init();
    lcdinit();
    fat_init();
    
	printf_tiny("\n\r------------------------------------------");
    printf_tiny("\n\rEmbedded Systems Design Final Project ");
    printf_tiny("\n\r");
    printf_tiny("\n\r           DATA LOGGER");
    printf_tiny("\n\r------------------------------------------");
    printf_tiny("\n\r");
   
    while(1)
    {
        lcd_command(0x01);												// Clear LCD Screen
        printf_tiny("\n\rPress 1 or 2 to perform the following:");
        printf_tiny("\n\r1. Acquire data from the sensors");
        printf_tiny("\n\r2. Read sensor data from SD Card");
        lcdgotoxy(0,3);													
        lcdputstr("Data Logger");
        lcdgotoxy(2,0);
        lcdputstr("1. Acquire Data");
        lcdgotoxy(3,0);
        lcdputstr("2. Read Data");
        printf_tiny("\n\r");
        input1=getchar();												// Receive input from terminal

        if(input1=='1'||input1=='2')									// Check the input received
        {
            lcd_command(0x01);											// Clear LCD Screen
            lcdgotoxy(0,3);
            lcdputstr("Data Logger");
            lcdgotoxy(2,0);
            lcdputstr("Input received:");
            lcdputch(input1);
            wait_ms(1000);
            if(input1=='1')
            {
                num++;
                printf_tiny("\n\r");
                printf_tiny("\n\r------------------------------------------");
                printf_tiny("\n\r        Sensor Data Acquisition");
                printf_tiny("\n\r------------------------------------------");
                printf_tiny("\n\r");
                lcd_command(0x01);													// Clear LCD Screen
                lcdgotoxy(0,2);
                lcdputstr("Acquire Data");
                lcdgotoxy(1,0);
                lcdputstr("Enter date:");
                while(1)
                {
                    lcdgotoxy(2,0);
                    printf_tiny("\n\rEnter the month and day in MM/DD format");
                    printf_tiny("\n\r");
                    for(i=0;i<2;i++)												// Receive two characters for the month
                    {
                        m[i]=getchar();		
                        putchar(m[i]);
                        lcdputch(m[i]);
                    }
                    sl=getchar();
                    putchar(sl);
                    lcdputch(sl);
                    for(i=0;i<2;i++)												// Receive two characters for the month
                    {
                        d[i]=getchar();
                        putchar(d[i]);
                        lcdputch(d[i]);
                    }
                    month=atoi(m);													//Convert the month and day values from ascii to integer
                    day=atoi(d);
                    if(month<=0 || month>12 || day<0 || day>31)						// Check for valid date input
                    {
                        printf_tiny("\n\rPlease enter a valid date.");
                        lcd_command(0x01);
                        lcdgotoxy(0,2);
                        lcdputstr("Acquire Data");
                        lcdgotoxy(1,0);
                        lcdputstr("Enter date:");
                    }
                    else															// If not a valid date ask again
                    {
                        printf_tiny("\n\rDate set to %d/%d",month,day);
                        lcdgotoxy(3,0);
                        lcdputstr("Date set:");
                        lcdputch((month/10)+0x30);
                        lcdputch((month%10)+0x30);
                        lcdputch('/');
                        lcdputch((day/10)+0x30);
                        lcdputch((day%10)+0x30);
                        break;
                    }
                }
                wait_ms(2000);
                while(1)
                {
                    printf_tiny("\n\rSelect a sensor:");
                    printf_tiny("\n\r1. Temperature Sensor");
                    printf_tiny("\n\r2. Humidity Sensor");
                    printf_tiny("\n\r3. Ultrasonic Sensor");
                    printf_tiny("\n\r");
                    lcd_command(0x01);
                    lcdgotoxy(0,1);
                    lcdputstr("Select Sensor");
                    lcdgotoxy(1,0);
                    lcdputstr("1. Temperature");
                    lcdgotoxy(2,0);
                    lcdputstr("2. Humidity");
                    lcdgotoxy(3,0);
                    lcdputstr("3. Ultrasonic");

                    input2=getchar();																// Receive sensor number
                    if(input2=='1' || input2=='2' || input2=='3')									// Check for the sensor number received
                    {
                        lcd_command(0x01);															// Clear LCD Screen
                        lcdputstr("Input Received:");
                        lcdputch(input2);
                        wait_ms(1000);
                        switch(input2)
                           {
                                case '1':
                                {
                                    printf_tiny("\n\rAcquiring Temperature Sensor Data...");
                                    humidity_temp();
                                    break;
                                }
                                case '2':
                                {
                                    printf_tiny("\n\rAcquiring Humidity Sensor Data...");
                                    humidity_temp();
                                    break;
                                }
                                case '3':
                                {
                                    printf_tiny("\n\rAcquiring Ultrasonic Sensor Data...");
                                    ultrasonic();
                                    break;
                                }
                           }
                         wait_ms(4000);
                     
                        printf_tiny("\n\r STORING THE VALUE IN SD CARD..."); 							// Create a packet array to be stored in the memory card
                        packet[0]=input2;																// Sensor number
                        packet[1]=month/10;																// Month high byte
                        packet[2]=month%10;																// Month low byte
                        packet[3]=day/10;																// Day high byte
                        packet[4]=day%10;																// Day low byte
                        packet[5]=byteh;																// Sensor data high byte
                        packet[6]=bytel;																// Sensor data low byte
						packet[7]=0x20;																
                    
					/************************  STORING THE VALUE IN SD CARD *****************************/
						 
						 addr= 0x50200 + (starting_cluster-2)*cluster_size + nofbytes; 		// 0x1200000 found out from the hex dump of the SD card
                         if(nofbytes > cluster_size) 										// If number of bytes exceeds cluster size i.e. 16KB
						  { 
            				addr=0x50200 + (starting_cluster-2)*cluster_size;
						    nofbytes=0;
						  }
						 CMDx(24,addr);  													// Same function as fseek  
                         r=response();
                         write_spi(0xFE);
                         for(i=0;i<8;i++)
                           write_spi(packet[i]);

                         write_spi(CRC);
                         write_spi(CRC);

                         delay_spi(20);
                         while((r=response())!=2);
						 nofbytes+=7; 													    // Increasing the number of bytes
                         printf_tiny("Packet written!!!! \r\n");
                         delay_spi(100);
                         break;                												// Select a sensor while loop break
                  }
                else
                    printf_tiny("\n\rPlease enter a valid input (1,2 or 3)");
            }
        }
        else if(input1=='2')
        {
            printf_tiny("\n\r");
            printf_tiny("\n\r-----------------------------------");
            printf_tiny("\n\r  Read Sensor Data from SD Card");
            printf_tiny("\n\r-----------------------------------");
            printf_tiny("\n\r");
            while(1)
                {
                    printf_tiny("\n\rSelect a sensor:");
                    printf_tiny("\n\r1. Temperature Sensor");
                    printf_tiny("\n\r2. Humidity Sensor");
                    printf_tiny("\n\r3. Ultrasonic Sensor");
                    lcd_command(0x01);
                    lcdgotoxy(0,1);
                    lcdputstr("Select Sensor");
                    lcdgotoxy(1,0);
                    lcdputstr("1. Temperature");
                    lcdgotoxy(2,0);
                    lcdputstr("2. Humidity");
                    lcdgotoxy(3,0);
                    lcdputstr("3. Ultrasonic");
                    input4=getchar();
                    if(input2=='1' || input2=='2' || input2=='3')
                    {
                        lcd_command(0x01);
                        lcdputstr("Input Received:");
                        lcdputch(input4);
                        wait_ms(1000);
                        break;
                    }
                    else
                        printf_tiny("\n\rPlease enter a valid input (1,2 or 3)");
                }
               while(1)
                {
                    lcdgotoxy(2,0);
                    printf_tiny("\n\rEnter the month and day in MM/DD format");
                    printf_tiny("\n\r");
                    for(i=0;i<2;i++)
                    {
                        m[i]=getchar();
                        putchar(m[i]);
                        lcdputch(m[i]);
                    }
                    sl=getchar();
                    putchar(sl);
                    lcdputch(sl);
                    for(i=0;i<2;i++)
                    {
                        d[i]=getchar();
                        putchar(d[i]);
                        lcdputch(d[i]);
                    }
                    month=atoi(m);
                    day=atoi(d);
                    if(month<=0 || month>12 || day<0 || day>31)
                    {
                        printf_tiny("\n\rPlease enter a valid date.");
                        lcd_command(0x01);
                        lcdgotoxy(0,2);
                        lcdputstr("Acquire Data");
                        lcdgotoxy(1,0);
                        lcdputstr("Enter date:");
                    }
                    else
                    {
                        printf_tiny("\n\rDate set to %d/%d",month,day);
                        lcdgotoxy(3,0);
                        lcdputstr("Date set:");
                        lcdputch((month/10)+0x30);
                        lcdputch((month%10)+0x30);
                        lcdputch('/');
                        lcdputch((day/10)+0x30);
                        lcdputch((day%10)+0x30);
                        break;
                    }
                }
                wait_ms(2000);


            for(j=0;j<num;j++)
            {
                 spi_ready(); 														//Checking if SD card is in Ready mode
				 addr= 0x50200 + (starting_cluster-2)*cluster_size;  
                 CMDx(18,addr); 													// Starting from the beginning of the file
                   do
                    {
                        write_spi(0xFF); 											// Sending a dummy byte
                        byte2=serial_data;
                    }while(byte2!=0xFE); 											// Checking if the LSB from SD card response is 0 

                 for(i=0;i<7;i++)
                 {
                       write_spi(0xFF);  											// Sending a dummy byte
                       byte2=serial_data;
                       if(i==0 && byte2!=input4)
                            break;
                       if(i==1 && byte2!=(month/10))
                            break;
                        if(i==2 && byte2!=(month%10))
                            break;
                       if(i==3 && byte2!=(day/10))
                            break;
                       if(i==4 && byte2!=(day%10))
                            break;
                       temp_byte[i]=byte2;
                     //  printf("\n\rBYTE READ %02X \r\n",byte2);
                 }
               if(i==7)
                 {
                    lcd_command(0x01);												// Clear LCD Screen
                    match=1;														// When i=7, match is found
                    printf_tiny("\n\r--------------------------------");
                    printf_tiny("\n\r       Sensor Data Read");
                    printf_tiny("\n\r--------------------------------");
                    printf_tiny("\n\rSensor Input: %d",(temp_byte[0]-0x30));
                    lcdputstr("Sensor Input:");
                    lcdputch(temp_byte[0]);
                    printf_tiny("\n\r");
                    lcdgotoxy(1,0);
                    if(temp_byte[0]=='1')											// Check the sensor number in byte 0
                    {
                        printf_tiny("\n\rTemperature Sensor Data Acquisition");
                        lcdputstr("Temperature");
                    }
                    else if(temp_byte[0]=='2')
                    {
                        printf_tiny("\n\rHumidity Sensor Data Acquisition");
                        lcdputstr("Humidity");
                    }
                    else if(temp_byte[0]=='3')
                    {
                        printf_tiny("\n\rUltrasonic Sensor Data Acquisition");
                        lcdputstr("Ultrasonic");
                    }
                    printf_tiny("\n\r");
                    month=(temp_byte[1]*10)+temp_byte[2];							// Calculate month using the two individual bytes
                    day=(temp_byte[3]*10)+temp_byte[4];								// Calculate day using the two individual bytes
                    printf_tiny("\n\rDate: %d/%d",month,day);
                    lcdgotoxy(1,0);
                    lcdputstr("Date:");
                    lcdputch((month/10)+0x30);
                    lcdputch((month%10)+0x30);
                    lcdputch('/');
                    lcdputch((day/10)+0x30);
                    lcdputch((day%10)+0x30);
                    sensor_read=(temp_byte[5]*10)+temp_byte[6];						// Calculate the sensor value using the two individual bytes
                    printf_tiny("\n\rSensor Data: %d",sensor_read);
                    lcdgotoxy(2,0);
                    lcdputstr("Sensor Data:");
                    lcdputch((sensor_read/10)+0x30);
                    lcdputch((sensor_read%10)+0x30);
                    printf_tiny("\n\r-------------------------------");
                    printf_tiny("\n\r");
                    wait_ms(2000);
                    break;
                 }
                 write_spi(0xFF);
                 write_spi(0xFF);
                 addr++;															// Increment the memory block address
            }
        if(match==0 || addr == cluster_size)										// If end of cluster has been reached
            printf_tiny("\n\rMatch not found.");
        }

    }
    else																					// Ask for a valid input
        printf_tiny("\n\rPlease enter a valid input (either 1 or 2)");
    }

}

/************************** Clear Terminal *****************************/

void clear_scr(void)
{
    printf_tiny("\033c");        	//VT100 Reset Terminal to initial state
    printf_tiny("\033[2J");        	//VT100 Clear Screen
    printf_tiny("\033H");        	//VT100 Cursor Set to Home
}

void serial_init(void)
{
    SCON = 0x50;            		//SCON register set to serial mode 1 and receiver enabled
    TMOD |= 0x20;            		//Set Timer1 to 8 bit auto reload mode
    TH1 = 0xFD;                		//Timer 1 register set for achieving a baud rate of 9600bps with SMOD = 1
    TR1=1;                  		//Start Timer 1
}

void putchar (char c)
{
    SBUF = c;                  		// Load serial port with transmit value
    while (!TI);            		// Compare asm code generated for these three lines
    TI = 0;                  		// Clear TI flag
}

char getchar ()
{
    while (!RI);            		// Compare asm code generated for these three lines
    RI = 0;                    		// Clear RI flag
    return SBUF;            		// Return character from SBUF
}

void wait_ms(unsigned int j)
{
    unsigned int i,k;
    for(i=0;i<j;i++)                //1ms delay
        for(k=0;k<0xFF;k++);
}

/************************** Poll the LCD busy flag *********************************/

void lcdbusywait()
{
    RS = 0;                                 // Register Select high for instruction operation
    RW = 1;                                 // R/W high for read
    address_ptr = ADDRESS_COUNTER;          // Setup global pointer to address F000
    while ((*address_ptr & BUSY) !=0);      // Wait till Busy flag is 1
}

void lcd_command(char command)
{
    lcdbusywait();
    RS = 0;                                 // Register Select low for instruction operation
    RW = 0;                                 // R/W low for write
    address_ptr = ADDRESS_COUNTER;          // Setup global pointer to address 8000
    *address_ptr = command;                 // Write the command
}

void lcdinit()
{
    wait_ms(50);                			//Wait more than 15ms
    lcd_command(0x30);          			// Initialize for 8 bit , 2 line
    wait_ms(10);                 			// Wait more than 4.1ms
    lcd_command(0x30);          			// Initialize for 8 bit , 2 line
    wait_ms(5);                 			// Wait more than 100us
    lcd_command(0x30);          			// Initialize for 8 bit , 2 line
    lcd_command(0x38);          			// Function Set Command: N=1, F=0 for DMC20480
    lcd_command(0x08);          			// Turn the display OFF
    lcd_command(0x0C);          			// Turn the display ON
    lcd_command(0x06);          			// Entry Mode Set command
    lcd_command(0x01);          			// Clear screen and send the cursor home
}

/**************** Set cursor to specified LCD DDRAM address **************************/

void lcdgotoaddr(unsigned char addr)
{
    lcd_command(addr);       				// Set valid DDRAM address according to user input
}


/*** Set cursor to LCD DDRAM address corresponding to specified row and column *******/

void lcdgotoxy(int row, int column)
{
    if(column==0)
        column=0x00;
     else if(column==1)
        column=0x01;
    else if(column==2)
        column=0x02;
    else if(column==3)
        column=0x03;
    else if(column==4)
        column=0x04;
    else if(column==5)
        column=0x05;
    else if(column==6)
        column=0x06;
    else if(column==7)
        column=0x07;
    else if(column==8)
        column=0x08;
    else if(column==9)
        column=0x09;
    else if(column==10)
        column=0x0a;
    else if(column==11)
        column=0x0b;
    else if(column==12)
        column=0x0c;
    else if(column==13)
        column=0x0d;
    else if(column==14)
        column=0x0e;
    else if (column==15)
        column=0x0f;
    if(row==0)
        pos=0x80+ column;        //Position = Row address + column
    else if(row==1)
        pos=0xC0+column;
    else if(row==2)
        pos=0x90+column;
    else if(row==3)
        pos=0xD0+column;
    lcdgotoaddr(pos);
}

/*********** Writes the specified character to the current  LCD cursor position ***********/

void lcdputch(char cc)
{
    lcdbusywait();                          // Check whether LCD is ready or busy?
    RS = 1;                                 // Register Select high for data operation
    RW = 0;                                 // R/W low for write
    address_ptr = ADDRESS_COUNTER;          // Setup global pointer to address 8000
    *address_ptr = cc;                      // Write the value
}

void lcdputstr(char *ss)
{
    int i, len;
    len = strlen(ss);                    	// Store string length in variable count
    for (i = 0; i < len; i++)            	// Loop till the all the characters are displayed
    {
        if(i == 16)
           lcd_command(0xC0);               // Set DDRAM address to 2nd line
        else if(i == 32)
           lcd_command(0x90);               // Set DDRAM address to 3rd line
        else if(i == 48)
           lcd_command(0xD0);               // Set DDRAM address to 4th line
        lcdputch(ss[i]);
    }
}

void start()						// Issue Start Condition
{
    SDA=1;
    _asm
        nop
        nop
    _endasm;

    SCL=1;
    _asm
        nop
        nop
        nop
    _endasm;

    SDA=0;
    _asm
        nop
        nop
        nop
        nop
        nop
    _endasm;

    SCL=0;
    _asm
        nop
        nop
        nop
    _endasm;
}

void stop()						// Issue Stop Condition
{
    SDA=0;
    _asm
        nop
        nop
    _endasm;

    SCL=1;
    _asm
        nop
        nop
        nop
        nop
        nop
    _endasm;

    SDA=1;
    _asm
        nop
        nop
        nop
    _endasm;

    SCL=0;
    _asm
        nop
        nop
        nop
    _endasm;
}

unsigned char ack()
{
    unsigned char ack_bit;

    SCL = 1;
    _asm
        nop
        nop
        nop
        nop
        nop
    _endasm;

    ack_bit = SDA;                // Check the status of the SDA pin

    SCL = 0;                      // Complete a clock cycle by clearing SCL

    return (ack_bit);             // Return the bit value that was received on SDA
}

void write(unsigned char data1)
{
    char i;
    unsigned char temp;

    for (i = 0; i < 8; i++)
    {
        temp = data1 & 0x80;
        if (temp == 0)            // MSB = 0
            SDA = 0;              // then SDA = 0
        else
            SDA = 1;              // else MSB = 1, the SDA = 1

        SCL = 1;
        _asm
            nop
            nop
            nop
            nop
            nop
        _endasm;

        SCL = 0;

        data1= data1<<1;          // rotate for next bit
    }
}

unsigned char read(unsigned char status)
{
    unsigned char count, byte = 0, in;

    SDA = 1;                                    // Configure SDA pin as input

    for (count = 0; count < 8 ;count++  )       // Loop till all 8 bytes are written
    {
        byte <<= 1;                             // Shift byte left by 1
        in = ack();                    			// Check bit value on SDA
        in &= 0x01;                        		// Set or clear a bit as per the RCV MASK
        byte |= in;                            	// Prepare the byte to be returned
    }

    SDA = status;                               // Status can be an ACK or NACK

    SCL=0;
    _asm
        nop
        nop
        nop
        nop
        nop
    _endasm;

    SCL=1;
    _asm
        nop
        nop
        nop
        nop
        nop
    _endasm;

    return byte;                                // Return the byte value that was received on SDA
}

void isr_init()
{
    EA=1;
    TMOD|=0x01;
    TH0=0x00;
    TL0=0x00;
    TR0=0;
    ET0=1;
}

void isr_one(void) interrupt(1)         // Timer 0 ISR
{
    TH0=0x00;                           // Load initial count as 0000H
    TL0=0x00;
    TR0=1;                              // Run Timer
    t++;
}

void measure_req()						// Send Measurement Request
{
    start();                            //Issue start condition
    write(SLAVE_WRITE);                 //Send Slave Address + Write Bit
    ack();                              // Receive acknowledgement
    stop();                             // Issue stop condition
}

void data_fetch()						// Data Fetch Request
{
    int i;
    start();                            //Issue start condition
    write(SLAVE_READ);                  //Send Slave Address+Read Bit
    ack();                              // Receive acknowledgement
    for(i=0;i<4;i++)                    // Read four bytes of sensor data
    {
        sensor[i]=(unsigned int)read(0);
        ack();
    }
}

void humidity_temp()
{
    int sensor_final;
    measure_req();
    data_fetch();
    sensor[0]=sensor[0]&0x3F;                           // Mask the first two status bits of the humidity byte
    sensor[3]=sensor[3]&0xFC;                           // Mask the last two bits that are don;t care of temperature byte
    humidity=(sensor[0]<<8)+sensor[1];                  // Combine 14 bits of humidity data
    sensor_final=(sensor[2]<<8)+(sensor[3]);
    temperature=(sensor_final>>2);                      // Combine 14 bits of temperature data
    rel_humidity=(humidity*0.0061);                     // Humidity Calibration
    tempC=(temperature*0.01)-40;                        // Temperature Calibration
    if(input2=='1')                                     // Check whether Temperature or Humidity data is requested
    {
        printf_tiny("\n\rTemperature=%d",tempC);
        lcd_command(0x01);                              // Clear LCD screen
        lcdputstr("Data Acquired");
        lcdgotoxy(2,0);
        lcdputstr("Temperature:");
        lcdputch((tempC/10)+0x30);                      // Print tens value of the Temperature
        lcdputch((tempC%10)+0x30);                      // Print units value of the Temperature
        byteh=tempC/10;
        bytel=tempC%10;
    }
    else if(input2=='2')
    {
        printf_tiny("\n\rRelative Humidity=%d",rel_humidity);
        lcd_command(0x01);                              // Clear LCD screen
        lcdputstr("Data Acquired.");
        lcdgotoxy(2,0);
        lcdputstr("Humidity: ");
        lcdputch((rel_humidity/10)+0x30);               // Print tens value of the Humidity
        lcdputch((rel_humidity%10)+0x30);               // Print units value of the umidity
        lcdputch('%');
        byteh=rel_humidity/10;
        bytel=rel_humidity%10;
    }
}

void ultrasonic()											// Calculate distance im inches
{
    int t1,t2,count;                                        // Define variables for time and count
    while(P1_0==1);                                         // Wait while the signal is high
    while(P1_0==0);                                         // Wait till the signal is low
    while(P1_0==1)                                          // When a valid high pulse is obtained
    {
        TR0=1;												// Start the timer
    }
    TR0=0;                                                  // Stop the timer
    t1=(TH0<<8);                                            // Store count by shifting TH0 by 8-bits to place in 10s position
    t2=TL0;                                                 // Store the LSB count
    count=t1+t2;
    time=(t*71135)+count;                                   // Obtain the total count if Timer overflows
    distance=(time/147);                                    // Obtain distance
    printf("\n\rDistance: %d (inches)",distance);
    lcd_command(0x01);                                      // Clear LCD screen
    lcdputstr("Data Acquired.");
    lcdgotoxy(2,0);
    lcdputstr("Distance: ");
    lcdputch((distance/10)+0x30);                           // Print tens value of the distance
    lcdputch((distance%10)+0x30);                           // Print units value of the distance
    lcdputstr("in");
    byteh=distance/10;
    bytel=distance%10;
}


void CMD(unsigned char cmd)
{
     SD_CS=0;
     write_spi(cmd | 0x40);
      write_spi(0);
      write_spi(0);
      write_spi(0);
      write_spi(0);
      write_spi(CRC);
}
void CMDx(unsigned char cmd,unsigned long addr)
{
     SD_CS=0;												// Chip select line made low 
     write_spi(cmd | 0x40);
       write_spi(addr>>24);
       write_spi(addr>>16);
       write_spi(addr>>8);
       write_spi(addr>>0);
       write_spi(CRC);
}

void write_spi( unsigned char byte)
{
     SPDAT=byte;                             // Send the byte to be transferred to SPDAT 
     while(!transmit_completed);
     transmit_completed = 0;
}

int response(void)
{
    int j=-1,i;
     for(i=0;i<5000;i++)
      {
         write_spi(0xFF);
         data_save = SPDAT;

         if(!(data_save&0x80))
         {
            break;
         }

      }

    switch(data_save)
        {
            case 0x00: printf_tiny("READY MODE \r\n");
                       j=2;
                       break;
            case 0x01: printf_tiny("IDLE MODE \r\n");
                       j=1;
                       break;
            case 0x02: printf_tiny("Error in Erase reset \r\n");
                       j=-1;
                       break;
            case 0x04: printf_tiny("Illegal command\r\n");
                       j=-1;
                       break;
            case 0x08: printf_tiny("CRC Error \r\n");
                       j=-1;
                       break;
            case 0x10: printf_tiny("Erase sequence Error \r\n");
                       j=-1;
                       break;
            case 0x20: printf_tiny("Address error\r\n");
                       j=-1;
                       break;
            case 0x40: printf_tiny("Parameter error \r\n");
                       j=-1;
                       break;
            default:  printf_tiny("Response byte error\r\n");
        }

   return j;
}
void spi_ready()
{
    int r;
        CMD(0);                           // Send Command 0
        r=response();                     // Receive response
        SD_CS=0;                           // Clear CS
        write_spi(0xFF);                  // Send Dummy byte
        do
        {
          CMD(1);                         // Send Command 1
          r=response();                   // Receive response
        }while(r!=2);                     // Continue till response obtained is 0x02

         write_spi(0xFF);                 // Send Dummy Byte
        delay_spi(100);                   // Wait for 100 ms
        SPCON |= 0x01;
}


void delay_spi(int time)
{
	unsigned char i,j;
    for (i = 0; i <time ; i++)
        for (j = 0; j <0xFF ; j++);             //Delay for 1 ms
}
void spi_init()
{
    int i;
    SPCON |= 0x10; 							    // Master mode
    P1_1=1; 									// Enable Master
    SPCON |= 0x80; 								// Fclk Periph/32 = 172KHz
    SPCON |= 0x20;
    SPCON &= ~0x08; 							// CPOL=0;
    SPCON &= ~0x04; 							// CPHA=0;
    IEN1 |= 0x04; 								// Enable SPI Interrupt
    SPCON |= 0x40; 								// Run SPI
    EA=1;										// Enable all interrupts
    delay_spi(2);								// Wait for 2 ms
    SD_CS=1;										// Make CS high
    for(i=0;i<16;i++)							// Send dummy bytes for Power On Time
      write_spi(0xFF);
}


void fat_init()
{
    
	 unsigned char stored_file[]="DataLogger";
	   spi_init();      						// Initializing SD card by writing to SPCON register and Enabling SPI Interrupt
	   spi_ready();     						// Waiting till SD card enters ready mode and data could be read or written to it
      
	  /*-------------------------------     BOOT SECTOR      -----------------------------------*/
		addr=0x00000000;
		CMDx(18,addr);	       
		skip_bytes(12);
		bs.sector_size=serial_data; 				 // Storing the sector size. It is usually 512 bytes
	
     	skip_bytes(2);
		for(j=0;j<2;j++)
		 {
		   spi_write(0xFF):
		   shorttm|=serial_data;
		   shorttm<<8;
		 }
		bs.reserved_sectors=shorttm;  				// Storing number of reserved sectors
		shorttm=0;
		
		write_spi(0xFF);                
		bs.number_of_fats=serial_data;  			// Storing number of FAT copies
	
		write_spi(0xFF);
		bs.root_dir_entries=serial_data;  			// Number of directory entries in the root directory. This should be 1 since there is only file
	
     	skip_bytes(5);
		bs.fat_size_sectors=serial_data;   			// Number of sectors used for storing one FAT table
    	
		
	/*----------------------------  THE ROOT DIRECTORY     --------------------------------------*/ 		
		addr= (bs.reserved_sectors + (bs.fat_size_sectors*bs.sector_size))*(bs.sector_size); //addr is 0x740201 from hex dump of Sd card
		CMDx(18,addr);

	for(i=0;i<bs.root_dir_entries;i++)  			// Iterating only once since it has only one file
		{
		  for(j=0;j<8;j++)                    		// First 8 bytes of root directory has the file name stored
           {
		     write_spi(0xFF);
			  entry.filename[j]=serial_data;
			}		   
		}
		
		skip_bytes(19); 							// Skipping through bytes since the other info is not relevant
		for(j=0;j<2;j++)
		 {
		   spi_write(0xFF):
		   shorttm|=serial_data;
		   shorttm<<8;
		 }
		entry.starting_cluster=shorttm;   			// Storing the starting cluster number of the data of the file stored
		starting_cluster = entry.starting_cluster; 
        
		 for(j=0;j<4;j++)
		 {
    	    write_spi(0xFF);
        	temp|=serial_data;
	     	temp<<8;
		 }
		entry.filesize=temp;
		nofbytes= entry.filesize; 					// Storing file size of the file. It should be 0 initially	
}

void skip_bytes(int num)
{
  int i;
  for(i=0;i<num;i++)
     write_spi(0xFF); 							// Sending dummy bytes 
}

void it_SPI(void) interrupt 9 					// Interrupt Address is 0x004B
{

   switch(SPSTA)								// Check the value of SPSTA Register and enter the appropriate case
    {
        case 0x80:								// SPSTA value when SPIF Flag is set indicating successful transmission
            serial_data=SPDAT;					// Store the data in SPDAT in a variable
            transmit_completed=1;				// Set the variable that checks for transmission

         break;
        case 0x20:
            transmit_completed=0;
        break;
        case 0x10:
            transmit_completed=0;
        break;
        case 0x40:
            transmit_completed=0;
            break;
    }
}

