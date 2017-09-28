/* sb16.c - Sound driver implementation file.
 * Written by Soumithri Bala. */


#include "sb16.h"


/* global flag to keep track of sound card usage */
volatile int32_t in_use = 0;
/* global flag to keep track of interrupt status */
volatile int32_t int_flag = 1;
/* buffer from which DMA reads */
int8_t buffer[BUF_DIM][BUF_SIZE];


/* local function definitions */
int32_t sb16_reset();
uint8_t dsp_read();
void dsp_write(uint8_t command);
void dsp_init(uint16_t sample_rate, uint8_t bcommand, uint8_t bmode, uint16_t block_length);
void dma_init(uint16_t buf_offset, uint16_t buf_length, uint8_t buf_page);
void sb16_interrupt(void);
uint8_t lo_byte(uint16_t word);
uint8_t hi_byte(uint16_t word);


/* sb16_init
 *
 * 		DESCRIPTION: initializes the SB16
 *		INPUTS: info_block -- WAV header block with file data
 *		OUTPUTS: none
 *		RETURN VALUE: buffer -- address of buffer
 *		SIDE EFFECTS: sets SB16 and DMA settings
 */
int32_t sb16_init(const uint8_t* info_block) {

    uint8_t buf_page;
    uint8_t wav_check[FOUR_B + 1] = {0, 0, 0, 0, 0};
    uint16_t sample_rate, buf_offset;

    /* enable interrupts from the SB16 */
    enable_irq(SB16_IRQ_LINE);

    /* check if the card is already in use */
    if (in_use) {
        printf("Another process is using the SB16. Terminate it and try again.\n");
        return -1;
    }

    /* check if soundcard gets initialized properly */
    if (sb16_reset() == -1) {
        printf("SB16 initialization failed. Check hardware.\n");
        return -1;
    }

    /* check file */
    if (!info_block) {
        printf("Info block invalid.\n");
        return -1;
    }

    /* copy and reverse wav magic numbers because format is big endian */
    memcpy(wav_check, (info_block + WAV_MAGIC_LOC), FOUR_B);
    strrev((int8_t*)wav_check);

    /* check if valid */
    if (*((uint32_t*)wav_check) != WAV_MAGIC) {
        printf("Not a wav file.\n");
        return -1;
    }

    /* check if audio is compressed */
    if (*((uint16_t*)(info_block + WAV_FORMAT_LOC)) != 1) {
        printf("Only uncompressed music is supported.\n");
        return -1;
    }

    /* check if audio is stereo */
    if ((*((uint16_t*)(info_block + WAV_NCHANNELS_LOC)) != NCHANNELS) ||
            (*((uint16_t*)(info_block + BPSAMPLE_LOC)) != _16BITS)) {
        printf("Only 16-bit stereo audio is supported.\n");
        return -1;
    }

    /* load sample rate */
    sample_rate = *((uint16_t*)(info_block + SAMPLE_RATE_LOC));

    /* calculate buffer offset; align to 64KB page */
    buf_offset = ((uint32_t)buffer >> 1) % TWOTO16;

    /* find buffer page */
    buf_page = (uint32_t)buffer >> _16BITS;

    /* initialize dma */
    dma_init(buf_offset, (BUF_SIZE) - 1, buf_page);

    /* initialize dsp */
    dsp_init(sample_rate, DSP_BCOMMAND, DSP_BMODE, (BUF_SIZE / BUF_DIM) - 1);

    /* set flags high */
    in_use = 1;
    int_flag = 1;

    /* return pointer to buffer */
    return (int32_t)buffer;
}


/* sb16_copy_status
 *
 * 		DESCRIPTION: returns status of interrupt flag
 *		INPUTS: none
 *		OUTPUTS: none
 *		RETURN VALUE: int_flag -- value of interrupt flag
 *		SIDE EFFECTS: none
 */
int32_t sb16_copy_status() {

    return int_flag;
}


/* sb16_shutdown
 *
 * 		DESCRIPTION: calls reset and flags
 *		INPUTS: none
 *		OUTPUTS: none
 *		RETURN VALUE: 0 on success
 *		SIDE EFFECTS: resets the SB16
 */
int32_t sb16_shutdown() {

    /* call reset to clear SB16 values */
    sb16_reset();

    /* set flags to original values */
    in_use = 0;
    int_flag = 1;

    return 0;
}


/* sb16_reset
 *
 * 		DESCRIPTION: sends reset signal and waits
 *		INPUTS: none
 *		OUTPUTS: none
 *		RETURN VALUE: 0 or above on success, -1 on fail
 *		SIDE EFFECTS: resets the SB16
 */
int32_t sb16_reset() {

    int i;

    /* write 1 to reset port */
    outb(1, SB16_RESET_PORT);

    /* make system wait */
    for (i = 0; i < TWOTO16; i++) asm volatile("");

    /* write 0 to reset port */
    outb(0, SB16_RESET_PORT);

    /* check if read port returned success */
    i = WAITLOOP;
    while ((dsp_read() != SUCCESS_VAL) && i--);
    i--;

    /* returns -1 on failure, 0 or above on success */
    return i;
}


