
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
//기능함수
/*
 *  @brief  File Load Function
 *  @param  sFileName   File 주소
 *          sReadBuffer File Data 적재 할 배열 변수 시작 주소
 *  @retval 읽기 성공 시 File 크기 반환, 실패시 -1 return
 */
int fFileLoadFunction(unsigned char* sFileName, unsigned char* sReadBuffer);
/*
 *  @brief  문자열 형태의 숫자를 int32값으로 변환
 *  @param  source  문자열 에서 정수형으로 바꿀 문자열 시작 주소
 *  @retval int32로 변환 된 값
 */
int fConvertStringToInt32(char* source);
/*
 *  @brief  문자열 비교
 *  @param  source  비교할 문자
 *			target	비교할 타겟
 *			iSize	비교할 길이
 *  @retval 같으면 1, 다르면 0
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






