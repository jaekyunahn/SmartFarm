# SmartFarm

Description
===========

  Arduino + RaspberryPi로 만든 Smart Farm

  Raspberry-Pi : MQTT Broker 역할, C/C++ 기반 언어로 자체 제작한 Server 개념의 프로그램으로 외부 인터넷망을 통하여 사용자와 MQTT로 통신하며 Data를 주고 받는 구조.

  Arduino :  Raspberry-Pi와 Serial Port로 연결 되어 57600 Baudrate로 통신, Sensor Data 전송이나 LED,Pan,Window의 현재 상태 및 동작제어를 위한 명령어도 받도록 되어 있는 구조. 

  Android Application : 외부 인터넷망에서 MQTT Protocol로 동작하기 위해 실험적으로 제작하였다.

  이 프로젝트는 개인역량 증진 목표로 시작하게 된 프로젝트로 부족함이 많을 수 있습니다. 추후 부족분은 지속적으로 업데이트를 진행할 예정이며 Android는 처음 시작 한 부분이고 필요한 소스코드나 Layout관련 부분만 올린점 양해 바랍니다.

Environment
===========

  이 프로젝트에 사용한 제품들은 다음과 같습니다.
  
  Smart Farm kit
  > https://www.devicemart.co.kr/goods/view?no=13162940&NaPm=ct%3Dl2gbpsjd%7Cci%3Dcheckout%7Ctr%3Dppc%7Ctrx%3D%7Chk%3D77dc7e8e265790b641ff15580fcb16720f85351b
  
  Android 단말기
  > 아마존 파이어 태블릿 2021 Fire HD 10 32GB

  Raspberry-Pi
  > Raspberry-Pi 4B 8GB / RaspberryPi OS 32Bit
  
  개발 환경은 다음과 같습니다.
  > Arduino IDE
  > Visual Studio 2019 (Raspberry-Pi Code Edit 용도로 사용)
  > Android Studio 2021
  
Files  
=====
  
  Android/MainActivity.java
  Android Application의 Main Code로 주로 MQTT관련 접속, 데이터 송수신, 배열 형태로 수신 받은 image를 bitamp형식으로 변경하여 Ui에 Display할 수 있도록 하는 Source Code 입니다.
  
  Arduino/main/main.ino
  Smart Farm의 sensor Data나 기능 동작을 제어하는 Arduino의 Source Code 입니다. Raspberry Pi와 Serial로 통신 하도록 되어 있으며 수신은 interrupt로 처리하도록 되어 있습니다.
  
  raspi_app/function.c
  Smart Farm의 핵심 부분입니다. Android와 MQTT로 통신하면서 현재 정보나 제어 상태 혹은 제어 명령을 받아 내거나 Camera 촬영이나 Image 전송을 담당하며 Arduino와는 Serial로 통신하면서 Sensor Data를 받거나 기능 동작을 명령하기도 하며 사용자의 설정에 따라 스스로 제어할지 아니면 사용자의 제어를 적용 할 지 결정 하거나 처리하도록 되어 있습니다. 이와 같은 기능들을 동시처리 하기 위하여 Multi Thread (pThread lib)를 적용하였습니다.
