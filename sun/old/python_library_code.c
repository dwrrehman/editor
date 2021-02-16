// C-ized version of the python library for interfacign with the SSD1306 piOLED display.


enum SSD1306_commands {
	SET_CONTRAST = 0x81, // requires: arg[0]:u8

	SET_FOLLOW_RAM_CONTENTS = 0xA4,
	SET_ENTIRE_DISPLAY_ON = 0xA5,

	SET_NORMAL = 0xA6,
	SET_INVERSION = 0xA7,

	SET_DISPLAY_ON = 0xAF,
	SET_DISPLAY_OFF = 0xAE,

	SET_MEMORY_ADDRESS = 0x20,
	SET_COLUMN_ADDRESS = 0x21,
	SET_PAGE_ADDRESS = 0x22,

	SET_DISPLAY_START_LINE = 0x40,

	SET_SEG_REMAP = 0xA0,
	SET_MULTIPLEX_RATIO = 0xA8,
	SET_COM_OUT_DIR = 0xC0,
	SET_DISPLAY_OFFSET = 0xD3,
	SET_COM_PIN_CONFIG = 0xDA,
	SET_DISP_CLOCK_DIV = 0xD5,
	SET_PRECHARGE = 0xD9,
	SET_VCOM_DESEL = 0xDB,
	SET_CHARGE_PUMP = 0x8D,
};

class _SSD1306(framebuf.FrameBuffer):

        self.width = 128;
        self.height = 32;
        self.pages = 4;

        for cmd in (

            SET_DISPLAY_OFF,
            
            SET_MEM_ADDR,  0x00, 
            
            SET_DISPLAY_START_LINE,

            SET_SEG_REMAP | 0x01,  // column addr 127 mapped to SEG0
            SET_MUX_RATIO, 32 - 1,

            SET_COM_OUT_DIR | 0x08,  // scan from COM[N] to COM0

            SET_DISP_OFFSET, 0x00,

            SET_COM_PIN_CFG, 0x02
            
            SET_DISP_CLK_DIV, 0x80,

            SET_PRECHARGE,  0xF1,

            SET_VCOM_DESEL, 0x30,
            
            SET_CONTRAST,    0xFF, // maximum contrast

            SET_ENTIRE_ON,  // output follows RAM contents

            SET_NORMAL,  // not inverted

            SET_CHARGE_PUMP,  0x14,

            SET_DISPLAY_ON,
        ): 
            self.write_cmd(cmd)

        self.fill(0)
        self.show()

    def poweroff(self):
        self.write_cmd(SET_DISPLAY_OFF)

    def poweron(self):        
        self.write_cmd(SET_DISPLAY_ON)

    def contrast(self, contrast):
        self.write_cmd(SET_CONTRAST)
        self.write_cmd(contrast)

    def invert(self, invert):
        self.write_cmd(SET_NORM_INV | (invert & 1))

    def show(self):
        self.write_cmd(SET_COL_ADDR)
        self.write_cmd(0)
        self.write_cmd(31)
        self.write_cmd(SET_PAGE_ADDR)
        self.write_cmd(0)
        self.write_cmd(3)
        self.write_framebuf()

class SSD1306_I2C(_SSD1306):
    
    def __init__(
        self, width, height, i2c, *, addr=0x3C, external_vcc=False, reset=None
    ):
        self.i2c_device = i2c_device.I2CDevice(i2c, addr)
        self.addr = addr

        self.temp = bytearray(2)
        # Add an extra byte to the data buffer to hold an I2C data/command byte
        # to use hardware-compatible I2C transactions.  A memoryview of the
        # buffer is used to mask this byte from the framebuffer operations
        # (without a major memory hit as memoryview doesnt copy to a separate
        # buffer).
        self.buffer = bytearray(((height / 8) * width) + 1)
        self.buffer[0] = 0x40  # Set first byte of data buffer to Co=0, D/C=1
        super().__init__(
            memoryview(self.buffer)[1:],
            width,
            height,
            external_vcc=external_vcc,
            reset=reset,
        )

    def write_framebuf(self):
        """Blast out the frame buffer using a single I2C transaction to support
        hardware I2C interfaces."""
        with self.i2c_device:
            self.i2c_device.write(self.buffer)





