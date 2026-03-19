#include "driver.h"
#include "registers.h"
#include <stdio.h>

bool rp2350_pcm5122_init(rp2350_pcm5122_t *device){

	device->mute = false;

	/* i2c setup */
	i2c_init(i2c_default, 100 * 1000);
	gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
	gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    	bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));


	uint8_t ret[32]; // for read tests
	uint8_t page_check;
	uint8_t format_check;
	/* reset PCM5122 */

	// goto page 0
	uint8_t page[2] = {PCM51XX_REG_PAGE_SELECT, 0x00};
	if (i2c_write_blocking (i2c_default, device->addr, page, 2, false) == PICO_ERROR_GENERIC){ return false; }


	/*
	// read page back 
	i2c_read_blocking(i2c_default, device->addr, ret, 1, false); //check that page 1 is selected
	page_check = ret[0];
	*/

	//set standby

	uint8_t standby[2] = {PCM51XX_REG_STANDBY, 0x10};
	if (i2c_write_blocking (i2c_default, device->addr, standby, 2, false) == PICO_ERROR_GENERIC){ return false; };

	// reset registers and modules	
	uint8_t reset[2] = {PCM51XX_REG_RESET, 0x11};
	if (i2c_write_blocking (i2c_default, device->addr, reset, 2, false) == PICO_ERROR_GENERIC){ return false; };
	
	
	// leave standby
	uint8_t leave_standby[2] = {PCM51XX_REG_STANDBY, 0x00};	
	if (i2c_write_blocking (i2c_default, device->addr, leave_standby, 2, false) == PICO_ERROR_GENERIC){ return false; };


	/* rest of setup*/

	uint8_t error_config[2] = {PCM51XX_REG_ERROR_DETECT, 0b01111101};	
	if (i2c_write_blocking (i2c_default, device->addr, error_config, 2, false) == PICO_ERROR_GENERIC){ return false; };
	
	// enable pll
	uint8_t pll_enable[2] = {PCM51XX_REG_PLL, 0x01};	
	if (i2c_write_blocking (i2c_default, device->addr, pll_enable, 2, false) == PICO_ERROR_GENERIC){ return false; };

	//set pll ref
	uint8_t pll_ref[2] = {PCM51XX_REG_PLL_REF, 0x10};	
	if (i2c_write_blocking (i2c_default, device->addr, pll_ref, 2, false) == PICO_ERROR_GENERIC){ return false; };



	//set i2s format to 16bit 

	uint8_t i2s_format[2] = {PCM51XX_REG_I2S_CONFIG, PCM5122_I2S_FORMAT_DEFAULT};	
	if (i2c_write_blocking (i2c_default, device->addr, i2s_format, 2, false) == PICO_ERROR_GENERIC){ return false; };



/*
	//read back some data for testing	
	if (i2c_write_blocking (i2c_default, device->addr, i2s_format, 1, false) == PICO_ERROR_GENERIC){ return false; };
	i2c_read_blocking(i2c_default, device->addr, ret, 1, false);
	format_check = ret[0];


	while(1){

		printf("page:  %02X\n", page_check);
		printf("format: %02X\n", format_check);
	}
*/

	return true;

}

void rp2350_pcm5122_set_mute(rp2350_pcm5122_t *device, bool toggle){


	
	uint8_t page[2] = {PCM51XX_REG_PAGE_SELECT, 0x00};
	i2c_write_blocking(i2c_default, device->addr, page, 2, false);  // goto page 0


	uint8_t mute[2] = {PCM51XX_REG_MUTE, 0x11};
	uint8_t unmute[2] = {PCM51XX_REG_MUTE, 0x00};


	if (toggle){

		i2c_write_blocking(i2c_default, device->addr, mute, 2, false);
		device->mute = true;
	}else{	
		i2c_write_blocking(i2c_default, device->addr, unmute, 2, false);
		device->mute = false;
	}

}



bool rp2350_pcm5122_set_volume(rp2350_pcm5122_t *device, float right_db, float left_db){
	
	uint8_t left_raw = (uint8_t)constrain((24.0 - left_db) / 0.5, 0, 255);
	uint8_t right_raw = (uint8_t)constrain((24.0 - right_db) / 0.5, 0, 255);



	uint8_t left_vol[2] = {PCM51XX_REG_DIGITAL_VOLUME_L, left_raw};	
	if (i2c_write_blocking (i2c_default, device->addr, left_vol, 2, false) == PICO_ERROR_GENERIC){ return false; };

	uint8_t right_vol[2] = {PCM51XX_REG_DIGITAL_VOLUME_R, right_raw};	
	if (i2c_write_blocking (i2c_default, device->addr, right_vol, 2, false) == PICO_ERROR_GENERIC){ return false; };

	

	return true;
}
