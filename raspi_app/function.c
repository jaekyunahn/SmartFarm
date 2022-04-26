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

//�۽� �� ������
volatile char   Tx_buf[32];

// �۽� �� ���� ũ��
volatile int    iTxBufferCounter = 0;

//UART RX Buffer
volatile char   Rx_buf[1024];

// ���� ���� ũ��
volatile int    iRxBufferCounter = 0;

// �������Ϳ��� ��� �� �غ� �Ǿ����� ����
volatile int ready_arduino = 0;

// ����� �������� ����
volatile int user_control = 0;

//Serial Thread�� ���� ������ ���� ������ ����
volatile char rx_data_global[1024] = "";
volatile int incoming_data = 0;

// ���� �� �� ���๮�� ������ Parsing
char parsingData[32][256] = { {0x00,}, };

// MQTT ���ӿ� �ʿ��� ���� ���� ����
char server_address[256] = "";
int server_port = 0;
char server_id[256] = "";
char server_pw[256] = "";

// ��Ʈ�� ��� ����
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

// union type���� �迭 <-> ������ ������ ��ȯ
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

// Application �� ���� ��
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
		// Thread ���� �ϴ� ���� main Thread�� ���
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

	// ǥ�� �������̽� ����ü
	struct termios    uart_io;
	// üũ�� event ������ ���� struct
	struct pollfd     poll_events;

	char buf[64];
	char rx_data[1024];

	// ���� ���� ��Ŷ ũ��
	// ��Ŷ�� �߷��� ���� ��� ���� �ϱ� ���� ����
	int serial_rx_index = 0;
	// ���� ���� ���ڿ� ũ��
	int cnt;

	int x = 0;
	int y = 0;

	// �ø��� ��Ʈ open
loop:
	memset(sSerialPortName, 0x00, sizeof(sSerialPortName));

#if 1
	// ttyACMx
	sprintf(sSerialPortName, "/dev/ttyACM%d", iSerialPortIndex);
#else
	// ttyUSBx
	sprintf(sSerialPortName, "/dev/ttyUSB%d", iSerialPortIndex);
