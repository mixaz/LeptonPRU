#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "../include/lepton.h"

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

static void print_frame1(uint16_t *buf) {
	int i,j,nn;
	printf("----------------\n");
	for(i=0; i<NUMBER_OF_SEGMENTS*PACKETS_PER_SEGMENT; i++) {
		nn = i*PACKET_SIZE_UINT16;
		printf("%03d-[%04x %04x] ",i,buf[nn],buf[nn+1]);
		for(j=2;j<PACKET_SIZE_UINT16;j++)  {
			printf("%04x ",buf[nn+j]);
		}
		printf("\n");
	}
}

int main() {
	int fd,i;
	unsigned int cc = 0;
	int err;
	uint16_t *frame_buffers[FRAMES_NUMBER];
	
	size_t psize = sysconf(_SC_PAGESIZE);
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
	if (fd < 0) {
		perror("open");
		assert(0);
	}
	printf("fd = %d, FRAME_SIZE=%d\n", fd,FRAME_SIZE);

	for(i=0; i<FRAMES_NUMBER; i++) {
		int off = ((i*FRAME_SIZE+psize-1)/psize) * psize;
		frame_buffers[i] = mmap(NULL, FRAME_SIZE, PROT_READ, MAP_SHARED, fd, off);
		if (frame_buffers[i] == MAP_FAILED) {
			perror("mmap");
			assert(0);
		}
		printf("%d: %x\n", i, frame_buffers[i]);
	}

	int nn = 0;
	
	while(1) {
		err = read(fd,&cc,1);
		printf("read: err=%d cc=%d\n",err,cc);
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
		print_frame1(frame_buffers[cc]);
		cc = 1;
		err = write(fd,&cc,1);
		printf("write: err=%d cc=%d\n",err,cc);
		
		if(nn++ >= 10)
			break;
	}

	for(i=0; i<FRAMES_NUMBER; i++) {
		if (munmap(frame_buffers[i], FRAME_SIZE)) {
			perror("munmap");
			assert(0);
		}
	}

	close(fd);

	return 0;
}

