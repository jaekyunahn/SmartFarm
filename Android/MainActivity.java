package com.example.smartfarm;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.support.v7.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Base64;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;

import android.content.Intent;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;

import java.text.BreakIterator;

public class MainActivity extends AppCompatActivity {

    public String MQTTHOST = "";
    public String pubTopic = "test";
    EditText subTopic;

    private MqttAndroidClient mqttAndroidClient;
    MqttAndroidClient client;

    //각종 설정값
    static String hostIP = "<IPv4 Address>:<Port Number>";
    static String username = "MQTT ID";
    static String pw = "MQTT PW";

    //topic
    static String server_call = "server_call";
    static String response_text = "response_text";
    static String response_image = "response_image";

    // 제어장치 상태
    static boolean led_status = false;
    static boolean pan_status = false;
    static boolean window_status = false;
    static boolean userControl_status = false;

    // 택스트 뷰어
    TextView getText;
    TextView sLog;
    TextView gettmp;
    TextView gethum;
    TextView getwater;
    TextView getlight;

    // 버튼
    Button connect_server;
    //데이터 요청 및 제어
    Button getData;
    Button pan_button;
    Button window_button;
    Button led_button;
    Button camera_shot;
    Button user_control;

    ImageView imageView;

    Bitmap mainbitmap;

    // 들어온 문자열 데이터
    String incomingTextData = "";

    // 들어온 문자열 최대 길이
    int incomingTextlength = 0;
    // 들어온 문자열의 마지막 endchar위치, 여기서는 '#'
    int endcharIndex = 0;

    // 들어온 문자열이 정보인지 알림인지 판단
    String cmdchar = "";

    //tmp
    String incoming_tmp = "";
    //hum
    String incoming_hum = "";
    //water
    String incoming_water = "";
    //light
    String incoming_light = "";
    //led
    String incoming_led = "";
    //pan
    String incoming_pan = "";
    //window
    String incoming_window = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 텍스트 박스
        // input data 확인용도
        getText = findViewById(R.id.getText);
        // Log 확인 용도
        sLog = findViewById(R.id.sLog);

        // getData시 들어오는 데이터 파싱하여 각각 데이터에 맞게 저장
        // 온도
        gettmp = findViewById(R.id.gettmp);
        // 습도
        gethum = findViewById(R.id.gethum);
        // 수분
        getwater = findViewById(R.id.getwater);
        // 광량
        getlight = findViewById(R.id.getlight);

        // 이미지뷰
        imageView = findViewById(R.id.imageView);

        // MQTT Clinent ID 생성
        String clientId = MqttClient.generateClientId();
        // 안드로이드 MQTT 클라이언트 오브젝트 초기화
        client = new MqttAndroidClient(this.getApplicationContext(), hostIP, clientId);

        //Broker에 접속 하기 위한 MQTT 옵션 오브젝트 초기화
        MqttConnectOptions options = new MqttConnectOptions();

        // 접속하기 위한 옵션 세팅
        // 유저 ID
        options.setUserName(username);
        // 유저 PW
        options.setPassword(pw.toCharArray());

        //----------------------------------------------------------------------------------------

