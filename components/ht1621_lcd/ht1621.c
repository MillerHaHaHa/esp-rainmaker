/*******************************************************************************
Copyright 2016-2018 anxzhu (github.com/anxzhu)
Copyright 2018 Valerio Nappi (github.com/5N44P) (changes)
Based on segment-lcd-with-ht1621 from anxzhu (2016-2018)
(https://github.com/anxzhu/segment-lcd-with-ht1621)

Partially rewritten and extended by Valerio Nappi (github.com/5N44P) in 2018

This file is part of the HT1621 arduino library, and thus under the MIT license.
More info on the project and the license conditions on :
https://github.com/5N44P/ht1621-7-seg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "ht1621.h"

#define LOW 			0
#define HIGH 			1

static int _cs_p = 0;
static int _wr_p = 0;
static int _data_p = 0;
static int _backlight_p = 0;
static bool _backlight_en = 0;
static char _buffer[BUFFERSIZE] = {0};

static void wrone(unsigned char addr, unsigned char sdata);
static void wrclrdata(unsigned char addr, unsigned char sdata);
static void wrCLR(unsigned char len);
static void wrDATA(unsigned char data, unsigned char cnt);
static void wrCMD(unsigned char CMD);
static void setdecimalseparator(int dpposition);
static void config(); // legacy: why not in begin func
static void update();

void delay(int second)
{
	vTaskDelay(second * 1000 / portTICK_PERIOD_MS);
}

esp_err_t digitalWrite(int gpio_num, uint32_t level)
{
	return gpio_set_level(gpio_num, level);
}

// HT1621::HT1621(){
// 		_buffer[0] = 0x00;
// 		_buffer[1] = 0x00;
// 		_buffer[2] = 0x00;
// 		_buffer[3] = 0x00;
// 		_buffer[4] = 0x00;
// 		_buffer[5] = 0x00;
// 		_buffer[6] = 0x00;
// }

void ht1621_begin(int cs_p,int wr_p,int data_p,int backlight_p)
{
	memset(_buffer, 0, BUFFERSIZE);

	//zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = ((1ULL<<cs_p) | (1ULL<<wr_p) | (1ULL<<data_p));
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

	_cs_p=cs_p;
	_wr_p=wr_p;
	_data_p=data_p;
	_backlight_p=backlight_p;
	_backlight_en=true;

	config();
}

void ht1621_begin_noBacklight(int cs_p,int wr_p,int data_p)
{
	memset(_buffer, 0, BUFFERSIZE);

	//zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = ((1ULL<<cs_p) | (1ULL<<wr_p) | (1ULL<<data_p));
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

	_cs_p=cs_p;
	_wr_p=wr_p;
	_data_p=data_p;
	_backlight_en = false;

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	config();
}

void wrDATA(unsigned char data, unsigned char cnt) {
	unsigned char i;
	for (i = 0; i < cnt; i++) {
		digitalWrite(_wr_p, LOW);
		DELAY_ACTION;
		if (data & 0x80) {
			digitalWrite(_data_p, HIGH);
		}
		else
		{
			digitalWrite(_data_p, LOW);
		}
		DELAY_ACTION;
		digitalWrite(_wr_p, HIGH);
		DELAY_ACTION;
		data <<= 1;
	}
}
void wrclrdata(unsigned char addr, unsigned char sdata)
{
	addr <<= 2;
	digitalWrite(_cs_p, LOW);
	DELAY_ACTION;	
	wrDATA(0xa0, 3);
	wrDATA(addr, 6);
	wrDATA(sdata, 8);
	digitalWrite(_cs_p, HIGH);
	DELAY_ACTION;
}

void ht1621_display()
{
	wrCMD(LCDON);
}

void ht1621_noDisplay()
{
	wrCMD(LCDOFF);
}

void wrone(unsigned char addr, unsigned char sdata)
{
	addr <<= 2;
	digitalWrite(_cs_p, LOW);
	DELAY_ACTION;
	wrDATA(0xa0, 3);
	wrDATA(addr, 6);
	wrDATA(sdata, 8);
	digitalWrite(_cs_p, HIGH);
	DELAY_ACTION;
}
void ht1621_backlight()
{
	if (_backlight_en)
		digitalWrite(_backlight_p, HIGH);
	delay(1);
}
void ht1621_noBacklight()
{
	if(_backlight_en)
		digitalWrite(_backlight_p, LOW);
	delay(1);
}
void wrCMD(unsigned char CMD) {  //100
	digitalWrite(_cs_p, LOW);
	DELAY_ACTION;
	wrDATA(0x80, 4);
	wrDATA(CMD, 8);
	digitalWrite(_cs_p, HIGH);
	DELAY_ACTION;
}
void config()
{
	wrCMD(BIAS);
	wrCMD(RC256);
	wrCMD(SYSDIS);
	wrCMD(WDTDIS1);
	wrCMD(SYSEN);
	wrCMD(LCDON);
	wrCMD(TONEOFF);
}
void wrCLR(unsigned char len) {
	unsigned char addr = 0;
	unsigned char i;
	for (i = 0; i < len; i++) {
		wrclrdata(addr, 0x00);
		addr = addr + 2;
	}
}

void ht1621_beep(int enable)
{
	if(enable) {
		wrCMD(TONEON);
	} else {
		wrCMD(TONEOFF);
	}
}

void ht1621_clear(void)
{
	wrCLR(16);
}

// old code for reference

/*void ht1621_dispnum(float num){ //传入显示的数据，最高位为小数点和电量显示，显示数据为0.001-99999.9
//

	floatToString(_buffer,num,4);
 char temp;

	//unsigned char lednum[10]={0x7D,0x60,0x3E,0x7A,0x63,0x5B,0x5F,0x70,0x7F,0x7B};//显示 0 1 2 3 4 5 6 7 8 9
	unsigned int i;
	for(i=0;i<7;i++){

		if(temp=='0'){
			_buffer[i]|=0x7D;
		}
		else if (temp=='1'){
			_buffer[i]|=0x60;
		}
		else if (temp=='2'){
			_buffer[i]|=0x3e;
		}
		else if (temp=='3'){
			_buffer[i]|=0x7a;
		}
		else if (temp=='4'){
			_buffer[i]|=0x63;
		}
		else if (temp=='5'){
			_buffer[i]|=0x5b;
		}
		else if( temp=='6'){
			_buffer[i]|=0x5f;
		}
		else if (temp=='7'){
			_buffer[i]|=0x70;
		}
		else if (temp=='8'){
			_buffer[i]|=0x7f;
		}
		else if (temp=='9'){
			_buffer[i]|=0x7b;
		}
		else if (temp=='.'){
			_buffer[i]|=0xff;
		}
		else if (temp=='-'){
			_buffer[i]|=0x02;
		}
	}



		int dpposition;
		//find the position of the decimal point (0xff) in the buffer
		dpposition = strchr(_buffer, 0xff)-_buffer;
		_buffer[dpposition] = 0x80 | _buffer[dpposition+1];

		for(int i=BUFFERSIZE; i<dpposition-1; i--){
		 _buffer[i] =  _buffer[i+1];

		}




	update();
} */

