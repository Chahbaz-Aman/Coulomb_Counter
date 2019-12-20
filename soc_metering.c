#include <p89v51rd2.h>
#include <math.h>
#include <intrins.h>

#define ADC_bus P1
#define LCD_bus P0
#define ocv_sig  P3^0 //P3^7 causes interrupts at INT1 for idling
#define T0_count 0xD2	//50 us timing
#define MSB 0xB4      //...
#define LSB 0x00			//T2 20ms timing 11.0592 MHz crystal

char check = 3;
char code soc_vs_ocv[256];
long Ampacity;	  	//Ampere hour value -- will be converted to Ampere second

sbit ALE = P2^0;		//...
sbit SOC = P2^1;		//...
sbit EOC = P2^2;		//...
sbit OE  = P2^3;		// ADC0808 pin definitions
sbit CS_A= P2^4;		//...
sbit CS_B= P2^5;		//...
sbit CS_C= P2^6;		//...

sbit RS = P3^5;     //...
sbit RW = P3^6;     // LCD pin definitions
sbit E  = P3^7;			//...
sbit backlight = P3^0; //LCD backlight control 
sbit LCD_busy =P0^7;//D7 bit of LCD bus 
sbit clk = P2^7;    //Clockout for ADC

bit ft0 = 0, ft2 = 0, fex0 = 0, fex1 = 0, b = 0;
unsigned char temp_SoC,SoC=100,old_SoC;
unsigned char V_shunt,V_temp,V_terminal,V_OCV;
unsigned char temperature_time = 0, min = 0;
unsigned int  sample_rate_count = 0;
long t0_steps = 0;
float R,alpha,I;
double SoC_acc = 100;
float I_factor, V_factor, T_factor;

void record(unsigned char,unsigned char); //to add data to database in the cloud
void send(unsigned char);              //serial transmission function

void delay(int x) {
	while(x--);
}

void ADC_init() {
	ALE = 0;
	SOC = 0;
	OE  = 0;
	EOC = 1;
	CS_A= 0;
	CS_B= 0;
	CS_C= 0;
}

void ADC_read() {
	ALE = 1;
	SOC = 1;
	ALE = 0;
	SOC = 0;
	while(EOC==1);
	while(EOC==0);
	OE  = 1;
	return;
}


void LCD_isready() {
	LCD_busy = 1;	//set as input
	RS = 0;
	RW = 1;
	while(LCD_busy == 1) {
		E = 0;
		delay(100);
		E = 1;
	}
	return;
}

void LCD_command(unsigned char com) {
	LCD_isready();
	LCD_bus = com;
	RS = 0; RW = 0; E = 1;
	delay(100);
	E = 0;
	delay(100);
}
void LCD_data(unsigned char dat) {
	LCD_isready();
	LCD_bus = dat;
	RS = 1; RW = 0; E = 1;
	delay(100);
	E = 0;
	delay(100);
}

void LCD_init() {
	backlight = 1;
	LCD_isready();
	LCD_command(0x38);  //set 8 bit, 2 line, 5x7 display char
	LCD_command(0x01);  //clear the display
	LCD_command(0x0F);  //display on cursor blinking
	LCD_command(0x80);  //cursor to beginning of line 1
}

void get_runtime_data() {
		CS_A = 0; CS_B = 0; CS_C = 0;		 //shunt/fuse voltage at IN0 of ADC
		ADC_read();
	delay(100);
		V_shunt = ADC_bus;
	  delay(100);	
		OE = 0;
	
		CS_B = 1; CS_C = 0;          //accumulator terminal voltage at IN2 of ADC
		ADC_read();
	delay(100);
		V_terminal = ADC_bus;
		delay(100);
		OE = 0;
	
	if(++temperature_time == 60) { //update temperature every minute <-- high inertia parameter
		CS_B = 0;		CS_C = 1;					 //shunt/fuse temperature at IN1 of ADC
		ADC_read();
	delay(100);
		V_temp = ADC_bus;
		delay(100);
		OE = 0;
		temperature_time = 0;
	}
	
}

void set_initials() {
		Ampacity = 4;      //Ah
			Ampacity*=3600;  //Ampacity in Ampere-seconds
		R = 1;             //Shunt resitance in Ohms at NTP
		alpha = 0.0001;    //Shunt resistance temperature coefficient
		I_factor = 5.0/255.0;    //(Load current at 5V shunt drop)/255;
	  V_factor = 13.6/255.0;   //Peak Voltage/255;
		T_factor = 0/255.0;
		SoC = 100;         //First run must happen with a fully charged accumulator
		old_SoC = SoC;
}

