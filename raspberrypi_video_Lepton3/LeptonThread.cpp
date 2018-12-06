#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonThread.h"
#include "stdio.h"
#include "Palettes.h"
#include "Lepton_I2C.h"


#include "leptonpru.h"

#define HAND_TEMP_THRESHOLD 	8050

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
	leptonpru_mmap *frame_buffers[FRAMES_NUMBER];

	size_t psize = sysconf(_SC_PAGESIZE);
	int frame_size = sizeof(leptonpru_mmap);
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
	if (fd < 0) {
		perror("open");
		exit(0);
	}
	printf("fd = %d, FRAME_SIZE=%d\n", fd,frame_size);

	for(i=0; i<FRAMES_NUMBER; i++) {
		int off = ((i*frame_size+psize-1)/psize) * psize;
		frame_buffers[i] = (leptonpru_mmap*)mmap(NULL, frame_size, PROT_READ, MAP_SHARED, fd, off);
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

		leptonpru_mmap *frameBuffer = frame_buffers[cc];
		uint16_t minValue = frameBuffer->min_val;
		uint16_t maxValue = frameBuffer->max_val;
		float diff =  maxValue - minValue;
//		printf("read: err=%d cc=%d, min=%d, max=%d\n",err,cc,minValue,maxValue);

		//If the difference between Max and Min is 0, we need to get a new frame before emitting.
		if(minValue != maxValue){
		    float scale = 255/diff;
		    QRgb color;

		    for(int i=0; i<IMAGE_HEIGHT; i++) {
			    for(int j=0; j<IMAGE_WIDTH; j++) {
			
				value = (frameBuffer->image[i*IMAGE_WIDTH+j] - minValue) * scale;
				
				const int *colormap = colormap_ironblack;
				color = qRgb(colormap[3*value], colormap[3*value+1], colormap[3*value+2]);
				
				myImage.setPixel(j, i, color);
					
			    }
		    }

		    //Emit the finalized image for update.
		    emit updateImage(myImage);
		}
		cc = 1;
		err = write(fd,&cc,1);
//		printf("write: err1=%d cc=%d\n",err,cc);

	}
	
}

void LeptonThread::performFFC() {
	lepton_perform_ffc();
}