        //  Sub connect
        connect_server = (Button)findViewById(R.id.connect_server);
        connect_server.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // 관심 topic 등록
                setSubscription();
                // 버튼 비활성화
                connect_server.setEnabled(false);
                // 최초 Farm 현재상태 읽기
                publisherFunction(server_call,"GET_DATA");
            }
        });

        //  get data
        getData = (Button)findViewById(R.id.getData);
        getData.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Farm 상태 읽기
                publisherFunction(server_call,"GET_DATA");
            }
        });

        //  Sub connect
        led_button = (Button)findViewById(R.id.led_button);
        led_button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // LED 제어
                if(led_status==false) {
                    led_status=true;
                    led_button.setText("LED On");
                    publisherFunction(server_call,"LED_ON");
                }
                else {
                    led_status=false;
                    led_button.setText("LED OFF");
                    publisherFunction(server_call,"LED_OFF");
                }
            }
        });

        //  Sub connect
        pan_button = (Button)findViewById(R.id.pan_button);
        pan_button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // PAN 제어
                if(pan_status==false) {
                    pan_status=true;
                    pan_button.setText("PAN On");
                    publisherFunction(server_call,"PAN_ON");
                }
                else {
                    pan_status=false;
                    pan_button.setText("PAN Off");
                    publisherFunction(server_call,"PAN_OFF");
                }
            }
        });

        //  Sub connect
        window_button = (Button)findViewById(R.id.window_button);
        window_button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // window 제어
                if(window_status==false) {
                    window_status=true;
                    publisherFunction(server_call,"WINDOW_ON");
                    window_button.setText("Window Open");
                }
                else {
                    window_status=false;
                    publisherFunction(server_call,"WINDOW_OFF");
                    window_button.setText("Window Close");
                }
            }
        });

        //camera_shot
        camera_shot = (Button)findViewById(R.id.camera_shot);
        camera_shot.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // 카메라 촬영
                publisherFunction(server_call,"CAMERA");
            }
        });

        //user_control
        user_control = (Button)findViewById(R.id.user_control);
        user_control.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // window 제어
                if(userControl_status==false) {
                    userControl_status=true;
                    publisherFunction(server_call,"USER_CON_ON");
                    user_control.setText("user control On");
                    led_button.setEnabled(true);
                    pan_button.setEnabled(true);
                    window_button.setEnabled(true);
                    publisherFunction(server_call,"GET_DATA");
                }
                else {
                    userControl_status=false;
                    publisherFunction(server_call,"USER_CON_OFF");
                    user_control.setText("user control Off");
                    led_button.setEnabled(false);
                    pan_button.setEnabled(false);
                    window_button.setEnabled(false);
                }
            }
        });

        //----------------------------------------------------------------------------------------

        try {
            IMqttToken token = client.connect(options);
            token.setActionCallback(new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    // We are connected
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    // Something went wrong e.g. connection timeout or firewall problems
                }
            });
        } catch (MqttException e) {
            e.printStackTrace();
        }

        // 수신 Callback
        client.setCallback(new MqttCallback() { //setCallback for message arrive
            @Override
            public void connectionLost(Throwable cause) {
                sLog.setText("connectionLost");
            }


            // topic 별 분류
            @Override
            public void messageArrived(String topic, MqttMessage message) throws Exception {
                if (topic.equals(response_text)) {
                    sLog.setText("Incoming data, response_text");

                    //
                    incomingTextData = new String(message.getPayload());
                    incomingTextlength = incomingTextData.length();

                    // 문자 수신 완료 표기를 위해 마지막 글자를 '#'으로 지정하고 위치 파악
                    for (int x = 0 ; x < incomingTextlength ; x++) {
                        if(incomingTextData.toCharArray()[x] == '#') {
                            // 최초 발견 되는 #위치 저장하고 loop 종료
                            endcharIndex = x;
                            break;
                        }
                    }
                    //sLog.setText("Incoming data size:" + endcharIndex + 1);

                    // 문자 데이터를 수신 했을 때 getData인 경우 파싱 하기 위해 맨 앞 1글자 D 여부를 확인
                    cmdchar = incomingTextData.substring(0,1);

                    //sLog.setText("cmdchar:" + cmdchar);

                    //앞부분 글자와 데이터 종료 문자인 # 제거
                    incomingTextData = incomingTextData.substring(1,endcharIndex);

                    // 들어온 문자 데이터 1번째 단어가 D 일때. 즉 getData 응답 데이터 일때 동작
                    if(cmdchar.toCharArray()[0]=='D') {
                        // 문자열 분리
                        String parsingStr[] = incomingTextData.split(",");

                        // tmp
                        incoming_tmp = parsingStr[0];
                        gettmp.setText("현재온도:" + incoming_tmp + "'c");

                        // hum
                        incoming_hum = parsingStr[1];
                        gethum.setText("현재습도:" + incoming_hum + "%");

                        // water
                        incoming_water = parsingStr[2];
                        getwater.setText("현재수분:" + incoming_water + "%");

                        // light
                        incoming_light = parsingStr[3];
                        getlight.setText("현재광량:" + incoming_light + "lux");

                        // led
                        incoming_led = parsingStr[4];
                        if (incoming_led.toCharArray()[0]=='0') {
                            led_status = false;
                            led_button.setText("LED Off");
                        } else if (incoming_led.toCharArray()[0]=='1') {
                            led_status = true;
                            led_button.setText("LED On");
                        }
                        // pan
                        incoming_pan = parsingStr[5];
                        if (incoming_pan.toCharArray()[0]=='0') {
                            pan_status = false;
                            pan_button.setText("PAN Off");
                        } else if (incoming_pan.toCharArray()[0]=='1') {
                            pan_status = true;
                            pan_button.setText("PAN On");
                        }

                        // window
                        incoming_window = parsingStr[6];
                        if (incoming_window.toCharArray()[0]=='0') {
                            window_status = false;
                            window_button.setText("Window close");
                        } else if (incoming_window.toCharArray()[0]!='0') {
                            window_status = true;
                            window_button.setText("Window Open");
                        }
                    }

                    getText.setText(incomingTextData);
                }
                // 들어온 토픽이 이미지면 동작
                if (topic.equals(response_image)) {
                    sLog.setText("Incoming data, response_image");
                    // 배열 형태를 비트맵 형태로 변환
                    mainbitmap = ByteToBitmap( message.getPayload() );
                    // 사이즈 통일 -> 현재는 320*240으로 들어 오지만 속도 개선이 있을 시 대비
                    mainbitmap = mainbitmap.createScaledBitmap(mainbitmap,320,240,true);
                    // 이미지 뷰어에 등록
                    imageView.setImageBitmap(mainbitmap);
                }
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {
                //not use
            }
        });
    }

    //퍼블리셔 함수
    public void publisherFunction(String topic, String Data) {
        try {
            // 지정한 토픽에 데이터 전달
            client.publish(topic, Data.getBytes(), 0 ,false);
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    // 메세지를 수신 할 수 있는 함수
    public void setSubscription() {
        try {
            // 관심 topic 등록
            // 단말기 -> 제어컴퓨터
            client.subscribe(server_call, 0);
            // 제어컴퓨터 -> 단말기. 문자 데이터
            client.subscribe(response_text, 0);
            // 제어컴퓨터 -> 단말기. 이미지 데이터
            client.subscribe(response_image, 0);
        }
        catch(MqttException e) {
            e.printStackTrace();
        }
    }

    /*
     * Byte배열형을 BitMap으로 변환시켜주는 함수
     */
    public static Bitmap ByteToBitmap(byte[] encodedByte) {
        try {
            //바이트 배열을 비트맵 형태로 변환
            Bitmap bitmap = BitmapFactory.decodeByteArray(encodedByte, 0, encodedByte.length);
            return bitmap;
        } catch (Exception e) {
            e.getMessage();
            return null;
        }
    }
}