void display(unsigned char SoC) {
	unsigned char a,b,c,i=0,dis[]="Discharged";
	temp_SoC = SoC;
	LCD_command(0x01);
	if(SoC<=0) {
			while(dis[i]!='\0')
			LCD_data(dis[i++]);
			return;
		}
	LCD_data('S'); LCD_data('o'); LCD_data('C'); LCD_data(' '); LCD_data('='); LCD_data(' ');
	a = temp_SoC/100| 0x30;
	temp_SoC %= 100;
	b = temp_SoC/10 | 0x30;
	c = temp_SoC%10 | 0x30;
	
	if(SoC == 100) {
		LCD_data(a);	LCD_data(b);	LCD_data(c);
	}
	else if(SoC>=10) {
		LCD_data(' ');	LCD_data(b);	LCD_data(c);	
	}
	else {
		LCD_data(' ');	LCD_data(' ');	LCD_data(c);	
	}
  LCD_data('%');
}


void main() { 
	IP0H = 0x5;
	ADC_bus = 0xFF; //Port 1 used for input
	TMOD = 0x22;		//Timer 0 and Timer 1 in autoreload mode
	SCON = 0x20;		//10 bit UART communication
	TH0  = T0_count;   
	TH1  = -3;    	//9600 baud rate for NodeMCU
	
	EA = 1; ET2 = 1; ET1 = 0; EX1 = 0; ET0 = 1; EX0 = 1; 
	
	T2MOD= 0x00;		//no clock out no down counting
	T2CON= 0x00;		//TF2 EXF2 RCLK TCLK EXEN2 C/T CP/RL :16bit autoreload
	                //using accurate T2 20ms interrupt allows less accumulating timestep error
	TH2 = MSB; RCAP2H = MSB; TL2 = LSB; RCAP2L = LSB; 
	TH1 = -3; 		  //9600 baud rate for NodeMCU
	IT0 = 1;     		//edge trigger at INT0 will put the uC in idle mode
	IT1 = 1;        //...

	set_initials(); 
	LCD_init();
	ADC_init();
	
	TR0 = 1;
	TR1 = 1;
	TR2 = 1;
	
	display(old_SoC);	
	
	while(1) {	

		if(fex0==1) { 
			fex0= 0;               //power up            
			//IT0 = 1;             //High to Low transition at INT0 will cause interrupt
			b = ~b;
			if(b) {
			  ET2 = 0; ET0 = 0; 
				LCD_command(0x08);
				backlight = 0;
				PCON = 0x01;         //idle mode at odd instance of interrupt
				_nop_; _nop_; _nop_;				
			}
			else {
				backlight = 1;
				LCD_command(0x0F);
			}
		} 
				
		if(ft0 ==1) { 
			ft0 = 0; 
			++sample_rate_count;
			if(sample_rate_count == 20000)   	        //1 second samples
				{	
					sample_rate_count = 0;
					get_runtime_data();					
					R = R + R*alpha*(V_temp*T_factor - 25);
					record(V_shunt,1);
					record(V_terminal,2);
					//**---------------SoC calculation------------------**//
				
					I = V_shunt*I_factor/R;
					if(SoC>0) 
						{
							SoC_acc = SoC_acc - I*100.0/Ampacity;
							SoC = ceil(SoC_acc);
						}
					else
						{
							SoC = 0;
						}
						
					//**-------------------------------------------------**//
					if(SoC!=old_SoC) {	
						old_SoC = SoC;
						display(old_SoC);       //Update LCD if SoC has changed
					}
					record(SoC,3);
				}
			}                   
		} //while end
}

void isr_ex0() interrupt 0 { //power down/power up interrupt
	fex0 = 1;									 //highest priprity interrupt
	ET2 = 1; ET0 = 1;          //to allow timers interrupts in next normal operation
}


void isr_ex1() interrupt 2 { //idle mode interrupt
	fex1= 1;  
	EX1 = 0; EX0 = 1;          //to prevent wake-up due to timer interrupts
}

void isr_t0()  interrupt 1 { //run time sampling interrupt
	ft0 = 1;
	clk = ~clk;                //clock out for ADC0808
}

void isr_t2()  interrupt 5 { //data sampling interrupt
	ft2 = 1;
}
void record(unsigned char byte, unsigned char id) {
	int x;
	switch (id) { 
		case 1:	x = byte*I_factor*10/R; //x10 for 100mA resolution
							send(x/10 + 0x30); //MSD in ASCII
							send(x%10 + 0x30); //LSD in ASCII
						break;
		case 2: x = byte*V_factor;
							send(x/10 + 0x30); //MSD in ASCII
							send(x%10 + 0x30); //LSD in ASCII
						break;
		case 3:	x = byte;
						if(x==100) x == 99;
						//x%=100;
							send(x/10 + 0x30); //MSD in ASCII
							send(x%10 + 0x30); //LSD in ASCII
						break;
		default:break;				
	}
}

void send(unsigned char ch) {
	SBUF = ch;										 //send digit
	while(TI==0);
	TI = 0;
}