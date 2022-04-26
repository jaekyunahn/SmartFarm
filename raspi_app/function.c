/*
 *	Include
 */
#include "main.h"

/*
 *	Define
 */
#define GRAY_LEVEL	1
#define RGB_LEVEL	3

#define CAMERA_X_SIZE	320
#define CAMERA_Y_SIZE	240
#define CAMERA_C_SIZE	RGB_LEVEL

#define FRMAEBUFFERSIZE CAMERA_X_SIZE * CAMERA_Y_SIZE * CAMERA_C_SIZE

#define DEBUG_LOG	1

/*
 *	Function.c Global Variable
 */
//input thread getdata
volatile unsigned char* sCameraData;
volatile unsigned char sCameraShotCommand = 0;

//
unsigned char CameraDataBuffer[FRMAEBUFFERSIZE] = { 0x00, };

struct mosquitto* pubmosq;
int mqtt_send_data_len = 0;
char mqtt_send_data[1024] = "";
char mqtt_send_data_topic[1024] = "";

char sendCameraData[FRMAEBUFFERSIZE] = "";

//송신 할 데이터
volatile char   Tx_buf[32];

// 송신 할 버퍼 크기
volatile int    iTxBufferCounter = 0;

//UART RX Buffer
volatile char   Rx_buf[1024];

// 수신 버퍼 크기
volatile int    iRxBufferCounter = 0;

// 엑츄에이터에서 통신 할 준비가 되었는지 여부
volatile int ready_arduino = 0;

// 사용자 수동제어 여부
volatile int user_control = 0;

//Serial Thread로 부터 데이터 수신 받으면 적용
volatile char rx_data_global[1024] = "";
volatile int incoming_data = 0;

// 지정 해 둔 개행문자 단위로 Parsing
char parsingData[32][256] = { {0x00,}, };

// MQTT 접속에 필요한 정보 저장 변수
char server_address[256] = "";
int server_port = 0;
char server_id[256] = "";
char server_pw[256] = "";

// 비트맵 헤더 선언
struct BMP_HEADER_STRUCT
{
	unsigned short	iMagicNumber;
	unsigned int 	iBitmapFileSize;
	unsigned short	bfReserved1;
	unsigned short	bfReserved2;
	unsigned int 	iButmapRealDataOffsetValue;
	unsigned int 	iBackendHeaderSize;
	unsigned int 	iBitmapXvalue;
	unsigned int 	iBitmapYvalue;
	unsigned short 	iColorPlane;
	unsigned short 	iPixelBitDeepValue;
	unsigned int 	iCompressType;
	unsigned int 	iImageSize;
	unsigned int 	iPixcelPerMeterXvalue;
	unsigned int 	iPixcelPerMeterYvalue;
	unsigned int 	iColorPalet;
	unsigned int 	iCriticalColorCount;
};

// union type으로 배열 <-> 정수형 데이터 변환
union convert_int_string
{
	unsigned int int32data;
	char sdata[4];
};
union convert_short_string
{
	unsigned short shortdata;
	char sdata[2];
};

/*
 *	EXT function
 */
int fCameraInitFunction(int width, int height, int colortype);
unsigned char* fCameraShotFunction(void);

