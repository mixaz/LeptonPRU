/* led.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define OE_ADDR 0x134
#define GPIO_DATAOUT 0x13C
#define GPIO_DATAIN 0x138
#define GPIO0_ADDR 0x44E07000
#define GPIO1_ADDR 0x4804C000
#define GPIO2_ADDR 0x481AC000
#define GPIO3_ADDR 0x481AF000

int main(int argc, char * argv[] )
{
	volatile uint32_t * gpio0;
	int fp;

	fp = open( "/dev/mem", O_RDWR | O_SYNC );
	if ( fp < 0 ) {
		printf("can not open mem\n");
		return EXIT_FAILURE;
	}
	gpio0 = (uint32_t *)mmap( NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fp, GPIO0_ADDR );

	printf("gpio0[5] = %d\n", (gpio0[GPIO_DATAIN/4] & (1 << 5)) ? 1 : 0);

	close(fp);
	return EXIT_SUCCESS;
}

