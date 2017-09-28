/* Simple music program to demonstrate audio system call functionality.
 * Written by Soumithri Bala. */


#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"


#define BUF_SIZE    (65536 / 2)
#define BUF_DIM     2
#define COPY_LEN    1024
#define IBLOCK_SIZE 44
#define TWO_B       2
#define FOUR_B      4
#define EIGHT_B     8
#define RADIX       10
#define _4KB        4096


int main() {

    int32_t fd;
    uint32_t buf_val[BUF_DIM];
    uint8_t fname[COPY_LEN];
    uint8_t info_block[IBLOCK_SIZE];
    int32_t init_retval;
    volatile int prev_cstatus = 0;
    volatile int temp = 0;

    /* get file name */
    if (0 != ece391_getargs (fname, COPY_LEN)) {
        ece391_fdputs (1, (uint8_t*)"could not read arguments\n");
        return 3;
    }

    /* check if filename is valid */
    if (-1 == (fd = ece391_open (fname))) {
        ece391_fdputs (1, (uint8_t*)"file not found\n");
        return 2;
    }

    /* read first 44 blocks of file */
    ece391_read(fd, info_block, IBLOCK_SIZE);

    /* get retval from init, which should be a ptr if successful */
    init_retval = ece391_audio_init(info_block);
    /* terminate program if init was unsuccessful */
    if (init_retval == -1) return 0;

    /* calculate and load pointer values */
    buf_val[0] = (uint32_t)init_retval;
    buf_val[1] = (uint32_t)init_retval + BUF_SIZE;

    /* copy first chunks into buffer */
    ece391_read(fd, (int8_t*)buf_val[0], BUF_SIZE);
    ece391_read(fd, (int8_t*)buf_val[1], BUF_SIZE);

    while(1) {
        /* record interrupt status */
        temp = ece391_audio_cstatus();
        /* check if status changed */
        if (prev_cstatus != temp) {
            /* copy block into correct buffer region, and terminate program if
             * finished */
            if (!ece391_read(fd, (int8_t*)buf_val[temp], BUF_SIZE)) {
                ece391_audio_shutdown();
                return 0;
            }
            /* record current status */
            prev_cstatus = temp;
        }
    }

    /* abnormal if program reaches this point... */
    return 1;
}