// Application 주 진입 점
int fAppFunctionEntryPoint(int argc, char ** argv){
	int res = 0;
	
	//init Camera
	res = fCameraInitFunction(CAMERA_X_SIZE, CAMERA_Y_SIZE, CAMERA_C_SIZE);
	if (res != 0){
		printf("fCameraInitFunction Error! :%d\r\n", res);
		return -1;
	}

	//read config file
	read_config_file();

	//pThread Init & Start
	res = fPthreadInitFunction();
	if (res != 0){
		printf("fPthreadInitFunction Error! :%d\r\n",res);
		return -1;
	}

	while (1){
		// Thread 동작 하는 동안 main Thread는 대기
	}

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//serial thread
void* tSerialThreadFunction(void* data) {
	pid_t pid;            // process id
	pthread_t tid;        // thread id

	pid = getpid();
	tid = pthread_self();

	char* thread_name = (char*)data;

	printf("[Serial]Start Serial Thread :%s\r\n", thread_name);

	/*
	 *	Serial Thread
	 */
	int serial_fd;

	unsigned char sSerialPortName[64] = "";
	int iSerialPortIndex = 0;

	// 표준 인터페이스 구조체
	struct termios    uart_io;
	// 체크할 event 정보를 갖는 struct
	struct pollfd     poll_events;

	char buf[64];
	char rx_data[1024];

	// 수신 받은 패킷 크기
	// 패킷이 잘려서 오는 경우 누적 하기 위한 변수
	int serial_rx_index = 0;
	// 수신 받은 문자열 크기
	int cnt;

	int x = 0;
	int y = 0;

	// 시리얼 포트 open
loop:
	memset(sSerialPortName, 0x00, sizeof(sSerialPortName));

#if 1
	// ttyACMx
	sprintf(sSerialPortName, "/dev/ttyACM%d", iSerialPortIndex);
#else
	// ttyUSBx
	sprintf(sSerialPortName, "/dev/ttyUSB%d", iSerialPortIndex);
#endif

	// 디바이스를 open 한다.
	serial_fd = open(sSerialPortName, O_RDWR | O_NOCTTY);
	if (0 > serial_fd) {
		printf("[Serial]open error:%s\n", sSerialPortName);
		// 재시도 특정 횟수 넘어가면 강제 종료
		if (iSerialPortIndex > 65535) {
			return -1;
		}
		else {
			iSerialPortIndex++;
			goto loop;
		}
	}

	// 시리얼 포트 통신 환경 설정
	memset(&uart_io, 0, sizeof(uart_io));
	/* control flags */
	uart_io.c_cflag = B57600 | CS8 | CLOCAL | CREAD;   //115200 , 8bit, 모뎀 라인 상태를 무시, 수신을 허용.
	/* output flags */
	uart_io.c_oflag = 0;
	/* input flags */
	uart_io.c_lflag = 0;
	/* control chars */
	uart_io.c_cc[VTIME] = 0;    //타이머의 시간을 설정
	uart_io.c_cc[VMIN] = 1;     //ead할 때 리턴되기 위한 최소의 문자 개수를 지정

	//라인 제어 함수 호출
	tcflush(serial_fd, TCIFLUSH);              // TCIFLUSH : 입력을 비운다
	// IO 속성 선택
	tcsetattr(serial_fd, TCSANOW, &uart_io);   // TCSANOW : 속성을 바로 반영
	// 시리얼 포트 디바이스 드라이버파일을 제어
	fcntl(serial_fd, F_SETFL, FNDELAY);        // F_SETFL : 파일 상태와 접근 모드를 설정

	// poll 사용을 위한 준비   
	poll_events.fd = serial_fd;
	poll_events.events = POLLIN | POLLERR;          // 수신된 자료가 있는지, 에러가 있는지
	poll_events.revents = 0;

	usleep(1000);
	// 1차레 리셋 신호 보내서 동기화
	serial_send("RESET#", 7);
	usleep(1000);

	memset(buf, 0x00, sizeof(buf));
	memset(rx_data, 0x00, sizeof(rx_data));

	while (1) {
		memset(buf, 0x00, sizeof(buf));
		cnt = read(serial_fd, buf, sizeof(buf));
		if (cnt > 1) {
			for (x = (cnt - 1); x >= 0; x--) {
				// 사람이 볼 수 있는 문자열만 받는다
				if ((buf[x] >= 0x21) && (buf[x] <= 0x7E)) {
					if ((x == 0) && (buf[x] != '\n')) {
						//전수 누적
						printf("[Serial]buf[%d]:%s\r\n", cnt, buf);
						memcpy(rx_data + serial_rx_index, buf, sizeof(char) * cnt);
						serial_rx_index = serial_rx_index + cnt;
					}
					if (buf[x] == '#') {
						//누적 종료, 인덱스 초기화
						memcpy(rx_data + serial_rx_index, buf, sizeof(char) * cnt);
						//Todo
						memset(rx_data_global, 0x00, sizeof(rx_data_global));
						printf("[Serial]rx_data[%d]:%s\r\n", serial_rx_index, rx_data);
						memcpy(rx_data_global, rx_data, sizeof(rx_data));
						// 만약 엑츄에이터 에서 준비가 완료된 문자를 보내면
						if (fCompareFunction(rx_data, "START", 5)) {
							printf("[Serial]Arduino online!!\r\n");
							// 사용 준비 완료 플래그 설정
							ready_arduino = 1;
						}
						// 수신 관련 변수 초기화
						serial_rx_index = 0;
						memset(rx_data, 0x00, sizeof(rx_data));
						memset(buf, 0x00, sizeof(buf));
						incoming_data = 1;
					}
				}
			}
		}

		//write할 데이터 존재시
		if (iTxBufferCounter != 0) {
			// 시리얼 데이터 전송
			cnt = write(serial_fd, (const void*)Tx_buf, sizeof(char) * iTxBufferCounter);
			// 데이터 전송이 끝나면 보낼 데이터 크기 변수 초기화
			iTxBufferCounter = 0;
		}
	}
}

// MQTT Sub Thread
void* tSubThreadFunction(void* data){
	pid_t pid;            // process id
	pthread_t tid;        // thread id

	pid = getpid();
	tid = pthread_self();

	char* thread_name = (char*)data;

	printf("Start Sub Thread :%s\r\n", thread_name);

	/*
	 *	Sub Thread
	 */
	sub();
}

void* t_function_CameraShotThread(void* data){
	pid_t pid;            // process id
	pthread_t tid;        // thread id

	pid = getpid();
	tid = pthread_self();

	char* thread_name = (char*)data;

	printf("Start Thread :%s\r\n", thread_name);

	/*
	 *	Camera Thread
	 */
	while (1) {
		//카메라 촬영 요청이 들어오면
		if (sCameraShotCommand == 1) {
			// shot
			sCameraData = fCameraShotFunction();
			// 요청 플래그 제거
			sCameraShotCommand = 0;
		}
	}
	printf("%s:Exit\r\n", thread_name);
}

// 자동 제어 함수
void* tAutoContolThreadFunction(void* data) {
	pid_t pid;            // process id
	pthread_t tid;        // thread id

	pid = getpid();
	tid = pthread_self();

	char* thread_name = (char*)data;

	printf("Start Auto Thread :%s\r\n", thread_name);

	/*
	 *	Thread
	 */

	int res = 0;
	int x, y;

	// 시리얼 데이터 수신 받은 내용 저장
	char rx_data[1024] = "";

	// 시리얼 데이터 endchar 위치
	int iEndcharIndex = 0;

	int tmp = 0;
	int hum = 0;
	int water = 0;
	int light = 0;
	int led = 0;
	int pan = 0;
	int window = 0;

	// parsing 횟수
	int parsingcount = 0;
	// parsing 결과 받는 변수
	char* parsing_result;

	while (1){
		if( (ready_arduino == 1) && (user_control == 0)){
			// 전송중인 데이터가 있는지 확인
			while (1){
				if (iTxBufferCounter == 0) {
					break;
				}
			}
			// 현재 데이터 요청
			res = serial_send("getData#", 9);
			//데이터 날라오는지 감지
			incoming_data = 0;
			while (1) {
				if (incoming_data == 1) {
					break;
				}
			}
			// 분석
			// Find endchar
			for (x = 0 ; x < sizeof(rx_data) ; x++){
				if ((rx_data_global[0] == 'D') && (rx_data_global[x] == '#')){
					memset(rx_data, 0x00, sizeof(rx_data));
					memcpy(rx_data, rx_data_global + 1, sizeof(char) * (x - 1));
					printf("[auto]rx_data=%s\r\n", rx_data);
				}
			}

			// 데이터 파싱
			parsingcount = 0;
			parsing_result = strtok(rx_data,",");
			// 자른 문자열이 나오지 않을 때까지 반복
			while (parsing_result != NULL){
				// 자른 문자열 출력
				//printf("%s\n", parsing_result);     
				switch (parsingcount){
				case 0:
					tmp = fConvertStringToInt32(parsing_result);
					break;
				case 1:
					hum = fConvertStringToInt32(parsing_result);
					break;
				case 2:
					water = fConvertStringToInt32(parsing_result);
					break;
				case 3:
					light = fConvertStringToInt32(parsing_result);
					break;
				case 4:
					led = fConvertStringToInt32(parsing_result);
					break;
				case 5:
					pan = fConvertStringToInt32(parsing_result);
					break;
				case 6:
					window = fConvertStringToInt32(parsing_result);
					if (window > 0){
						window = 1;
					}
					break;
				default:
					break;
				}

				// 다음 문자열을 잘라서 포인터를 반환
				parsing_result = strtok(NULL, ",");    
				parsingcount++;
			}

			// 제어
#if DEBUG_LOG
			printf("tmp   =%d\r\n", tmp);
			printf("hum   =%d\r\n", hum);
			printf("water =%d\r\n", water);
			printf("light =%d\r\n", light);
			printf("led   =%d\r\n", led);
			printf("pan   =%d\r\n", pan);
			printf("window=%d\r\n", window);
#endif
			// tmp 이 높다 -> pan과 window 동작
			if (tmp > 25){
				if (pan==0){
					//pan동작
					serial_send("CMD:PAN=1#", 10);
#if DEBUG_LOG
					printf("Pan On\r\n");
#endif
				}
				if (window == 0){
					//window open
					serial_send("CMD:WND=1#", 10);
#if DEBUG_LOG
					printf("window Open\r\n");
#endif
				}
			}
			else{
				if (pan == 1){
					//pan정지
					serial_send("CMD:PAN=0#", 10);
#if DEBUG_LOG
					printf("Pan Off\r\n");
#endif
				}
				if (window == 1){
					//window close
					serial_send("CMD:WND=0#", 10);
#if DEBUG_LOG
					printf("window Close\r\n");
#endif
				}
			}

			// 저녁 7시~ 익일 8시 까지 자동 On
			printf("getnowtime()=%d\r\n", getnowtime());
			if ((getnowtime() < 8) || (getnowtime() >= 16)){
				if (led == 0){
					//led동작
					serial_send("CMD:LED=1#", 10);
#if DEBUG_LOG
					printf("LED On\r\n");
#endif
				}
			}
			else{
				if (led == 1){
					//led정지
					serial_send("CMD:LED=0#", 10);
#if DEBUG_LOG
					printf("LED Off\r\n");
#endif
				}
			}
		}

		//10초 마다 읽고 자동 제어 할 수 있도록
		sleep(10);
	}
}

/*
 *	pThread
 */
int fPthreadInitFunction(void){
	//pthread variable
	pthread_t p_thread[4];
	int thr_id;

	//Thread Name
	char p1[] = "thread_Camera_Shot";	// 0번 스레드 이름 : Camera Shot 담당
	char p2[] = "thread_serial";		// 1번 스레드 이름 : serial 담당
	char p3[] = "thread_mqtt_sub";		// 2번 스레드 이름 : mqtt_sub 담당
	char p4[] = "thread_auto_control";		// 3번 스레드 이름 : Auto control 담당

	//Camera Thread
	thr_id = pthread_create(&p_thread[0], NULL, t_function_CameraShotThread, (void*)p1);
	if (thr_id < 0)
	{
		perror("thread create error : ");
		return -1;
	}

	//serial Thread
	thr_id = pthread_create(&p_thread[1], NULL, tSerialThreadFunction, (void*)p2);
	if (thr_id < 0) {
		perror("thread create error : ");
		return -1;
	}

	// Sub thread
	thr_id = pthread_create(&p_thread[2], NULL, tSubThreadFunction, (void*)p3);
	if (thr_id < 0) {
		perror("thread create error : ");
		return -1;
	}

	// auto control thread
	thr_id = pthread_create(&p_thread[3], NULL, tAutoContolThreadFunction, (void*)p4);
	if (thr_id < 0) {
		perror("thread create error : ");
		return -1;
	}

	//Start Pthread
	pthread_detach(p_thread[0]);
	pthread_detach(p_thread[1]);
	pthread_detach(p_thread[2]);
	pthread_detach(p_thread[3]);

	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);
	pthread_join(p_thread[2], NULL);
	pthread_join(p_thread[3], NULL);

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int serial_send(const char* sPacketData, int count) {
	memcpy(Tx_buf, sPacketData, sizeof(char) * count);
	iTxBufferCounter = count;
	return count;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------


void on_connect(struct mosquitto* mosq, void* obj, int rc) {
	printf("ID: %d\n", *(int*)obj);
	if (rc) {
		printf("Error with result code: %d\n", rc);
		exit(-1);
	}
	mosquitto_subscribe(mosq, NULL, "server_call", 0);
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
	int res = 0;
	int image_res = 0;

#if DEBUG_LOG
	printf("[MQTT_Sub]New message[topic:%s,qos:%d]:%s\n", msg->topic, msg->qos, (char*)msg->payload);
#endif

	if (ready_arduino == 1) {
		// 통신중인 데이터 유무 확인
		//iTxBufferCounter
		while (1)
		{
			if (iTxBufferCounter == 0) {
				break;
			}
		}

		// 현재 팜 정보 요청이 들어오면
		if (fCompareFunction((char*)msg->payload, "GET_DATA", 8)) {
			res = serial_send("getData#", 9);
		}

		// LED On/Off 요청
		else if (fCompareFunction((char*)msg->payload, "LED_ON", 6)) {
			res = serial_send("CMD:LED=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "LED_OFF", 7)) {
			res = serial_send("CMD:LED=0#", 10);
		}

		// Pan On/Off 요청
		else if (fCompareFunction((char*)msg->payload, "PAN_ON", 6)) {
			res = serial_send("CMD:PAN=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "PAN_OFF", 7)) {
			res = serial_send("CMD:PAN=0#", 10);
		}

		// Widnow Open/close 요청
		else if (fCompareFunction((char*)msg->payload, "WINDOW_ON", 9)) {
			res = serial_send("CMD:WND=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "WINDOW_OFF", 10)) {
			res = serial_send("CMD:WND=0#", 10);
		}

		// 사용자 자동제어 여부
		else if (fCompareFunction((char*)msg->payload, "USER_CON_ON", 11)) {
			user_control = 1;
			return;
		}
		else if (fCompareFunction((char*)msg->payload, "USER_CON_OFF", 12)) {
			user_control = 0;
			return;
		}

		else if (fCompareFunction((char*)msg->payload, "CAMERA", 6)) {
			//Camera Shot 요청
			sCameraShotCommand = 1;
			while (1){
				// 사진 데이터 획득 완료 될 때 까지 대기
				if (sCameraShotCommand == 0){
					break;
				}
			}

			// 1초 대기
			usleep(1000);

			//Save File
			save_bitmap();

			//convert jpg
			// 기존에 있던 jpg 제거
			popen_function("rm /home/pi/ramdisk/image.jpg");
			// jpg 변환 라이브러리 사용
			popen_function("convert /home/pi/ramdisk/image.bmp /home/pi/ramdisk/image.jpg");

			// 1초 대기
			usleep(1000);

			// read file
			memset(sendCameraData, 0x00, sizeof(sendCameraData));
			image_res = fFileLoadFunction("/home/pi/ramdisk/image.jpg", sendCameraData);
			// 사진 전송
			res = send_pub("response_image", sizeof(char) * image_res, sendCameraData);
#if DEBUG_LOG
			printf("[MQTT_Sub]image(%dbyte) res=%d\r\n", image_res, res);
#endif

			return;
		}

#if DEBUG_LOG
		printf("[MQTT_Sub]serial_send=%d\r\n", res);
#endif

		incoming_data = 0;
		while (1) {
			if (incoming_data == 1) {
				break;
			}
		}
		res = send_pub("response_text", sizeof(rx_data_global), rx_data_global);
#if DEBUG_LOG
		printf("[MQTT_Sub]res=%d\r\n", res);
		printf("[MQTT_Sub]res=%s\r\n", rx_data_global);
#endif
	}
}

int sub(void) {
	int rc, id = 12;

	// mosquitto 라이브러리 초기화
	mosquitto_lib_init();

	struct mosquitto* mosq;

	// sub 객체 생성
	mosq = mosquitto_new("subscribe-test", true, &id);
	// broker(서버)와 연결하기 위한 callback함수 등록
	mosquitto_connect_callback_set(mosq, on_connect);
	// pub로 부터 수신데이터를 받기 위한 callback함수 등록
	mosquitto_message_callback_set(mosq, on_message);
	// broker에 등록한 계정 인증 
	mosquitto_username_pw_set(mosq, server_id, server_pw);

	// broker에 접속
	rc = mosquitto_connect(mosq, server_address, server_port, 10);
	if (rc) {
		printf("[MQTT_Sub]Could not connect to Broker with return code %d\n", rc);
		return -1;
	}
	
	// pub로 부터 데이터를 수신하기 위해 대기. 수신데이터가 감지 될 때마다 callback 함수가 동작하게 되어 있다
	mosquitto_loop_start(mosq);

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

int send_pub(const char* topic, int data_len, const void* payload) {
	int rc;
	// MQTT lib init
	mosquitto_lib_init();
	// 구조체 할당
	//pubmosq = mosquitto_new("publisher-test", true, NULL);
	pubmosq = mosquitto_new(server_id, true, NULL);
	// MQTT 브로커 (서버)에 등록된 ID와 PW를 구조체 적제
	mosquitto_username_pw_set(pubmosq, server_id, server_pw);
	// MQTT 브로커 연결 시도
	rc = mosquitto_connect(pubmosq, server_address, server_port, 10);
	// 연결이 안되면
	if (rc != 0) {
		printf("[MQTT_Pub]Client could not connect to broker! Error Code: %d\n", rc);
		mosquitto_destroy(pubmosq);
		rc = -1;
	}
	// 연결이 되면
	else {
		// 데이터 전달, topic에 맞게 전달 해야 구독자들 한테 데이터 분류를 쉽게 해 줌
		rc = mosquitto_publish(pubmosq, NULL, topic, data_len, payload, 0, false);
		//연결 종료
		mosquitto_disconnect(pubmosq);
		// 메모리로 부터 구조체 인스턴스 제거
		mosquitto_destroy(pubmosq);
		// 사용한 라이브러리 메모리에서 제거
		mosquitto_lib_cleanup();
	}

	return rc;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

//문자열 비교
int fCompareFunction(char* source, char* target, int iSize) {
	int i = 0;
	for (i = 0; i < iSize; i++) {
		if (source[i] != target[i]) {
			return 0;
		}
	}
	return 1;
}

/*
 *  @brief  File Load Function
 *  @param  sFileName   File 주소
 *          sReadBuffer File Data 적재 할 배열 변수 시작 주소
 *  @retval 읽기 성공 시 File 크기 반환, 실패시 -1 return
 */
int fFileLoadFunction(unsigned char* sFileName, unsigned char* sReadBuffer) {
	unsigned int iFileSize = 0;
	FILE* fp = fopen(sFileName, "rb");
	if (fp == NULL) {
		printf("File Open Error:%s\r\n", sFileName);
		return -1;
	}
	fseek(fp, 0L, SEEK_END);
	iFileSize = (int)ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	fread(sReadBuffer, sizeof(char), sizeof(char) * iFileSize, fp);
	fclose(fp);
	return iFileSize;
}

/*
 *  @brief  문자열 형태의 숫자를 int32값으로 변환
 *  @param  source  문자열 에서 정수형으로 바꿀 문자열 시작 주소
 *  @retval int32로 변환 된 값
 */
int fConvertStringToInt32(char* source) {
	int buf = source[0] - 0x30;
	int res = 0;
	int i = 1;
	res = res + buf;
	while (1) {
		if ((source[i] >= 0x30) && (source[i] <= 0x39)) {
			res = res * 10;
			buf = source[i] - 0x30;
			res = res + buf;
		}
		else {
			break;
		}
		i++;
	}
	return res;
}

// 2바이트 정수 데이터를 char형식으로 쪼개기 위한 함수
int convertShortToString(unsigned short shortdata, char* targetstring){
	union convert_short_string convert;
	convert.shortdata = shortdata;
	memcpy(targetstring, convert.sdata, sizeof(short));
	return 0;
}

// 4바이트 정수 데이터를 char형식으로 쪼개기 위한 함수
int convertInt32ToString(unsigned int int32data, char* targetstring){
	union convert_int_string convert;
	convert.int32data = int32data;
	memcpy(targetstring, convert.sdata, sizeof(int));
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
void parsing_config_data(char* source, int filesize) {
	// 행 단위 index
	int row_index = 0;
	// 행 내부 index
	int varindex = 0;

	// 각 행마다 구문 분석, 어떤 index인지 확인
	// 개행문자 위치
	int end_index = 0;
	// 필요한 데이터 시작 위치
	int start_data = 0;

	// 설정값의 종류 읽는 변수
	char tmp_indexValue[256] = "";
	// 실제 설정값 종류
	char tmp_dataValue[256] = "";

	// 범용성 변수
	int x, y;

	// 개행 문자 단위로 쪼개기
	//\r,\n 말고 세미콜론으로 개행 지정 
	for (x = 0; x < filesize; x++) {
		if (source[x] == ';') {
			// 개행문자를 ';'로 지정
			parsingData[row_index][varindex] = source[x];
			varindex = 0;
			row_index++;
		}
		else if ((source[x] == '\r') || (source[x] == '\n')) {
			// Nope
		}
		else {
			// 그 외 문자는 모두 저장
			parsingData[row_index][varindex] = source[x];
			varindex++;
		}
	}

#if DEBUG_LOG
	// 제대로 개행 하고 읽었나 확인
	printf("Config File Read\r\n");
	printf("----------------------------------------------------\r\n");
	for (x = 0; x < row_index; x++) {
		//row_index
		printf("[%3d]%s\r\n", x, parsingData[x]);
	}
	printf("----------------------------------------------------\r\n");
#endif

#if DEBUG_LOG
	// 
	printf("load config data\r\n");
	printf("----------------------------------------------------\r\n");
#endif
	for (x = 0; x < row_index; x++) {
		// 개행 단위로 parsing에 필요한 정보나 임시 변수 초기화
		end_index = 0;
		start_data = 0;
		memset(tmp_indexValue, 0x00, sizeof(tmp_indexValue));
		memset(tmp_dataValue, 0x00, sizeof(tmp_dataValue));
		//설정값 종류와 실제 설정값 분리를 위한 Parsing
		for (y = 0; y < 256; y++) {
			//type과 data를 분리하기 위한 문자 감지시
			if (parsingData[x][y] == ':') {
				end_index = y;
				start_data = y + 1;
				memcpy(tmp_indexValue, parsingData[x], sizeof(char) * y);
			}
			// 개행문자 감지시
			else if (parsingData[x][y] == ';') {
				// 실제 데이터를 복사
				memcpy(tmp_dataValue, parsingData[x] + (start_data), sizeof(char) * ((y - 1) - start_data + 1));
				// 설정값 타입이 서버 아이피 주소일시
				if (fCompareFunction(tmp_indexValue, "SERVER_ADDRESS", 14)) {
					// 문자열 그대로 가져온다
					memcpy(server_address, tmp_dataValue, sizeof(tmp_dataValue));
#if DEBUG_LOG
					printf("[server_address]%s\r\n", server_address);
#endif
				}
				// 설정값 타입이 서버 포트번호 일시
				if (fCompareFunction(tmp_indexValue, "SERVER_PORT", 11)) {
					// 문자열을 정수타입 변수로 저장
					server_port = fConvertStringToInt32(tmp_dataValue);
#if DEBUG_LOG
					printf("[server_port]%d\r\n", server_port);
#endif
				}
				// 설정값 타입이 서버 MQTT ID 일시
				if (fCompareFunction(tmp_indexValue, "SERVER_ID", 9)) {
					// 문자열 그대로 가져온다
					memcpy(server_id, tmp_dataValue, sizeof(tmp_dataValue));
#if DEBUG_LOG
					printf("[server_id]%s\r\n", server_id);
#endif
				}
				// 설정값 타입이 서버 MQTT PW 일시
				if (fCompareFunction(tmp_indexValue, "SERVER_PW", 9)) {
					// 문자열 그대로 가져온다
					memcpy(server_pw, tmp_dataValue, sizeof(tmp_dataValue));
#if DEBUG_LOG
					printf("[server_pw]%s\r\n", server_pw);
#endif
				}
			}
		}
	}
#if DEBUG_LOG
	printf("----------------------------------------------------\r\n");
#endif
}

//config file 처리
void read_config_file(void) {
	// config file 읽어올 변수
	char readbuffer[1024] = "";
	// 읽기 여부 성공여부 & 파일 사이즈.
	// -1이면 config file 읽기 실패로 간주
	int res = 0;
	// config file 읽어올 변수 초기화
	memset(readbuffer, 0x00, sizeof(readbuffer));
	//read file
	res = fFileLoadFunction("./config.txt", readbuffer);
	if (res > 0) {
		// 읽기 성공이면 설정값 읽어오기 시작
		parsing_config_data(readbuffer, res);
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

//save bitmap
void save_bitmap(void){
	int x = 0, y = 0, ny = 0;;
	int res = 0;

	// 비트맵 헤더
	struct BMP_HEADER_STRUCT bh;

	// 비트맵 매직넘버 : "BM"
	bh.iMagicNumber = 0x4D42;
	// 헤더 포함한 실제 파일 크기
	bh.iBitmapFileSize = (CAMERA_X_SIZE * CAMERA_Y_SIZE * CAMERA_C_SIZE) + 54;	//헤더 포함
	bh.bfReserved1 = 0;
	bh.bfReserved2 = 0;
	// 실제 데이터 오프셋 정보, 최소 헤더크기부터 시작
	bh.iButmapRealDataOffsetValue = 54; //?
	// 헤더중 윈도우 호환 헤더의 길이.
	// 헤더는 BMP 파일 헤더와 비트맵 정보헤더(DIB)헤더로 나뉘어 있는데
	// BMP파일 헤더는 14바이트로 고정 되어 있고 주로 사용하는 DIB 헤더는 윈도우 3.0 호환 헤더인 "윈도 V3"로 40바이트로 이루어져 있다.
	bh.iBackendHeaderSize = 40;
	// 비트맵 픽셀 크기
	bh.iBitmapXvalue = CAMERA_X_SIZE;
	bh.iBitmapYvalue = CAMERA_Y_SIZE;
	bh.iColorPlane = 1;
	// 비트맵 색상 크기. 주로 사용하는 255,255,255는 24바이트
	bh.iPixelBitDeepValue = 24;	//24비트
	bh.iCompressType = 0;
	// 헤더 제외한 비트맵 파일 크기
	bh.iImageSize = CAMERA_X_SIZE * CAMERA_Y_SIZE * CAMERA_C_SIZE; //921600
	bh.iPixcelPerMeterXvalue = 0;
	bh.iPixcelPerMeterYvalue = 0;
	bh.iColorPalet = 0;
	bh.iCriticalColorCount = 0;

	// 자주 쓰고 지우고 할 데이터라 램디스크 생성하여 임시로 저장
	FILE* fp = fopen("/home/pi/ramdisk/image.bmp", "w");
	if (fp == 0)
	{
		printf("File open fail\r\n");
	}

	// 비트맵 헤더를 저장할때 사용할 변수로 각 정보 당 최대 4바이트(32bit)를 넘지 않는다.
	char buf[4] = "";

	memset(buf, 0x00, sizeof(buf));
	convertShortToString(bh.iMagicNumber, buf);
	fseek(fp, 0L, SEEK_SET);
	fwrite(buf, sizeof(char), 2, fp);
	//printf("iMagicNumber\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iBitmapFileSize, buf);
	fseek(fp, 2L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iBitmapFileSize\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertShortToString(bh.bfReserved1, buf);
	fseek(fp, 6L, SEEK_SET);
	fwrite(buf, sizeof(char), 2, fp);
	//printf("bfReserved1\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertShortToString(bh.bfReserved2, buf);
	fseek(fp, 8L, SEEK_SET);
	fwrite(buf, sizeof(char), 2, fp);
	//printf("bfReserved2\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iButmapRealDataOffsetValue, buf);
	fseek(fp, 10L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iButmapRealDataOffsetValue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iBackendHeaderSize, buf);
	fseek(fp, 14L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iBackendHeaderSize\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iBitmapXvalue, buf);
	fseek(fp, 18L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iBitmapXvalue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iBitmapYvalue, buf);
	fseek(fp, 22L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iBitmapYvalue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertShortToString(bh.iColorPlane, buf);
	fseek(fp, 26L, SEEK_SET);
	fwrite(buf, sizeof(char), 2, fp);
	//printf("iColorPlane\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertShortToString(bh.iPixelBitDeepValue, buf);
	fseek(fp, 28L, SEEK_SET);
	fwrite(buf, sizeof(char), 2, fp);
	//printf("iPixelBitDeepValue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iCompressType, buf);
	fseek(fp, 30L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iCompressType\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iImageSize, buf);
	fseek(fp, 34L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iImageSize\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iPixcelPerMeterXvalue, buf);
	fseek(fp, 38L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iPixcelPerMeterXvalue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iPixcelPerMeterYvalue, buf);
	fseek(fp, 42L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iPixcelPerMeterYvalue\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iColorPalet, buf);
	fseek(fp, 46L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iColorPalet\r\n");

	memset(buf, 0x00, sizeof(buf));
	convertInt32ToString(bh.iCriticalColorCount, buf);
	fseek(fp, 50L, SEEK_SET);
	fwrite(buf, sizeof(char), 4, fp);
	//printf("iCriticalColorCount\r\n");

	// 카메라로 부터 받은 데이터 임시 변수에 기록
	ny = CAMERA_Y_SIZE;
	for (y = 0; y < CAMERA_Y_SIZE; y++){
		// 비트맵은 height부분이 반전 되서 기록 되기 때문에 거꾸로 읽어서 저장해야 한다.
		ny = ny - 1;
		for (x = 0; x < CAMERA_X_SIZE * 3; x++){
			CameraDataBuffer[(y * CAMERA_X_SIZE * 3) + x] = *(sCameraData + ((ny * CAMERA_X_SIZE * 3) + x));
		}
	}

	// 실제 카메라 데이터 기록
	res = fwrite(CameraDataBuffer, sizeof(char), bh.iImageSize, fp);	
	printf("res=%d\r\n", res);

	// 파일 오브젝트 정리
	fclose(fp);
}


//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#define MAXLINE 256

// 리눅스 시스템 쉘 사용하기 위한 함수
int popen_function(char *command){
	FILE* fp;
	int state;

	char buff[MAXLINE];
	fp = popen(command, "r");
	if (fp == NULL){
		perror("erro : ");
		exit(0);
	}

	while (fgets(buff, MAXLINE, fp) != NULL){
		printf("%s", buff);
	}

	state = pclose(fp);
	printf("state is %d\n", state);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
// 시간대만 반환 받을 것
int getnowtime(void){
	int ltime;
	time_t clock;
	struct tm* date;

	clock = time(0);
	date = localtime(&clock);
	ltime = date->tm_hour;// *10000;
	//ltime += date->tm_min * 100;
	//ltime += date->tm_sec;
	
	return(ltime);
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
