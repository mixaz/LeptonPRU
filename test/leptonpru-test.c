#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "../include/leptonpru.h"
#include "../include/leptonpru_int.h"

static void print_frame(char *buf, int size) {
	int cc;
	int count;
	while(size--) {
		cc = *buf++;
		count = 1;
		while(size && *buf == cc) {
			count++;
			size--;
			buf++;
		}
		printf("%2x:%d ",cc,count);
	}
	printf("\n");
}

#ifndef RAW_DATA
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
#else
static void print_frame1(uint16_t *buf) {
	int i,j,k,nn;
	int h0,h1;
	printf("----------------\n");
	nn = 0;
	for(k=0; k<NUMBER_OF_SEGMENTS; k++) {
		printf("segment: %d\n",k);
		for(i=0; i<PACKETS_PER_SEGMENT; i++) {
			h0 = buf[nn++];
			h1 = buf[nn++];
			printf("%03d: [%04x-%04x] ",i,h0,h1);
			for(j=2;j<PACKET_SIZE_UINT16;j++)  {
//				if(j < 10)
				    printf("%04x ",buf[nn]);
				nn++;
			}
			printf("\n");
		}
	}
}
#endif

int main() {
	int fd,i;
	unsigned int cc = 0;
	int err;
	leptonpru_mmap *frame_buffers[FRAMES_NUMBER];
	
	size_t psize = sysconf(_SC_PAGESIZE);
	int frame_size = sizeof(leptonpru_mmap);
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
	if (fd < 0) {
		perror("open");
		assert(0);
	}
	printf("fd = %d, FRAME_SIZE=%d, page_size: %d\n", fd,frame_size,psize);

	for(i=0; i<FRAMES_NUMBER; i++) {
		int off = ((i*frame_size+psize-1)/psize) * psize;
		frame_buffers[i] = mmap(NULL, frame_size, PROT_READ, MAP_SHARED, fd, off);
		if (frame_buffers[i] == MAP_FAILED) {
			perror("mmap");
			assert(0);
		}
		printf("%d: %x\n", i, frame_buffers[i]);
	}

	int nn = 0;
	
	while(1) {
		err = read(fd,&cc,1);
		printf("read: nn=%d err=%d cc=%d\n",nn,err,cc);
		if(err == 0) {
			sleep(1);
			continue;
		}
		if(err < 0) {
			printf("error %d detected, exiting\n",err);
			break;
		}
		if(cc >= FRAMES_NUMBER) {
			printf("unexpected frame number, exiting\n");
			break;
		}
		print_frame1(frame_buffers[cc]->image);
		cc = 1;
		err = write(fd,&cc,1);
		printf("write: nn=%d err=%d cc=%d\n",nn,err,cc);
		
		if(nn++ >= 4)
			break;
	}

	printf("releasing buffers\n");
	
	for(i=0; i<FRAMES_NUMBER; i++) {
		printf("unmap buffer:%d\n",i);
		if (munmap(frame_buffers[i], frame_size)) {
			perror("munmap");
			assert(0);
		}
	}

	printf("closing fd\n");
	close(fd);

	printf("quit\n");
	return 0;
}

