#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

#define NCHAN  8

#define CHAN0  9 /* GPIO  9 */ 
#define CHAN1 10 /* GPIO 10 */
#define CHAN2 11 /* GPIO 11 */
#define CHAN3 12 /* GPIO 12 */
#define CHAN4 13 /* GPIO 13 */
#define CHAN5 14 /* GPIO 14 */
#define CHAN6 15 /* GPIO 15 */
#define CHAN7 16 /* GPIO 16 */

#define CHAN0BIT (1<<CHAN0) 
#define CHAN1BIT (1<<CHAN1)
#define CHAN2BIT (1<<CHAN2)
#define CHAN3BIT (1<<CHAN3)
#define CHAN4BIT (1<<CHAN4)
#define CHAN5BIT (1<<CHAN5)
#define CHAN6BIT (1<<CHAN6)
#define CHAN7BIT (1<<CHAN7)

#define CHAN_MASK (CHAN0BIT|CHAN1BIT|CHAN2BIT|CHAN3BIT|CHAN4BIT|CHAN5BIT|CHAN6BIT|CHAN7BIT)

extern uint32_t gpioBits[NCHAN];

// gpioSet is pointer to GPIO register to set a gpio output. 
// The instruction *gpioSet = x; will set the gpio outputs 
// according to the bits set in x. Bits to 0 are ignored.
// The instruction *gpioSet = 1 << 3; sets the gpio3 to 3.3v. 
// Other gpios are unaffected. Dereferencing this pointer 
// returns an undefined value.
extern volatile uint32_t *gpioSet;

// gpioClr is pointer to GPIO register to set a gpio output to 0.
// The instruction *gpioClr = x; will clear the gpio outputs 
// according to the bits set in x. Bits to 0 are ignored.
// The instruction *gpioClr = 1 << 3; sets the gpio3 to 0v. 
// Other gpios are unaffected. Dereferencing this pointer 
// returns an undefined value.
extern volatile uint32_t *gpioClr;

// gpio_init initializes the gpio and return NULL when it succeed.
// It returns 0 if the host is a supported raspberry device. It
// returns 1 if the host is not a raspberry device in which
// case the gpioSet and gpioClr will point to normal memory.
// Assigning values to gpioSet or gpioClr will then succeed, but
// without effect. This allows to test the program on non-raspberry
// devices. Returns 2 if the gpio is already initialized. 
// Returns -1 in case of error. In this case the gpio is not 
// functionnal and gpioSet and gpioClr are both null.
int gpio_init();


#endif // GPIO_H