#endif

	// ����̽��� open �Ѵ�.
	serial_fd = open(sSerialPortName, O_RDWR | O_NOCTTY);
	if (0 > serial_fd) {
		printf("[Serial]open error:%s\n", sSerialPortName);
		// ��õ� Ư�� Ƚ�� �Ѿ�� ���� ����
		if (iSerialPortIndex > 65535) {
			return -1;
		}
		else {
			iSerialPortIndex++;
			goto loop;
		}
	}

	// �ø��� ��Ʈ ��� ȯ�� ����
	memset(&uart_io, 0, sizeof(uart_io));
	/* control flags */
	uart_io.c_cflag = B57600 | CS8 | CLOCAL | CREAD;   //115200 , 8bit, �� ���� ���¸� ����, ������ ���.
	/* output flags */
	uart_io.c_oflag = 0;
	/* input flags */
	uart_io.c_lflag = 0;
	/* control chars */
	uart_io.c_cc[VTIME] = 0;    //Ÿ�̸��� �ð��� ����
	uart_io.c_cc[VMIN] = 1;     //ead�� �� ���ϵǱ� ���� �ּ��� ���� ������ ����

	//���� ���� �Լ� ȣ��
	tcflush(serial_fd, TCIFLUSH);              // TCIFLUSH : �Է��� ����
	// IO �Ӽ� ����
	tcsetattr(serial_fd, TCSANOW, &uart_io);   // TCSANOW : �Ӽ��� �ٷ� �ݿ�
	// �ø��� ��Ʈ ����̽� ����̹������� ����
	fcntl(serial_fd, F_SETFL, FNDELAY);        // F_SETFL : ���� ���¿� ���� ��带 ����

	// poll ����� ���� �غ�   
	poll_events.fd = serial_fd;
	poll_events.events = POLLIN | POLLERR;          // ���ŵ� �ڷᰡ �ִ���, ������ �ִ���
	poll_events.revents = 0;

	usleep(1000);
	// 1���� ���� ��ȣ ������ ����ȭ
	serial_send("RESET#", 7);
	usleep(1000);

	memset(buf, 0x00, sizeof(buf));
	memset(rx_data, 0x00, sizeof(rx_data));

	while (1) {
		memset(buf, 0x00, sizeof(buf));
		cnt = read(serial_fd, buf, sizeof(buf));
		if (cnt > 1) {
			for (x = (cnt - 1); x >= 0; x--) {
				// ����� �� �� �ִ� ���ڿ��� �޴´�
				if ((buf[x] >= 0x21) && (buf[x] <= 0x7E)) {
					if ((x == 0) && (buf[x] != '\n')) {
						//���� ����
						printf("[Serial]buf[%d]:%s\r\n", cnt, buf);
						memcpy(rx_data + serial_rx_index, buf, sizeof(char) * cnt);
						serial_rx_index = serial_rx_index + cnt;
					}
					if (buf[x] == '#') {
						//���� ����, �ε��� �ʱ�ȭ
						memcpy(rx_data + serial_rx_index, buf, sizeof(char) * cnt);
						//Todo
						memset(rx_data_global, 0x00, sizeof(rx_data_global));
						printf("[Serial]rx_data[%d]:%s\r\n", serial_rx_index, rx_data);
						memcpy(rx_data_global, rx_data, sizeof(rx_data));
						// ���� �������� ���� �غ� �Ϸ�� ���ڸ� ������
						if (fCompareFunction(rx_data, "START", 5)) {
							printf("[Serial]Arduino online!!\r\n");
							// ��� �غ� �Ϸ� �÷��� ����
							ready_arduino = 1;
						}
						// ���� ���� ���� �ʱ�ȭ
						serial_rx_index = 0;
						memset(rx_data, 0x00, sizeof(rx_data));
						memset(buf, 0x00, sizeof(buf));
						incoming_data = 1;
					}
				}
			}
		}

		//write�� ������ �����
		if (iTxBufferCounter != 0) {
			// �ø��� ������ ����
			cnt = write(serial_fd, (const void*)Tx_buf, sizeof(char) * iTxBufferCounter);
			// ������ ������ ������ ���� ������ ũ�� ���� �ʱ�ȭ
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
		//ī�޶� �Կ� ��û�� ������
		if (sCameraShotCommand == 1) {
			// shot
			sCameraData = fCameraShotFunction();
			// ��û �÷��� ����
			sCameraShotCommand = 0;
		}
	}
	printf("%s:Exit\r\n", thread_name);
}

