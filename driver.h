#ifndef RP2350_PCM5122
#define RP2350_PCM5122


/* driver for controlling PCM5122 via i2c from a Pico using pico-sdk */


#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#define PCM5122_ADDRESS_DEFAULT 0x4c
#define PCM5122_I2S_FORMAT_DEFAULT 0x00 //i2s 16bit

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt))) // from Arduino



struct rp2350_pcm5122{

	i2c_inst_t i2c_device;
	uint8_t addr;
	bool mute, deem, filt, agn;
	uint8_t vol_l, vol_r;
	short att;
	

};

typedef struct rp2350_pcm5122 rp2350_pcm5122_t;

	

bool rp2350_pcm5122_init(rp2350_pcm5122_t *device); // return true if init succesfull


bool rp2350_pcm5122_set_volume(rp2350_pcm5122_t *device, float right_db, float left_db);
void rp2350_pcm5122_set_deemphasis(rp2350_pcm5122_t *device, bool toggle);
void rp2350_pcm5122_set_mute(rp2350_pcm5122_t *device, bool toggle);


#endif
