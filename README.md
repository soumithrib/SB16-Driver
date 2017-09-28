# Creative Sound Blaster 16 Driver
Creative Sound Blaster 16 sound card driver capable of CD-quality playback, written to be run on my ECE 391 OS project. This driver uses the ```Intel DMA Controller``` and the SB16's ```Double-Buffering``` mode to ensure the highest possible audio playback quality. Some function and system call definitions are not present, as I am not allowed to upload the entire OS codebase; these functions, however, are mainly for reading/writing or interrupt handling, and are therefore not essential to understand the functionality of the driver.

```sb16_driver.c``` - Initializes DSP and DMA, copies blocks to DSP, handles interrupts

```sb16_driver.h``` - Constant definitions

```user_level_program.c``` - Parses WAV files with sound driver and OS system calls