void update(){ // takes the buffer and puts it straight into the driver
		// the buffer is backwards with respect to the lcd. could be improved

	// for(int i = 0; i < BUFFERSIZE; i++) {
	// 	wrone(2 * i, _buffer[BUFFERSIZE - i]);
	// }

	// printf("[ht1621] _buffer = ");
	// for(int i = 1; i <= BUFFERSIZE; i++) {
	// 	printf("%02x ", _buffer[BUFFERSIZE - i]);
	// }
	// printf("\n");

	wrone(2, _buffer[16]);
	wrone(4, _buffer[15]);
	wrone(6, _buffer[14]);

	wrone(18, _buffer[8]);
	wrone(20, _buffer[7]);
	wrone(22, _buffer[6]);
	wrone(24, _buffer[5]);
	wrone(26, _buffer[4]);
	wrone(28, _buffer[3]);
	wrone(30, _buffer[2]);
	wrone(32, _buffer[1]);
}

void ht1621_update(void)
{
	update();
}

// void update(){ // takes the buffer and puts it straight into the driver
// 		// the buffer is backwards with respect to the lcd. could be improved
// 		wrone(0, _buffer[5]);
// 		wrone(2, _buffer[4]);
// 		wrone(4, _buffer[3]);
// 		wrone(6, _buffer[2]);
// 		wrone(8, _buffer[1]);
// 		wrone(10,_buffer[0]);
// }

