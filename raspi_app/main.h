
/* 
 *  Include
 */
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <linux/fb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <ncurses.h>
#include <pthread.h>
#include <mosquitto.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>

#include <pthread.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <stdint.h>
#include <unistd.h>
#include <termios.h> 
#include <fcntl.h>    
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <linux/fb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
/*
 *  Define
 */




/*
 *  Function
 */
// Appliction Entry Function
int fAppFunctionEntryPoint(int argc, char** argv);


//pThread
int fPthreadInitFunction(void);

void* t_function_CameraShotThread(void* data);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
//����Լ�
/*
 *  @brief  File Load Function
 *  @param  sFileName   File �ּ�
 *          sReadBuffer File Data ���� �� �迭 ���� ���� �ּ�
 *  @retval �б� ���� �� File ũ�� ��ȯ, ���н� -1 return
 */
int fFileLoadFunction(unsigned char* sFileName, unsigned char* sReadBuffer);
/*
 *  @brief  ���ڿ� ������ ���ڸ� int32������ ��ȯ
 *  @param  source  ���ڿ� ���� ���������� �ٲ� ���ڿ� ���� �ּ�
 *  @retval int32�� ��ȯ �� ��
 */
int fConvertStringToInt32(char* source);
/*
 *  @brief  ���ڿ� ��
 *  @param  source  ���� ����
 *			target	���� Ÿ��
 *			iSize	���� ����
 *  @retval ������ 1, �ٸ��� 0
 */
int fCompareFunction(char* source, char* target, int iSize);

//
int convertInt32ToString(unsigned int int32data, char* targetstring);
int convertShortToString(unsigned short shortdata, char* targetstring);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
//MQTT
int send_pub(const char* topic, int data_len, const void* payload);
int sub(void);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);
void on_connect(struct mosquitto* mosq, void* obj, int rc);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
void read_config_file(void);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
int popen_function(char* command);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------






