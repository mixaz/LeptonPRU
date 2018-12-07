#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonThread.h"
#include "stdio.h"
#include "Palettes.h"
#include "Lepton_I2C.h"

#include "LeptonPruLib.h"

#define HAND_TEMP_THRESHOLD 	8050

int readyToToggle = 1;

LeptonThread::LeptonThread() : QThread(){}

LeptonThread::~LeptonThread() {}

void LeptonThread::run()
{
	//Create the initial image and open the Spi port.
	myImage = QImage(160, 120, QImage::Format_RGB888);

	unsigned int value;

	int fd,err;
	LeptonPruContext ctx;
	
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open");
		assert(0);
	}

        if(LeptonPru_init(&ctx,fd) < 0) {
		perror("LeptonPru_init");
		assert(0);
	}

	//Camera Loop
	while(true) {

		err = LeptonPru_next_frame(&ctx);
		if(err < 0) {
			perror("LeptonPru_next_frame");
			assert(0);
		}

		leptonpru_mmap *frameBuffer = ctx.curr_frame;
		uint16_t minValue = frameBuffer->min_val;
		uint16_t maxValue = frameBuffer->max_val;
		float diff =  maxValue - minValue;

		//If the difference between Max and Min is 0, we need to get a new frame before emitting.
		if(minValue != maxValue){
		    float scale = 255/diff;
		    QRgb color;

		    for(int i=0; i<IMAGE_HEIGHT; i++) {
			    for(int j=0; j<IMAGE_WIDTH; j++) {
			
				value = (frameBuffer->image[i*IMAGE_WIDTH+j] - minValue) * scale;
				if(value > 255) 
					value = 255;
				
				const int *colormap = colormap_ironblack;
				color = qRgb(colormap[3*value], colormap[3*value+1], colormap[3*value+2]);
				
				myImage.setPixel( j, i, color);
					
			    }
		    }

		    //Emit the finalized image for update.
		    emit updateImage(myImage);
		}

	}
	
}

void LeptonThread::performFFC() {
	lepton_perform_ffc();
}
