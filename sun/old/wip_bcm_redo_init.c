int peripheral_init(void) {

    int memfd = -1;
    int ok = 0;

    if (!geteuid()) {

      if ((memfd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
	  fprintf(stderr, "bcm2835_init: Unable to open /dev/mem: %s\n", strerror(errno));
	  goto exit;
      }
      
      /* Base of the peripherals block is mapped to VM */
      bcm2835_peripherals = mapmem("gpio", bcm2835_peripherals_size, memfd, bcm2835_peripherals_base);

      if (bcm2835_peripherals == MAP_FAILED) goto exit;

      bcm2835_gpio = bcm2835_peripherals + BCM2835_GPIO_BASE/4;      
      bcm2835_clk  = bcm2835_peripherals + BCM2835_CLOCK_BASE/4;
      bcm2835_bsc0 = bcm2835_peripherals + BCM2835_BSC0_BASE/4; /* I2C */
      bcm2835_bsc1 = bcm2835_peripherals + BCM2835_BSC1_BASE/4; /* I2C */
      bcm2835_st   = bcm2835_peripherals + BCM2835_ST_BASE/4;

      ok = 1;

    } else goto exit;

exit:
    if (memfd >= 0)
        close(memfd);

    if (!ok)
	bcm2835_close();

    return ok;
}

void peripheral_close(void) {
	unmapmem((void**) &bcm2835_peripherals, bcm2835_peripherals_size);
}

void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
    __sync_synchronize();
    uint32_t v = *paddr;
    __sync_synchronize();
    v = (v & ~mask) | (value & mask);
    __sync_synchronize();
    *paddr = v;
    __sync_synchronize();
}

void gpio_function_select(uint8_t pin, uint8_t mode) {

    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
    uint8_t   shift = (pin % 10) * 3;
    uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t  value = mode << shift;
    bcm2835_peri_set_bits(paddr, value, mask);
}







/* Read an number of bytes from I2C */

uint8_t bcm2835_i2c_read(char* buf, uint32_t len) {
#ifdef I2C_V1
    volatile uint32_t* dlen    = bcm2835_bsc0 + BCM2835_BSC_DLEN/4;
    volatile uint32_t* fifo    = bcm2835_bsc0 + BCM2835_BSC_FIFO/4;
    volatile uint32_t* status  = bcm2835_bsc0 + BCM2835_BSC_S/4;
    volatile uint32_t* control = bcm2835_bsc0 + BCM2835_BSC_C/4;
#else
    volatile uint32_t* dlen    = bcm2835_bsc1 + BCM2835_BSC_DLEN/4;
    volatile uint32_t* fifo    = bcm2835_bsc1 + BCM2835_BSC_FIFO/4;
    volatile uint32_t* status  = bcm2835_bsc1 + BCM2835_BSC_S/4;
    volatile uint32_t* control = bcm2835_bsc1 + BCM2835_BSC_C/4;
#endif

    uint32_t remaining = len;
    uint32_t i = 0;
    uint8_t reason = BCM2835_I2C_REASON_OK;

    /* Clear FIFO */
    bcm2835_peri_set_bits(control, BCM2835_BSC_C_CLEAR_1 , BCM2835_BSC_C_CLEAR_1 );

    /* Clear Status */
    *status = BCM2835_BSC_S_CLKT | BCM2835_BSC_S_ERR | BCM2835_BSC_S_DONE);
    /* Set Data Length */
    *dlen = len;
    /* Start read */
    *control = BCM2835_BSC_C_I2CEN | BCM2835_BSC_C_ST | BCM2835_BSC_C_READ);
    
    /* wait for transfer to complete */
    while (!(*status & BCM2835_BSC_S_DONE)) {
        /* we must empty the FIFO as it is populated and not use any delay */
        while (remaining && *status & BCM2835_BSC_S_RXD) {
	    /* Read from FIFO, no barrier */
	    buf[i] = *fifo;
	    i++;
	    remaining--;
    	}
    }
    
    /* transfer has finished - grab any remaining stuff in FIFO */
    while (remaining && (*status & BCM2835_BSC_S_RXD)) {
        /* Read from FIFO, no barrier */
        buf[i] = *fifo;
        i++;
        remaining--;
    }
    
    /* Received a NACK */
    if (bcm2835_peri_read(status) & BCM2835_BSC_S_ERR) reason = BCM2835_I2C_REASON_ERROR_NACK;
    
    /* Received Clock Stretch Timeout */
    else if (bcm2835_peri_read(status) & BCM2835_BSC_S_CLKT) reason = BCM2835_I2C_REASON_ERROR_CLKT;
    
    /* Not all data is received */
    else if (remaining) reason = BCM2835_I2C_REASON_ERROR_DATA;

    bcm2835_peri_set_bits(status, BCM2835_BSC_S_DONE , BCM2835_BSC_S_DONE);

    return reason;
}






/*


its probably possible to not use this library in order to write the i2c code to control the display.. 


	but that kinda involves knowing alot about the memory device/layout of the bcm2835, 

		and im also learning about the ssd1306, 

			and so i kinda have my hands full... 

					i think ill just use the library, as opposed to implememting a minimal version of it, that is useful for my use cases... 

				i mean, maybe in the future, when we get this thing working with the library, we can go back and try to make it work WITHOUT the dependancy of the WHOLE bcm library... 




							definitely something id LIKE do it... 

						just kinda seems more fun, lol, 


						then using a library     lol





							using a library is kinda the cheap way out lol 




anyways, lets start with using it for noww, just to get thigns off the ground. 


*/






