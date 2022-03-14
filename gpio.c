#include "gpio.h"
#include "print.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <string.h>

// fastest method to set the GPIO pins. 
// benchmarks: https://codeandlife.com/2012/07/03/benchmarking-raspberry-pi-gpio-speed/
// the original code: https://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access

// for setting or clearing gpio outputs.
volatile uint32_t *gpioSet;
volatile uint32_t *gpioClr;

// #define BCM2708_PERI_BASE        0x20000000  /* Raspberry PI? */
// #define BCM2711_PERI_BASE        0xFE000000  /* Raspberry PI4 */
#define GPIO_BASE                0x200000 /* GPIO controller */
#define GPIO_SIZE                0xF4 /* GPIO registers size in bytes */

int piCores;
int pi_ispi;
uint32_t pi_peri_phys;

uint32_t gpioBits[NCHAN] = {CHAN0BIT, CHAN1BIT, CHAN2BIT, CHAN3BIT, CHAN4BIT, CHAN5BIT, CHAN6BIT, CHAN7BIT};
uint32_t gpioID[NCHAN] = {CHAN0, CHAN1, CHAN2, CHAN3, CHAN4, CHAN5, CHAN6, CHAN7};


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
// #define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
// #define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
// #define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

// #define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
// #define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

// #define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

// #define GPIO_PULL *(gpio+37) // Pull up/pull down
// #define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock



// copied from pigpio (pigpio.c)
// return the revision number of 0 if unknown.
unsigned gpioHardwareRevision(void) {
   static unsigned rev = 0;
   char buf[512];
   char term;

   if (rev) 
      return rev;

   FILE * filp = fopen ("/proc/cpuinfo", "r");
   if(filp == NULL) {
      printErr("failed opening '/proc/cpuinfo'\n");
      return -1;
   }
   while (fgets(buf, sizeof(buf), filp) != NULL)
      if (!strncasecmp("revision\t:", buf, 10))
         if (sscanf(buf+10, "%x%c", &rev, &term) == 2)
            if (term != '\n') 
               rev = 0;
   fclose(filp);

   /* (some) arm64 operating systems get revision number here  */

   if (rev == 0) {
      filp = fopen ("/proc/device-tree/system/linux,revision", "r");
      if (filp != NULL) {
         uint32_t tmp;
         if (fread(&tmp,1 , 4, filp) == 4) {
            /*
               for some reason the value returned by reading
               this /proc entry seems to be big endian,
               convert it.
            */
            rev = ntohl(tmp);
            rev &= 0xFFFFFF; /* mask out warranty bit */
         }
         fclose(filp);
      }
   }

   piCores = 0;
   pi_ispi = 0;
   rev &= 0xFFFFFF; /* mask out warranty bit */

   /* Decode revision code */

   if ((rev & 0x800000) == 0) { /* old rev code */
      if ((rev > 0) && (rev < 0x0016)) {/* all BCM2835 */
         pi_ispi = 1;
         piCores = 1;
         pi_peri_phys = 0x20000000;
      } else {
         if(rev != 0)
            printErr("unknown revision=%X\n", rev);
         rev = 0;
      }
   } else { /* new rev code */
      switch ((rev >> 12) & 0xF)  /* just interested in BCM model */
      {

         case 0x0:   /* BCM2835 */
            pi_ispi = 1;
            piCores = 1;
            pi_peri_phys = 0x20000000;
            break;

         case 0x1:   /* BCM2836 */
         case 0x2:   /* BCM2837 */
            pi_ispi = 1;
            piCores = 4;
            pi_peri_phys = 0x3F000000;
            break;

         case 0x3:   /* BCM2711 */
            pi_ispi = 1;
            piCores = 4;
            pi_peri_phys = 0xFE000000;
            break;

         default:
            printErr("unknown rev code (%x)\n", rev);
            rev=0;
            pi_ispi = 0;
            break;
      }
   }
   // print("revision=%x\n", rev);
   // print("pi_peri_phys=%x\n", pi_peri_phys);
   // print("piCores=%d\n", piCores);
   // print("pi_isPi=%d\n", pi_ispi);
   //fprintf(stdout, "pi_dram_bus=%x", pi_dram_bus);
   //fprintf(stdout, "pi_mem_flag=%x", pi_mem_flag);
   return rev;
}

// Set up a memory regions to access GPIO
int gpio_init() {
	int  mem_fd;
	void *gpio_map;
   static uint32_t dummyRegister;

	if(gpioSet != NULL && gpioClr != NULL)
      return 2;
   if(gpioSet != NULL || gpioClr != NULL)
		return -1;

   unsigned rev = gpioHardwareRevision();
   if(rev == 0) {
      // if host is not a raspberry pi, it's ok.
      if(pi_ispi == 0) {
         gpioClr = gpioSet = &dummyRegister;
         return 1;
      }
      printErr("failed getting a valid revision value");
      return -1;
   }

   if((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printErr("can't open /dev/mem: must run as root\n");
      return -1;
   }

   gpio_map = mmap(
      NULL,                    // Any adddress in our space will do
      GPIO_SIZE,               // Map length
      PROT_READ|PROT_WRITE,    // Enable reading & writting to mapped memory
      MAP_SHARED,              // Shared with other processes
      mem_fd,                  // File to map
      pi_peri_phys+GPIO_BASE // Offset to GPIO peripheral
   );

   close(mem_fd); // No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printErr("mmap error: %s\n", strerror(errno));
      return -1;
   }

   // set all channels to output mode
   uint32_t *gpio = (uint32_t *)gpio_map;
   for(int i = 0; i < NCHAN; i++) {
      uint32_t reg = gpioID[i]/10;
      uint32_t shift = (gpioID[i]%10) * 3;
      gpio[reg] &= ~(7<<shift);
      gpio[reg] |= 1<<shift;
   }
   gpioSet = (volatile uint32_t *)gpio + 7;
   gpioClr = (volatile uint32_t *)gpio + 10;
   return 0;
}
