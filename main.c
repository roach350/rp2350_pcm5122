#include <math.h>
#include <complex.h>
#include "hardware/gpio.h"

#include "../rp2350_i2s/rp2350_i2s.h"
#include "driver.h"

#include <stdio.h>

#define PIO_CLOCK_PER_BIT 2
#define BIT_PER_SAMPLE 16





void yield(void) {
	/* we could do context switching here for cooperative multitasking if we wanted */
	__dsb();
	__wfe();
}

static float cmagsquaredf(const float complex x) {
	return crealf(x) * crealf(x) + cimagf(x) * cimagf(x);
}

static uint64_t xorshift64star(void) {
	/* marsaglia et al., yields 64 bits, most significant are most random */
	static uint64_t x = 1; /* must be nonzero */
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	return x * 0x2545F4914F6CDD1DULL;
}

static float frand_minus_frand(void) {
	/* generate 64 random bits, of which we will use the most significant 46, in two groups of 23 */
	const uint64_t bits = xorshift64star();

	/* generate two random numbers each uniformly distributed on [1.0f, 2.0f) */
	const union { uint32_t u; float f; } x = { .u = 0x3F800000U | ((bits >> 41) & 0x7FFFFFU) };
	const union { uint32_t u; float f; } y = { .u = 0x3F800000U | ((bits >> 18) & 0x7FFFFFU) };

	/* and subtract them, yielding a triangular distribution on (-1.0f, +1.0f) */
	return x.f - y.f;
}

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}



/* from pico-sdk docmentation */
void i2c_bus_scan(){

   printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");


}

int main() {

	stdio_init_all();
	__attribute((aligned(sizeof(int16_t) * 2 * SAMPLES_PER_CHUNK * CHANNELS)))
	int16_t buffer[CHANNELS * SAMPLES_PER_CHUNK * CHANNELS];
	_Static_assert(1U << BUFFER_WRAP_BITS == sizeof(buffer), "wtf");


	/* enable sevonpend, so that we don't need nearly-empty ISRs */
	scb_hw->scr |= M33_SCR_SEVONPEND_BITS;

	set_sys_clock_48mhz();

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);


	//pcm5122 i2c setup
	rp2350_pcm5122_t pcm5122;
	pcm5122.addr = PCM5122_ADDRESS_DEFAULT;
	rp2350_pcm5122_init(&pcm5122);
/*
	while (1){	
		i2c_bus_scan();
	}
*/
	rp2350_pcm5122_set_mute(&pcm5122, false);
	rp2350_pcm5122_set_volume(&pcm5122, -103.0, 0); //mute left channel



	//i2s setup
	rp2350_i2s_t i2s_support;
	i2s_support.buffer = buffer;
	i2s_support.dma_channel = 0;
	i2s_support.bck_pin = 10;
	i2s_support.wsel_pin = 11;
	i2s_support.din_pin = 12;

	i2s_init(&i2s_support);

	

	const float sample_rate = 48e6f * 256.0f / (i2s_support.sys_cycle_256ths_per_pio_clock * PIO_CLOCK_PER_BIT * BIT_PER_SAMPLE * CHANNELS);

	/* this can be any value between dc and fs/2, does not need to be an integer */
	const float tone_frequency = 454.0f;

	/* multiplier relative to full scale */
	const float tone_amplitude = 0.1f;

	const float complex advance = cexpf(I * 2.0f * (float)M_PI * tone_frequency / sample_rate);

	/* this will evolve along the unit circle */
	float complex carrier = -1.0f;

	for (size_t ichunk = 0;; ichunk++) {
		int16_t * const dst = i2s_support.buffer + (CHANNELS * SAMPLES_PER_CHUNK * (ichunk % 2)) ;
		for (size_t ival = 0; ival < SAMPLES_PER_CHUNK; ival++) {
			const float sample = crealf(carrier) * tone_amplitude;

			for (size_t ic = 0; ic < CHANNELS; ic++)
				/* map [-1.0, 1.0] to [0, TOP] with triangular pdf dither */
				/* TODO: validate full scale and no clipping */
				dst[ic + CHANNELS * ival] = (sample * 32766) + 0.5f + frand_minus_frand();
			/* rotate complex sinusoid at the desired frequency */
			carrier *= advance;

			/* renormalize carrier to unity */
			carrier = carrier * (3.0f - cmagsquaredf(carrier)) / 2.0f;
		}

		/* if we just filled the first chunk and have not enabled the pio yet, enable it,
		   and immediately fill the next chunk without waiting for an interrupt */
		if (0 == ichunk && !(i2s_support.pio->ctrl & (1U << i2s_support.sm)))
			pio_sm_set_enabled(i2s_support.pio, i2s_support.sm, true);
		else {
			/* run other tasks or low power sleep until next dma interrupt */
			while (!(dma_hw->intr & 1U << i2s_support.dma_channel)) yield();

			/* acknowledge and clear the interrupt in both dma and nvic */
			dma_hw->ints0 = 1U << i2s_support.dma_channel;
			irq_clear(DMA_IRQ_0);
		}
	}
}