void ht1621_set_buffer(uint8_t addr, uint8_t data)
{
	_buffer[addr] = data;
}

uint8_t ht1621_get_buffer(uint8_t addr)
{
	return _buffer[addr];
}

void setdecimalseparator(int decimaldigits) {
	 // zero out the eight bit
		_buffer[3] &= 0x7F;
		_buffer[4] &= 0x7F;
		_buffer[5] &= 0x7F;

	if( decimaldigits <= 0 || decimaldigits > 3){
		return;
	}

	// 3 is the digit offset
	// the first three eights bits in the buffer are for the battery signs
	// the last three are for the decimal point
	_buffer[6-decimaldigits] |= 0x80;

	// refactoring, old code for reference
	/*switch(decimaldigits){
		case 1:
			_buffer[5] |= 0x80;
			break;
		case 2:
			_buffer[4] |= 0x80;
			break;
		case 3:
			_buffer[3] |= 0x80;
			break;
		case 0:
		default:
			break;
	}*/
}

void ht1621_print_long_num(long num){
	if(num > 999999) // basic checks
		num = 999999; // clip into 999999
	if(num < -99999) // basic checks
		num = -99999; // clip into -99999

	char localbuffer[7]; //buffer to work with in the function
	snprintf(localbuffer,7, "%6li", num); // convert the decimal into string

	for(int i=0; i<6; i++){
		_buffer[i] &= 0x80; // mask the first bit, used by batter and decimal point
		switch(localbuffer[i]){ // map the digits to the seg bits
			case '0':
				_buffer[i] |= 0x7D;
				break;
			case '1':
				_buffer[i] |= 0x60;
				break;
			case '2':
				_buffer[i] |= 0x3e;
				break;
			case '3':
				_buffer[i] |= 0x7a;
				break;
			case '4':
				_buffer[i] |= 0x63;
				break;
			case '5':
				_buffer[i] |= 0x5b;
				break;
			case '6':
				_buffer[i] |= 0x5f;
				break;
			case '7':
				_buffer[i] |= 0x70;
				break;
			case '8':
				_buffer[i] |= 0x7f;
				break;
			case '9':
				_buffer[i] |= 0x7b;
				break;
			case '-':
				_buffer[i] |= 0x02;
				break;
			}
		}

		update();

}

void ht1621_print_num_pre(float num, int precision){
	if(num > 999999) // basic checks
		num = 999999; // clip into 999999
	if(num < -99999) // basic checks
		num = -99999; // clip into -99999
	if(precision > 3 && num > 0)
		precision = 3; // if positive max precision allowed = 3
	else if(precision > 2 && num < 0)
		precision = 2;// if negative max precision allowed = 2
	if(precision < 0)
		precision = 0; // negative precision?!

	long ingegerpart;
	ingegerpart = ((long)(num*pow(10,precision)));

	ht1621_print_long_num(ingegerpart); // draw the integerized number
	setdecimalseparator(precision); // draw the decimal point

	update();
}

void ht1621_print_float_num(float num){
	// could be smarter and actually check how many
	// non zeros we have in the decimals
	ht1621_print_num_pre(num, 3);
}
