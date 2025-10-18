#include "disk.h"
#include "fat.h"
#include "memdefs.h"
#include "memory.h"
#include "stdio.h"
#include "x86.h"
#include "startscreen.h"
#include <stdint.h>
#include <stdbool.h>

uint8_t *KernelLoadBuffer = (uint8_t *)MEMORY_LOAD_KERNEL;
uint8_t *Kernel = (uint8_t *)MEMORY_KERNEL_ADDR;

typedef void (*KernelStart)();

void __attribute__((cdecl)) start(uint16_t bootDrive)
{
	clrscr();

	bool drawScreen = true;
	draw_start_screen(drawScreen);

	delay_ms(1000);

	DISK disk;
	if (!DISK_Initialize(&disk, bootDrive))
	{
		printf("Disk init error\r\n");
		goto end;
	}

	if (!FAT_Initialize(&disk))
	{
		printf("FAT init error\r\n");
		goto end;
	}

	// load kernel
	FAT_File *fd = FAT_Open(&disk, "/sys/core/kernel.bin");
	uint32_t read;
	uint8_t *kernelBuffer = Kernel;
	while ((read = FAT_Read(&disk, fd, MEMORY_LOAD_SIZE, KernelLoadBuffer)))
	{
		memcpy(kernelBuffer, KernelLoadBuffer, read);
		kernelBuffer += read;
	}
	FAT_Close(fd);

	// execute kernel
	KernelStart kernelStart = (KernelStart)Kernel;
	kernelStart();

end:
	for (;;);
}
