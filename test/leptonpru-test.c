#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonPruLib.h"

static void print_frame1(uint16_t *buf) {
	int i,j,nn;
	printf("----------------\n");
	nn = 0;
	for(i=0; i<IMAGE_HEIGHT; i++) {
		printf("%03d: ",i);
		for(j=0;j<IMAGE_WIDTH;j++)  {
			if(j < 10)
			    printf("%04x ",buf[nn]);
			nn++;
		}
		printf("\n");
	}
}

int main() {
	int fd, err;
	LeptonPruContext ctx;
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
	if (fd < 0) {
		perror("open");
		assert(0);
	}

        if(LeptonPru_init(&ctx,fd) < 0) {
		perror("LeptonPru_init");
		assert(0);
	}

        int nn = 0;
		
	while(1) {
		
		err = LeptonPru_next_frame(&ctx);
		if(err < 0) {
			perror("LeptonPru_next_frame");
			break;
		}
		if(err == 0) {
			sleep(1);
			continue;
		}
		printf("frame:%d min: %d, max: %d",
			nn,ctx.curr_frame->min_val,ctx.curr_frame->max_val);
		print_frame1(ctx.curr_frame->image);
		
		if(nn++ >= 4)
			break;
	}

        if(LeptonPru_release(&ctx) < 0) {
		perror("LeptonPru_release");
	}

	close(fd);

	return 0;
}