/* dsp_read
 *
 * 		DESCRIPTION: reads from DSP
 *		INPUTS: none
 *		OUTPUTS: none
 *		RETURN VALUE: returns byte read from DSP
 *		SIDE EFFECTS: none
 */
uint8_t dsp_read() {

    /* wait for poll port to go high */
    while (!(inb(SB16_POLL_PORT) & BUF_RDY_VAL));

    /* return value read from port */
    return (uint8_t) inb(SB16_READ_PORT);
}


/* dsp_write
 *
 * 		DESCRIPTION: writes to DSP
 *		INPUTS: command -- command to be written
 *		OUTPUTS: none
 *		RETURN VALUE: none
 *		SIDE EFFECTS: sets commands in DSP
 */
void dsp_write(uint8_t command) {

    /* wait until card is ready to receive command */
    while (inb(SB16_WRITE_PORT) & BUF_RDY_VAL);

    /* send command */
    outb(command, SB16_WRITE_PORT);
}


/* dsp_init
 *
 * 		DESCRIPTION: initializes the DSP with correct values
 *		INPUTS: sample_rate -- sample rate of audio to be played
 *		        bcommand -- DSP command setting
 *		        bmode -- DSP mode setting
 *		        block_length -- length of buffer block to be played back
 *		OUTPUTS: none
 *		RETURN VALUE: none
 *		SIDE EFFECTS: sets values in DSP
 */
void dsp_init(uint16_t sample_rate, uint8_t bcommand, uint8_t bmode, uint16_t block_length) {

    /* command to set output sample rate */
    dsp_write(DSP_OUT_RATE_CMD);

    /* write low byte of sample rate */
    dsp_write(hi_byte(sample_rate));

    /* write high byte of sample rate */
    dsp_write(lo_byte(sample_rate));

    /* set output */
    dsp_write(bcommand);

    /* set mode */
    dsp_write(bmode);

    /* set low byte of block size */
    dsp_write(lo_byte(block_length));

    /* set high byte of block size */
    dsp_write(hi_byte(block_length));
}


/* dsp_init
 *
 * 		DESCRIPTION: initializes the DMA with correct values
 *		INPUTS: buf_offset -- offset value from page register
 *		        buf_length -- length of buffer to be copied
 *		        buf_page -- DMA page register value
 *		OUTPUTS: none
 *		RETURN VALUE: none
 *		SIDE EFFECTS: sets values in and initializes DMA
 */
void dma_init(uint16_t buf_offset, uint16_t buf_length, uint8_t buf_page) {

    /* send stop mask */
    outb(DMA_STOP_MASK, DMA_MASK_PORT);

    /* clear pointer */
    outb(0, DMA_CLR_PTR_PORT);

    /* set DMA to correct mode */
    outb(DMA_MODE, DMA_MODE_PORT);

    /* set low byte of buffer offset */
    outb(lo_byte(buf_offset), DMA_BASE_ADDR);

    /* set high byte of buffer offset */
    outb(hi_byte(buf_offset), DMA_BASE_ADDR);

    /* set low byte of length */
    outb(lo_byte(buf_length), DMA_COUNT_PORT);

    /* set high byte of length */
    outb(hi_byte(buf_length), DMA_COUNT_PORT);

    /* set page */
    outb(buf_page, DMA_PAGE_PORT);

    /* send start mask */
    outb(DMA_START_MASK, DMA_MASK_PORT);
}


/* sb16_interrupt
 *
 * 		DESCRIPTION: SB16 interrupt handler
 *		INPUTS: none
 *		OUTPUTS: none
 *		RETURN VALUE: none
 *		SIDE EFFECTS: reverses flag and acknowledges interrupt
 */
void sb16_interrupt(void) {

    /* interrupt setup */
    asm volatile("pushal");
    cli();

    /* toggle flag */
    int_flag = !int_flag;

    /* acknowledge interrupt */
    inb(SB16_POLL_PORT_16);

    /* eoi routine */
    send_eoi(SB16_IRQ_LINE);
    sti();
    asm volatile("          \n\
                  popal     \n\
                  leave     \n\
                  iret      \n\
                 ");
}


/* lo_byte
 *
 * 		DESCRIPTION: returns low byte of input word
 *		INPUTS: word -- word of which to find low byte
 *		OUTPUTS: none
 *		RETURN VALUE: word_ptr[0] -- low byte of word
 *		SIDE EFFECTS: none
 */
uint8_t lo_byte(uint16_t word) {

    uint8_t word_ptr[BUF_DIM];

    *((uint16_t*)word_ptr) = word;

    return word_ptr[0];
}


/* hi_byte
 *
 * 		DESCRIPTION: returns high byte of input word
 *		INPUTS: word -- word of which to find high byte
 *		OUTPUTS: none
 *		RETURN VALUE: word_ptr[1] -- high byte of word
 *		SIDE EFFECTS: none
 */
uint8_t hi_byte(uint16_t word) {

    uint8_t word_ptr[BUF_DIM];

    *((uint16_t*)word_ptr) = word;

    return word_ptr[1];
}
