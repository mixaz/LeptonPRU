#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonThread.h"
#include "stdio.h"
#include "Palettes.h"
#include "Lepton_I2C.h"


#include "lepton.h"

#define HAND_TEMP_THRESHOLD 8050

int readyToToggle = 1;

LeptonThread::LeptonThread() : QThread(){}

LeptonThread::~LeptonThread() {}

void LeptonThread::run()
{
	//Create the initial image and open the Spi port.
	myImage = QImage(160, 120, QImage::Format_RGB888);

	int column,row,value;

	struct timespec time_now;
	unsigned long int t_now = 0; //ms
	unsigned long int t_interval = 0; //ms

	int fd,i;
	unsigned int cc = 0;
	int err;
	uint16_t *frame_buffers[FRAMES_NUMBER];

	size_t psize = sysconf(_SC_PAGESIZE);
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
	if (fd < 0) {
		perror("open");
		exit(0);
	}
	printf("fd = %d, FRAME_SIZE=%d\n", fd,FRAME_SIZE);

	for(i=0; i<FRAMES_NUMBER; i++) {
		int off = ((i*FRAME_SIZE+psize-1)/psize) * psize;
		frame_buffers[i] = (uint16_t*)mmap(NULL, FRAME_SIZE, PROT_READ, MAP_SHARED, fd, off);
		if (frame_buffers[i] == MAP_FAILED) {
			perror("mmap");
			exit(0);
		}
		printf("%d: %x\n", i, frame_buffers[i]);
	}

	QRgb red = qRgb(255,0,0);

	//Camera Loop
	while(true) {

		err = read(fd,&cc,1);
		uint16_t *frameBuffer = frame_buffers[cc];
		uint16_t minValue = frameBuffer[0];
		uint16_t maxValue = frameBuffer[1];
		float diff =  maxValue - minValue;
//		printf("read: err=%d cc=%d, min=%d, max=%d\n",err,cc,minValue,maxValue);
		if(err == 0) {
			sleep(1);
			continue;
		}
		if(err < 0) {
			printf("error detected, exiting\n");
			break;
		}
		if(cc >= FRAMES_NUMBER) {
			printf("unexpected frame number, exiting\n");
			break;
		}

		//If the difference between Max and Min is 0, we need to get a new frame before emitting.
		if(minValue != maxValue){
			float scale = 255/diff;
			QRgb color;
/*
			//Iterates through the entire frame, all four segments, for colorization.
			for(int i=0;i<SEGMENT_SIZE_UINT16*4;i++) {
				//Skip the header of each line.
				if(i % PACKET_SIZE_UINT16 < 2) continue; 
				
				value = (frameBuffer[i] - minValue) * scale;
				const int *colormap = colormap_ironblack;
				if(value > 255) value = 255;	
				color = qRgb(colormap[3*value], colormap[3*value+1], colormap[3*value+2]);
			
				column = (i % PACKET_SIZE_UINT16 ) - 2;
				row = i / (PACKET_SIZE_UINT16);
				int new_row = (row/2);
				int new_column = (row % 2 == 0) ? column : column + 80 ;
				myImage.setPixel(new_column, new_row, color);
			}
*/
		for(int k=0; k<FRAME_SIZE_UINT16; k++) {
			if(k % PACKET_SIZE_UINT16 < 2) {
				continue;
			}
		
			value = (frameBuffer[k] - minValue) * scale;
			
			const int *colormap = colormap_ironblack;
			color = qRgb(colormap[3*value], colormap[3*value+1], colormap[3*value+2]);
			
				if((k/PACKET_SIZE_UINT16) % 2 == 0){//1
					column = (k % PACKET_SIZE_UINT16 - 2);
					row = (k / PACKET_SIZE_UINT16)/2;
				}
				else{//2
					column = ((k % PACKET_SIZE_UINT16 - 2))+(PACKET_SIZE_UINT16-2);
					row = (k / PACKET_SIZE_UINT16)/2;
				}	
								
				myImage.setPixel(column, row, color);
				
		}

			for(int i=0;i<160;i++) {
				myImage.setPixel(i, 10, red);
				myImage.setPixel(i, 110, red);
			}
			for(int i=0;i<120;i++) {
				myImage.setPixel(10, i, red);
				myImage.setPixel(150, i, red);
			}
			//Emit the finalized image for update.
			emit updateImage(myImage);
/*
			clock_gettime(CLOCK_MONOTONIC, &time_now);
			t_interval = time_now.tv_sec*1000 + time_now.tv_nsec/1000000-t_now;
			t_now = time_now.tv_sec*1000 + time_now.tv_nsec/1000000;
			printf("Time since last image %lu\n",t_interval);
*/
		}
		cc = 1;
		err = write(fd,&cc,1);
//		printf("write: err=%d cc=%d\n",err,cc);

	}
	
}

void LeptonThread::performFFC() {
	lepton_perform_ffc();
}
