#include <arch/i686/irq.h>
#include <fs/fat12/fat12.h>
#include <hal/hal.h>
#include <jvm/jvm.h>
#include <memory/memory.h>
#include <std/stdio.h>
#include <stdint.h>
#include <text/buffer.h>

extern uint8_t __bss_start;
extern uint8_t __end;

void crash_me();

void timer(Registers *regs)
{
  //printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));

   /* initialize basic memory allocator before other subsystems */
   mem_init();

   HAL_Initialize();

   clrscr();

   i686_IRQ_RegisterHandler(0, timer);

   printf("Hello from kernel!\n");

   // FAT12 test: read /test.txt
   printf("[DEBUG] Starting disk init\n");
   DISK disk;
   disk.type = DISK_TYPE_FLOPPY; // or DISK_TYPE_ATA for hard disk
   disk.id = 0;                  // usually 0 for first drive
   if (DISK_Initialize(&disk, 0))
   {
      printf("[DEBUG] Disk init OK\n");
      printf("[DEBUG] Starting FAT init\n");
      if (FAT_Initialize(&disk))
      {
         printf("[DEBUG] FAT init OK\n");
         printf("[DEBUG] Opening /test.txt\n");
         FAT_File *file = FAT_Open(&disk, "/test.txt");
         if (file)
         {
            printf("[DEBUG] /test.txt open OK\n");
            char buf[256] = {0};
            printf("[DEBUG] Reading file\n");
            uint32_t bytes = FAT_Read(&disk, file, sizeof(buf) - 1, buf);
               buf[bytes] = '\0';
               printf("[DEBUG] FAT_Read returned %u bytes\n", (unsigned)bytes);
               /* Hex-dump the returned bytes so we can inspect raw contents even
                * when printing as a string doesn't show anything. */
               printf("[DEBUG] Hexdump of returned buffer (%u bytes):\n", (unsigned)bytes);
               /* Use print_buffer() helper which emits hex using the
                * kernel's own hex routine. Our printf implementation
                * doesn't support width/zero-padding ("%02X"), which is
                * why you saw "2X" in the output. print_buffer prints
                * two hex chars per byte.
                */
               print_buffer("", buf, bytes);

               if (bytes > 0)
               {
                  printf("Contents of /test.txt:\n%s\n", buf);
               }
               else
               {
                  /* Hex-dump first 64 bytes to help debug */
                  printf("[DEBUG] Empty read; hexdump of buffer:\n");
                  for (unsigned i = 0; i < 64 && i < sizeof(buf); i++)
                  {
                     unsigned char c = (unsigned char)buf[i];
                     printf("%02X ", c);
                     if ((i & 15) == 15) printf("\n");
                  }
                  printf("\n");
               }
            FAT_Close(file);
            printf("[DEBUG] File closed\n");
         }
         else
         {
            printf("FAT: Could not open /test.txt\n");
         }
      }
      else
      {
         printf("FAT: Filesystem init failed\n");
      }
   }
   else
   {
      printf("DISK: Init failed\n");
   }

   // crash_me();

end:
   for (;;);
}