// �ڵ� ���� �Լ�
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

	// �ø��� ������ ���� ���� ���� ����
	char rx_data[1024] = "";

	// �ø��� ������ endchar ��ġ
	int iEndcharIndex = 0;

	int tmp = 0;
	int hum = 0;
	int water = 0;
	int light = 0;
	int led = 0;
	int pan = 0;
	int window = 0;

	// parsing Ƚ��
	int parsingcount = 0;
	// parsing ��� �޴� ����
	char* parsing_result;

	while (1){
		if( (ready_arduino == 1) && (user_control == 0)){
			// �������� �����Ͱ� �ִ��� Ȯ��
			while (1){
				if (iTxBufferCounter == 0) {
					break;
				}
			}
			// ���� ������ ��û
			res = serial_send("getData#", 9);
			//������ ��������� ����
			incoming_data = 0;
			while (1) {
				if (incoming_data == 1) {
					break;
				}
			}
			// �м�
			// Find endchar
			for (x = 0 ; x < sizeof(rx_data) ; x++){
				if ((rx_data_global[0] == 'D') && (rx_data_global[x] == '#')){
					memset(rx_data, 0x00, sizeof(rx_data));
					memcpy(rx_data, rx_data_global + 1, sizeof(char) * (x - 1));
					printf("[auto]rx_data=%s\r\n", rx_data);
				}
			}

			// ������ �Ľ�
			parsingcount = 0;
			parsing_result = strtok(rx_data,",");
			// �ڸ� ���ڿ��� ������ ���� ������ �ݺ�
			while (parsing_result != NULL){
				// �ڸ� ���ڿ� ���
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

				// ���� ���ڿ��� �߶� �����͸� ��ȯ
				parsing_result = strtok(NULL, ",");    
				parsingcount++;
			}

			// ����
#if DEBUG_LOG
			printf("tmp   =%d\r\n", tmp);
			printf("hum   =%d\r\n", hum);
			printf("water =%d\r\n", water);
			printf("light =%d\r\n", light);
			printf("led   =%d\r\n", led);
			printf("pan   =%d\r\n", pan);
			printf("window=%d\r\n", window);
#endif
			// tmp �� ���� -> pan�� window ����
			if (tmp > 25){
				if (pan==0){
					//pan����
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
					//pan����
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

			// ���� 7��~ ���� 8�� ���� �ڵ� On
			printf("getnowtime()=%d\r\n", getnowtime());
			if ((getnowtime() < 8) || (getnowtime() >= 16)){
				if (led == 0){
					//led����
					serial_send("CMD:LED=1#", 10);
#if DEBUG_LOG
					printf("LED On\r\n");
#endif
				}
			}
			else{
				if (led == 1){
					//led����
					serial_send("CMD:LED=0#", 10);
#if DEBUG_LOG
					printf("LED Off\r\n");
#endif
				}
			}
		}

		//10�� ���� �а� �ڵ� ���� �� �� �ֵ���
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
	char p1[] = "thread_Camera_Shot";	// 0�� ������ �̸� : Camera Shot ���
	char p2[] = "thread_serial";		// 1�� ������ �̸� : serial ���
	char p3[] = "thread_mqtt_sub";		// 2�� ������ �̸� : mqtt_sub ���
	char p4[] = "thread_auto_control";		// 3�� ������ �̸� : Auto control ���

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
		// ������� ������ ���� Ȯ��
		//iTxBufferCounter
		while (1)
		{
			if (iTxBufferCounter == 0) {
				break;
			}
		}

		// ���� �� ���� ��û�� ������
		if (fCompareFunction((char*)msg->payload, "GET_DATA", 8)) {
			res = serial_send("getData#", 9);
		}

		// LED On/Off ��û
		else if (fCompareFunction((char*)msg->payload, "LED_ON", 6)) {
			res = serial_send("CMD:LED=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "LED_OFF", 7)) {
			res = serial_send("CMD:LED=0#", 10);
		}

		// Pan On/Off ��û
		else if (fCompareFunction((char*)msg->payload, "PAN_ON", 6)) {
			res = serial_send("CMD:PAN=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "PAN_OFF", 7)) {
			res = serial_send("CMD:PAN=0#", 10);
		}

		// Widnow Open/close ��û
		else if (fCompareFunction((char*)msg->payload, "WINDOW_ON", 9)) {
			res = serial_send("CMD:WND=1#", 10);
		}
		else if (fCompareFunction((char*)msg->payload, "WINDOW_OFF", 10)) {
			res = serial_send("CMD:WND=0#", 10);
		}

		// ����� �ڵ����� ����
		else if (fCompareFunction((char*)msg->payload, "USER_CON_ON", 11)) {
			user_control = 1;
			return;
		}
		else if (fCompareFunction((char*)msg->payload, "USER_CON_OFF", 12)) {
			user_control = 0;
			return;
		}

		else if (fCompareFunction((char*)msg->payload, "CAMERA", 6)) {
			//Camera Shot ��û
			sCameraShotCommand = 1;
			while (1){
				// ���� ������ ȹ�� �Ϸ� �� �� ���� ���
				if (sCameraShotCommand == 0){
					break;
				}
			}

			// 1�� ���
			usleep(1000);

			//Save File
			save_bitmap();

			//convert jpg
			// ������ �ִ� jpg ����
			popen_function("rm /home/pi/ramdisk/image.jpg");
			// jpg ��ȯ ���̺귯�� ���
			popen_function("convert /home/pi/ramdisk/image.bmp /home/pi/ramdisk/image.jpg");

			// 1�� ���
			usleep(1000);

			// read file
			memset(sendCameraData, 0x00, sizeof(sendCameraData));
			image_res = fFileLoadFunction("/home/pi/ramdisk/image.jpg", sendCameraData);
			// ���� ����
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

	// mosquitto ���̺귯�� �ʱ�ȭ
	mosquitto_lib_init();

	struct mosquitto* mosq;

	// sub ��ü ����
	mosq = mosquitto_new("subscribe-test", true, &id);
	// broker(����)�� �����ϱ� ���� callback�Լ� ���
	mosquitto_connect_callback_set(mosq, on_connect);
	// pub�� ���� ���ŵ����͸� �ޱ� ���� callback�Լ� ���
	mosquitto_message_callback_set(mosq, on_message);
	// broker�� ����� ���� ���� 
	mosquitto_username_pw_set(mosq, server_id, server_pw);

	// broker�� ����
	rc = mosquitto_connect(mosq, server_address, server_port, 10);
	if (rc) {
		printf("[MQTT_Sub]Could not connect to Broker with return code %d\n", rc);
		return -1;
	}
	
	// pub�� ���� �����͸� �����ϱ� ���� ���. ���ŵ����Ͱ� ���� �� ������ callback �Լ��� �����ϰ� �Ǿ� �ִ�
	mosquitto_loop_start(mosq);

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

int send_pub(const char* topic, int data_len, const void* payload) {
	int rc;
	// MQTT lib init
	mosquitto_lib_init();
	// ����ü �Ҵ�
	//pubmosq = mosquitto_new("publisher-test", true, NULL);
	pubmosq = mosquitto_new(server_id, true, NULL);
	// MQTT ���Ŀ (����)�� ��ϵ� ID�� PW�� ����ü ����
	mosquitto_username_pw_set(pubmosq, server_id, server_pw);
	// MQTT ���Ŀ ���� �õ�
	rc = mosquitto_connect(pubmosq, server_address, server_port, 10);
	// ������ �ȵǸ�
	if (rc != 0) {
		printf("[MQTT_Pub]Client could not connect to broker! Error Code: %d\n", rc);
		mosquitto_destroy(pubmosq);
		rc = -1;
	}
	// ������ �Ǹ�
	else {
		// ������ ����, topic�� �°� ���� �ؾ� �����ڵ� ���� ������ �з��� ���� �� ��
		rc = mosquitto_publish(pubmosq, NULL, topic, data_len, payload, 0, false);
		//���� ����
		mosquitto_disconnect(pubmosq);
		// �޸𸮷� ���� ����ü �ν��Ͻ� ����
		mosquitto_destroy(pubmosq);
		// ����� ���̺귯�� �޸𸮿��� ����
		mosquitto_lib_cleanup();
	}

	return rc;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

//���ڿ� ��
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
 *  @param  sFileName   File �ּ�
 *          sReadBuffer File Data ���� �� �迭 ���� ���� �ּ�
 *  @retval �б� ���� �� File ũ�� ��ȯ, ���н� -1 return
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
 *  @brief  ���ڿ� ������ ���ڸ� int32������ ��ȯ
 *  @param  source  ���ڿ� ���� ���������� �ٲ� ���ڿ� ���� �ּ�
 *  @retval int32�� ��ȯ �� ��
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

// 2����Ʈ ���� �����͸� char�������� �ɰ��� ���� �Լ�
int convertShortToString(unsigned short shortdata, char* targetstring){
	union convert_short_string convert;
	convert.shortdata = shortdata;
	memcpy(targetstring, convert.sdata, sizeof(short));
	return 0;
}

// 4����Ʈ ���� �����͸� char�������� �ɰ��� ���� �Լ�
int convertInt32ToString(unsigned int int32data, char* targetstring){
	union convert_int_string convert;
	convert.int32data = int32data;
	memcpy(targetstring, convert.sdata, sizeof(int));
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
void parsing_config_data(char* source, int filesize) {
	// �� ���� index
	int row_index = 0;
	// �� ���� index
	int varindex = 0;

	// �� �ึ�� ���� �м�, � index���� Ȯ��
	// ���๮�� ��ġ
	int end_index = 0;
	// �ʿ��� ������ ���� ��ġ
	int start_data = 0;

	// �������� ���� �д� ����
	char tmp_indexValue[256] = "";
	// ���� ������ ����
	char tmp_dataValue[256] = "";

	// ���뼺 ����
	int x, y;

	// ���� ���� ������ �ɰ���
	//\r,\n ���� �����ݷ����� ���� ���� 
	for (x = 0; x < filesize; x++) {
		if (source[x] == ';') {
			// ���๮�ڸ� ';'�� ����
			parsingData[row_index][varindex] = source[x];
			varindex = 0;
			row_index++;
		}
		else if ((source[x] == '\r') || (source[x] == '\n')) {
			// Nope
		}
		else {
			// �� �� ���ڴ� ��� ����
			parsingData[row_index][varindex] = source[x];
			varindex++;
		}
	}

#if DEBUG_LOG
	// ����� ���� �ϰ� �о��� Ȯ��
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
		// ���� ������ parsing�� �ʿ��� ������ �ӽ� ���� �ʱ�ȭ
		end_index = 0;
		start_data = 0;
		memset(tmp_indexValue, 0x00, sizeof(tmp_indexValue));
		memset(tmp_dataValue, 0x00, sizeof(tmp_dataValue));
		//������ ������ ���� ������ �и��� ���� Parsing
		for (y = 0; y < 256; y++) {
			//type�� data�� �и��ϱ� ���� ���� ������
			if (parsingData[x][y] == ':') {
				end_index = y;
				start_data = y + 1;
				memcpy(tmp_indexValue, parsingData[x], sizeof(char) * y);
			}
			// ���๮�� ������
			else if (parsingData[x][y] == ';') {
				// ���� �����͸� ����
				memcpy(tmp_dataValue, parsingData[x] + (start_data), sizeof(char) * ((y - 1) - start_data + 1));
				// ������ Ÿ���� ���� ������ �ּ��Ͻ�
				if (fCompareFunction(tmp_indexValue, "SERVER_ADDRESS", 14)) {
					// ���ڿ� �״�� �����´�
					memcpy(server_address, tmp_dataValue, sizeof(tmp_dataValue));
#if DEBUG_LOG
					printf("[server_address]%s\r\n", server_address);
#endif
				}
				// ������ Ÿ���� ���� ��Ʈ��ȣ �Ͻ�
				if (fCompareFunction(tmp_indexValue, "SERVER_PORT", 11)) {
					// ���ڿ��� ����Ÿ�� ������ ����
					server_port = fConvertStringToInt32(tmp_dataValue);
#if DEBUG_LOG
					printf("[server_port]%d\r\n", server_port);
#endif
				}
				// ������ Ÿ���� ���� MQTT ID �Ͻ�
				if (fCompareFunction(tmp_indexValue, "SERVER_ID", 9)) {
					// ���ڿ� �״�� �����´�
					memcpy(server_id, tmp_dataValue, sizeof(tmp_dataValue));
#if DEBUG_LOG
					printf("[server_id]%s\r\n", server_id);
#endif
				}
				// ������ Ÿ���� ���� MQTT PW �Ͻ�
				if (fCompareFunction(tmp_indexValue, "SERVER_PW", 9)) {
					// ���ڿ� �״�� �����´�
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

//config file ó��
void read_config_file(void) {
	// config file �о�� ����
	char readbuffer[1024] = "";
	// �б� ���� �������� & ���� ������.
	// -1�̸� config file �б� ���з� ����
	int res = 0;
	// config file �о�� ���� �ʱ�ȭ
	memset(readbuffer, 0x00, sizeof(readbuffer));
	//read file
	res = fFileLoadFunction("./config.txt", readbuffer);
	if (res > 0) {
		// �б� �����̸� ������ �о���� ����
		parsing_config_data(readbuffer, res);
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

//save bitmap
void save_bitmap(void){
	int x = 0, y = 0, ny = 0;;
	int res = 0;

	// ��Ʈ�� ���
	struct BMP_HEADER_STRUCT bh;

	// ��Ʈ�� �����ѹ� : "BM"
	bh.iMagicNumber = 0x4D42;
	// ��� ������ ���� ���� ũ��
	bh.iBitmapFileSize = (CAMERA_X_SIZE * CAMERA_Y_SIZE * CAMERA_C_SIZE) + 54;	//��� ����
	bh.bfReserved1 = 0;
	bh.bfReserved2 = 0;
	// ���� ������ ������ ����, �ּ� ���ũ����� ����
	bh.iButmapRealDataOffsetValue = 54; //?
	// ����� ������ ȣȯ ����� ����.
	// ����� BMP ���� ����� ��Ʈ�� �������(DIB)����� ������ �ִµ�
	// BMP���� ����� 14����Ʈ�� ���� �Ǿ� �ְ� �ַ� ����ϴ� DIB ����� ������ 3.0 ȣȯ ����� "���� V3"�� 40����Ʈ�� �̷���� �ִ�.
	bh.iBackendHeaderSize = 40;
	// ��Ʈ�� �ȼ� ũ��
	bh.iBitmapXvalue = CAMERA_X_SIZE;
	bh.iBitmapYvalue = CAMERA_Y_SIZE;
	bh.iColorPlane = 1;
	// ��Ʈ�� ���� ũ��. �ַ� ����ϴ� 255,255,255�� 24����Ʈ
	bh.iPixelBitDeepValue = 24;	//24��Ʈ
	bh.iCompressType = 0;
	// ��� ������ ��Ʈ�� ���� ũ��
	bh.iImageSize = CAMERA_X_SIZE * CAMERA_Y_SIZE * CAMERA_C_SIZE; //921600
	bh.iPixcelPerMeterXvalue = 0;
	bh.iPixcelPerMeterYvalue = 0;
	bh.iColorPalet = 0;
	bh.iCriticalColorCount = 0;

	// ���� ���� ����� �� �����Ͷ� ����ũ �����Ͽ� �ӽ÷� ����
	FILE* fp = fopen("/home/pi/ramdisk/image.bmp", "w");
	if (fp == 0)
	{
		printf("File open fail\r\n");
	}

	// ��Ʈ�� ����� �����Ҷ� ����� ������ �� ���� �� �ִ� 4����Ʈ(32bit)�� ���� �ʴ´�.
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

	// ī�޶�� ���� ���� ������ �ӽ� ������ ���
	ny = CAMERA_Y_SIZE;
	for (y = 0; y < CAMERA_Y_SIZE; y++){
		// ��Ʈ���� height�κ��� ���� �Ǽ� ��� �Ǳ� ������ �Ųٷ� �о �����ؾ� �Ѵ�.
		ny = ny - 1;
		for (x = 0; x < CAMERA_X_SIZE * 3; x++){
			CameraDataBuffer[(y * CAMERA_X_SIZE * 3) + x] = *(sCameraData + ((ny * CAMERA_X_SIZE * 3) + x));
		}
	}

	// ���� ī�޶� ������ ���
	res = fwrite(CameraDataBuffer, sizeof(char), bh.iImageSize, fp);	
	printf("res=%d\r\n", res);

	// ���� ������Ʈ ����
	fclose(fp);
}


//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#define MAXLINE 256

// ������ �ý��� �� ����ϱ� ���� �Լ�
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
// �ð��븸 ��ȯ ���� ��
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
