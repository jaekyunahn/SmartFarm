#include <unistd.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <raspicam/raspicam.h>
#include <cstring>

#include "camera.h"

#define GRAY_LEVEL	1
#define RGB_LEVEL	3

using namespace std;

raspicam::RaspiCam Camera;

extern "C"{
	int fCameraInitFunction(int width, int height, int colortype){
		if (colortype == GRAY_LEVEL){
			// color type을 gray로 받을 시 
			Camera.setFormat(raspicam::RASPICAM_FORMAT_GRAY);
		}
		else if (colortype == RGB_LEVEL){
			// color type을 bgr로 받을 시 
			Camera.setFormat(raspicam::RASPICAM_FORMAT_BGR);
		}

		// piCamera width
		Camera.setWidth(width);
		// piCamera height
		Camera.setHeight(height);

		// camera open error시
		if (!Camera.open()){
			cerr << "Error opening camera" << endl;
			return -1;
		}

		// 라즈베리파이 카메라 모듈 init후 안정화에 필요한 권장 시간 3초
		cout << "Sleeping for 3 secs" << endl;
		sleep(3);
		cout << "OK" << endl;

		return 0;
	}

	// camera data
	unsigned char* data;

	unsigned char* fCameraShotFunction(void){
		// 기존의 data를 정리
		delete data;
		// camera shot
		Camera.grab();
		// 촬영한 사진 데이터를 가져온다. BGR format
		data = new unsigned char[Camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_BGR)];
		Camera.retrieve(data, raspicam::RASPICAM_FORMAT_BGR);
		// data 반환
		return data;
	}
}






