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
			// color type�� gray�� ���� �� 
			Camera.setFormat(raspicam::RASPICAM_FORMAT_GRAY);
		}
		else if (colortype == RGB_LEVEL){
			// color type�� bgr�� ���� �� 
			Camera.setFormat(raspicam::RASPICAM_FORMAT_BGR);
		}

		// piCamera width
		Camera.setWidth(width);
		// piCamera height
		Camera.setHeight(height);

		// camera open error��
		if (!Camera.open()){
			cerr << "Error opening camera" << endl;
			return -1;
		}

		// ��������� ī�޶� ��� init�� ����ȭ�� �ʿ��� ���� �ð� 3��
		cout << "Sleeping for 3 secs" << endl;
		sleep(3);
		cout << "OK" << endl;

		return 0;
	}

	// camera data
	unsigned char* data;

	unsigned char* fCameraShotFunction(void){
		// ������ data�� ����
		delete data;
		// camera shot
		Camera.grab();
		// �Կ��� ���� �����͸� �����´�. BGR format
		data = new unsigned char[Camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_BGR)];
		Camera.retrieve(data, raspicam::RASPICAM_FORMAT_BGR);
		// data ��ȯ
		return data;
	}
}






