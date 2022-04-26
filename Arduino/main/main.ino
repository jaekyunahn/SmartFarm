// Board = Mega 2560

// 아두이노 타이머
#include <MsTimer2.h>
// 아두이노 소프트웨어 리셋
#include <SoftwareReset.hpp>
// 아두이노 DHT 라이브러리
#include <DHT.h>
// 아두이노 서보모터 라이브러리
#include <Servo.h>
// 아두이노 i2C 라이브러리
#include <Wire.h>
// i2C 인터페이스 LCD를 사용하기 위한 라이브러리
#include <LiquidCrystal_I2C.h>

// 각종 동작 핀 number
#define SERVOPIN 9
#define LIGHTPIN 4
#define FAN_PIN 32
#define DHTPIN 12

// Serial incomming
int serial_incoming_flag = 0;

// DHT 11 Global var
float temperature, humidity; 

// cdc value Global val
int cdcValue = 0;
float pv_value;
float Rp;
float y;
float Ea;

// 기능 동작 변수
int led_st = 0;
int pan_st = 0;
int window_st = 0;

// Host로 부터 내려오는 데이터 1바이트 단위로 저장하는 임시 변수
char ch;

// 수분 센서 변수
int waterValue = 0;

// Host로 부터 내려오는 데이터 최종 문자열 형태로 저장하는 임시 변수
char sData[64] = { 0x00, };

// Host로 부터 내려오는 데이터 최종 문자열 형태로 저장하는 임시 변수 index
int rx_data_index = 0;

//서보 모터 위치
int servo_value = 0;

// blink 할 led 동작 제어 변수
bool led_blink_state = true;

// 수신된 문자열 길이
int incomingCount = 0;

// a String to hold incoming data
volatile String inputString = "";      
// whether the string is complete  
volatile bool stringComplete = false;  

// DHT 11 object
DHT dht(DHTPIN, DHT11);

// 서보모터 객체 변수
Servo myServo;

// LCD interface
LiquidCrystal_I2C lcd(0x27, 16, 2);

// LCD print function
void printLCD(int col, int row ,const char *str) {
    for(int i=0 ; i < strlen(str) ; i++){
        lcd.setCursor(col+i , row);
        lcd.print(str[i]);
    }
}

void setup() {
    // 시리얼로 수신된 문자열 길이 변수 초기화
    incomingCount = 0;
    
    // reserve 200 bytes for the inputString:
    inputString.reserve(10);
  
    // DHT 11 init
    dht.begin();
    
    // Serial 설정
    Serial.begin(57600); 

    // LCD init
    lcd.init();
    lcd.backlight();

    //serbo IO 선언
    myServo.attach(SERVOPIN);
    myServo.write(0);

    // LED GPIO 선언
    pinMode(LIGHTPIN, OUTPUT);

    // Pan GPIO 선언
    pinMode(FAN_PIN, OUTPUT);

    /*
     *  init ok 
     */ 
    //LCD
    printLCD(0, 0, "--Smart Farm--");
    printLCD(0, 1, "Ready to Farm "); 
    //Serial
    Serial.println("START#");

    /*
     *  led blink
     */
    // Blink LED GPIO 선언
    pinMode(23, OUTPUT);
    
    // 500ms마다 ledBlink 함수 동작
    MsTimer2::set(500, ledBlink);
    // 타이머 동작 시작
    MsTimer2::start();

}

void loop() { // put your main code here, to run repeatedly:
    // CDC sensor
    cdcValue = analogRead(A0);
    // CDC센서를 유사 Lux값으로 변환
    pv_value = float(cdcValue*5)/1023;
    Rp = (10*pv_value)/(5-pv_value);
    y = (log10(200/Rp))/0.7; 
    Ea = pow(10,y); 

    // 수분센서
    waterValue = analogRead(A1);
    waterValue = 100 - waterValue/10;
    
    // DHT 11 value
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    if (stringComplete){   
        memset(sData, 0x00, 64);
        sprintf(sData, "%s",inputString.c_str());
        
        // Test
        if(memcmp(sData, "ping", 4) == 0){
            Serial.println("Clive_actuator#");
        }
        
        else if(memcmp(sData, "getData", 7) == 0){
            //온도
            Serial.print("D");
            Serial.print((int)temperature);
            Serial.print(",");
            //습도
            Serial.print((int)humidity);
            Serial.print(",");
            //수분
            Serial.print(waterValue);
            Serial.print(",");
            //광센서
            Serial.print(Ea);
            Serial.print(",");
            //led 점등여부
            Serial.print(led_st);
            Serial.print(",");
            //pan 동작여부
            Serial.print(pan_st);
            Serial.print(",");
            //창문열림 여부
            Serial.print(window_st);
            // end
            Serial.println("#");
        }
        
        // LED
        if(memcmp(sData, "CMD:LED=", 8) == 0){
            if(sData[8] == '0') {
                //off
                Serial.println("CLED:0#");
                led_st = 0;
            }
            else if(sData[8] == '1') {
                //on
                Serial.println("CLED:1#");
                led_st = 1;
            }
            digitalWrite(LIGHTPIN, led_st);
        }
        
        // PAN
        else if(memcmp(sData, "CMD:PAN=", 8) == 0){
            if(sData[8] == '0') {
                //off
                Serial.println("CPAN:0#");
                pan_st = 0;
            }
            else if(sData[8] == '1') {
                //on
                Serial.println("CPAN:1#");
                pan_st = 1;
            }
            digitalWrite(FAN_PIN, pan_st);
        }
        
        // Window
        else if(memcmp(sData, "CMD:WND=", 8) == 0){
            if(sData[8] == '0') {
                //Close
                Serial.println("CWID:0#");
                window_st = 0;
            }
            else if(sData[8] == '1') {
                //Open
                Serial.println("CWID:1#");
                window_st = 90;
            }
            myServo.write(window_st);
        }

        // 리셋
        else if(memcmp(sData, "RESET", 5) == 0){
            softwareReset::standard();
        }

        /*
         *  패킷 처리 끝났으니 초기화
         */ 
        // String Type 변수 초기화
        inputString = "";
        // 수신 완료 플래그 해제
        stringComplete = false;
        // 변수 내부 데이터 제거
        inputString.remove(0);
        // 수신 카운트 초기화
        incomingCount = 0;
    }

    // LED blink
    digitalWrite(23, led_blink_state);  
}

void serialEvent(){
    // 1바이트씩 읽기
    char inChar = (char)Serial.read();
    // 특정 문자 범위만 받아 들이는걸로
    //(A~z, 0~9 기타 기호문자 같이 사람이 읽을 수 있는 범위만 수신 변수에 저장)
    if((inChar >= 0x21) && (inChar <= 0x7E)){
        //1바이트씩 누적
        inputString += inChar;
        // 들어온 카운트 갱신
        incomingCount++;
        // '#'문자를 endchar로 지정하였으니 감지가 되면 수신 플래그 갱신
        if (inChar == '#'){
            //
            stringComplete = true;
        }
    }
}

// 타이머 동작시 호출될 함수
void ledBlink(){
    led_blink_state = !led_blink_state;
}
