#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <Modbus.h>
#include <ModbusIP_ESP32.h>

#define Endereco_LCD 0x27 

#define sensorPin 34
#define RY1pin 5
#define SDAPin 4
#define SCLPin 15

#define timeOutCmd 5000
#define timeOutWifi 10000

// Estamos usando um LCD 20x4.
LiquidCrystal_I2C lcd(Endereco_LCD, 20, 4);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Configura keypad numérico
const byte LINHASnum = 4;
const byte COLUNASnum = 3;
char matriz_numerico[LINHASnum][COLUNASnum] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'s','0','d'}
};//s = sobe; d = desce;

//byte PinosLinhasNUM[LINHASnum] = {33,25,26,27};
//byte PinosColunasNUM[COLUNASnum] = {14,12,13};
byte PinosLinhasNUM[LINHASnum] = {13,12,14,27};
byte PinosColunasNUM[COLUNASnum] = {26,25,33};

Keypad tecladoNUM = Keypad( makeKeymap(matriz_numerico), PinosLinhasNUM, 
                            PinosColunasNUM, LINHASnum, COLUNASnum);
char keypadNum;
char buf[3];
String strAux="", strInjetora="09", strRef="1365";


unsigned int intCaixa=600, intAt=590;
unsigned long intTotal=590;
bool sensor =0, estadoAtual=0;


//A pesar de haver apenas um teclado físicamente, tive que criar um segundo teclado usando os mesmos pinos especial para os comandos. O programa não estava lendo as teclas após executar algum comando.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Configura keypad comandos
const byte LINHAScmd = 1;
const byte COLUNAScmd = 2;
char matriz_comando[LINHAScmd][COLUNAScmd] = {
  {'s','d'}
};//s = Sobe; d = Desce;

//byte PinosLinhasCMD[LINHAScmd] = {27};
//byte PinosColunasCMD[COLUNAScmd] = {14,13};
byte PinosLinhasCMD[LINHAScmd] = {27};
byte PinosColunasCMD[COLUNAScmd] = {26,33};

Keypad tecladoCMD = Keypad( makeKeymap(matriz_comando), PinosLinhasCMD, 
                            PinosColunasCMD, LINHAScmd, COLUNAScmd);

char posicaoCur=0;
unsigned long lastCmdTime = 0;
char comando;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Configurações ModbusIP (Wifi)
//Nome e senha da rede
char ssid[] = "Edison";
char senha[] = "34420679";

//Setar IP fixo do dispositivo
byte ip[]      = { 192, 168, 0, 120};
byte gateway[] = { 192, 168, 0, 1 };
byte subnet[]  = { 255, 255, 255, 0 };

//Modbus offset registradores (0-9999)
const int injetora_HREG = 0;
const int referencia_HREG = 1;
const int caixa_HREG = 2;
const int atual_HREG = 3;
const int total_HREG = 4;


ModbusIP mb;//Cria o objeto Modbus

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
  Serial.begin(115200);
  pinMode(sensorPin,INPUT);
  pinMode(RY1pin,OUTPUT);

  Wire.begin(SDAPin, SCLPin);
  lcd.begin();
  uint8_t simbWifiOK[8]  = {0x0, 0x0, 0x7, 0x8, 0x13, 0x14, 0x15}; //Caracter especial: Wifi conectado
  uint8_t simbWifiNOK[8]  = {0x3, 0x4, 0x8, 0x9, 0x0, 0x14, 0x8, 0x14}; //Caracter especial: Wifi desconectado
  
  lcd.createChar(1, simbWifiOK); //Armazena o simbolo do wifi na memoria 1
  lcd.createChar(0, simbWifiNOK); //Armazena o simbolo do wifi desconectado na memoria 0

  mb.config(ssid, senha, ip, gateway, subnet);
  //mb.config(ssid, senha);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Conectando ");
    Serial.println(ssid);
    lcd.setCursor(0,0);
    lcd.print("Conectando Wifi...");
    if(millis()>timeOutWifi) break;
    delay(500);
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());

    lcd.setCursor(0,0);
    lcd.print("Wifi Conectado!");
    lcd.setCursor(0,1);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP());
    delay(3000);
  }
  else{
    Serial.println("WiFi não foi conectado!:(");
    lcd.setCursor(0,0);
    lcd.print("Nao foi possivel");
    lcd.setCursor(0,1);
    lcd.print("conectar o Wifi!");
    delay(3000);
  }

  desenhaMenu1();

  
  //Adiciona os registradores na classe modbus
  mb.addHreg(injetora_HREG, 0);
  mb.addHreg(referencia_HREG, 0);
  mb.addHreg(caixa_HREG, 0);
  mb.addHreg(atual_HREG, 0);
  mb.addHreg(total_HREG, 0);

}

void loop() {
  
  mb.task();//A função task é responsável pelo tráfego de dados Modbus. Quando houver um request neste IP task responde automaticamente
  mb.Hreg(injetora_HREG, strInjetora.toInt());
  mb.Hreg(referencia_HREG, strRef.toInt());
  mb.Hreg(caixa_HREG, intCaixa);
  mb.Hreg(atual_HREG, intAt);
  mb.Hreg(total_HREG, intTotal);
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Verifica se passou uma peça (borda de descida)
  sensor = digitalRead(sensorPin);//caso detecte uma peça, irá para 0
  if (sensor != estadoAtual) {
    estadoAtual = sensor;
    if (sensor == LOW){
      intTotal =intTotal+1;
      intAt = intAt +1;
      Serial.println("Passou uma peça");
      //gravaMemoria(intTotal);
    }
  }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Verifica se foi apertado algum botão de comando (* ou #)

  comando = tecladoCMD.getKey();
//  keypadNum = tecladoNUM.getKey();
  if((comando==NO_KEY) && millis()- lastCmdTime > timeOutCmd){//passaram-se 5 seg e nenhum botão foi apertado: Desativa edição -> posicaoCur = 0
    strAux="";
    posicaoCur = 0;
  }
  if(comando=='s') { //Sobe
    strAux="";
    posicaoCur = posicaoCur -1;
    if(posicaoCur>5) posicaoCur =1;
    if(posicaoCur<1) posicaoCur =5;
    lastCmdTime = millis();
  }
  if(comando=='d') { //Desce
    strAux="";
    posicaoCur = posicaoCur+1;
    if(posicaoCur>5) posicaoCur =1;
    if(posicaoCur<1) posicaoCur =5;
    lastCmdTime = millis();
  }
 

  switch(posicaoCur){
    case 0: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 0: Operação normal, mostra a contagem atual e controla o relé RY1
      strAux="";
      lcd.noCursor();
      lcd.noBlink();
      
      lcd.setCursor(15,2);
      lcd.print(intAt);
      lcd.setCursor(7,3);
      lcd.print(intTotal);
       
      lcd.setCursor(19,3);
      if(WiFi.status() == WL_CONNECTED) lcd.write(1); //Desenha caracter especial de Wifi conectado
      else  lcd.write(0); //Desenha caracter especial de Wifi desconectado

      if(intAt >= intCaixa) digitalWrite(RY1pin,HIGH);
      else digitalWrite(RY1pin,LOW);
      
    break;
    case 1: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 1: Edita string Injetora
      keypadNum = tecladoNUM.getKey();
      lcd.setCursor(9+strAux.length(),0);
      lcd.cursor();
      lcd.blink();
      if(keypadNum!='s' and keypadNum!='d' and keypadNum!=NO_KEY){
        lastCmdTime = millis();//Para reiniciar o timeOut
        lcd.noCursor();
        lcd.noBlink();
        lcd.setCursor(9,0);
        lcd.print("   ");
        lcd.setCursor(9,0);
        strAux += keypadNum;
        strInjetora = strAux; //Invés de usar um int, usei uma string.
        lcd.print(strInjetora);
        //intInjetora = strAux.toInt();
        if(strAux.length()==3) posicaoCur=0;

        Serial.print("strInjetora: ");
        Serial.println(strInjetora);        
      }
    break;
    case 2: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 2: Edita string Ref
      keypadNum = tecladoNUM.getKey();
      lcd.setCursor(4+strAux.length(),1);
      lcd.cursor();
      lcd.blink();
      if(keypadNum!='s' and keypadNum!='d' and keypadNum!=NO_KEY){
        lastCmdTime = millis();//Para reiniciar o timeOut
        lcd.noCursor();
        lcd.noBlink();
        lcd.setCursor(4,1);
        lcd.print("    ");
        lcd.setCursor(4,1);
        strAux += keypadNum;
        strRef = strAux; //Invés de usar um int, usei uma string.
        lcd.print(strRef);
        //intInjetora = strAux.toInt();
        if(strAux.length()==4) posicaoCur=0;

        Serial.print("strRef: ");
        Serial.println(strRef);
      }
    break;
    case 3: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 3: Edita int Caixa
      keypadNum = tecladoNUM.getKey();
      if(strAux.length()==0 and keypadNum=='0') keypadNum=NO_KEY; //Impede que o primeiro dígito deja 0
      lcd.setCursor(6+strAux.length(),2);
      lcd.cursor();
      lcd.blink();
      if(keypadNum!='s' and keypadNum!='d' and keypadNum!=NO_KEY){
        lastCmdTime = millis();//Para reiniciar o timeOut
        lcd.noCursor();
        lcd.noBlink();
        lcd.setCursor(6,2);
        lcd.print("     ");
        lcd.setCursor(6,2);
        strAux += keypadNum;
        lcd.print(strAux);        
        intCaixa = strAux.toInt();
        if(strAux.length()==5) posicaoCur=0;
        Serial.print("intCaixa: ");
        Serial.println(intCaixa);
      }
    break;
    case 4: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 4: Edita int cxAtual
      keypadNum = tecladoNUM.getKey();
      lcd.setCursor(15+strAux.length(),2);
      lcd.cursor();
      lcd.blink();
      if(keypadNum!='s' and keypadNum!='d' and keypadNum!=NO_KEY){
        lastCmdTime = millis();//Para reiniciar o timeOut
        lcd.noCursor();
        lcd.noBlink();
        lcd.setCursor(15,2);
        lcd.print("     ");
        lcd.setCursor(15,2);
        strAux += keypadNum;
        lcd.print(strAux);        
        intAt = strAux.toInt();
        if(strAux.length()==5) posicaoCur=0;
        if(keypadNum == '0' and strAux.length()==1) posicaoCur=0;
        Serial.print("intAt: ");
        Serial.println(intAt);
      }
    break;
    case 5: //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Case posicaoCur 5: Edita unsigned long TOTAL
      keypadNum = tecladoNUM.getKey();
      lcd.setCursor(7+strAux.length(),3);
      lcd.cursor();
      lcd.blink();
      if(keypadNum!='s' and keypadNum!='d' and keypadNum!=NO_KEY){
        lastCmdTime = millis();//Para reiniciar o timeOut
        lcd.noCursor();
        lcd.noBlink();
        lcd.setCursor(7,3);
        lcd.print("       ");
        lcd.setCursor(7,3);
        strAux += keypadNum;
        lcd.print(strAux);
        intTotal = strAux.toInt();
        if(strAux.length()==7) posicaoCur=0;
        if(keypadNum == '0' and strAux.length()==1) posicaoCur=0;
        Serial.print("intTotal: ");
        Serial.println(intTotal);
      }
    break;
    default:
      posicaoCur = 0;
    break;
  }//Fim Switch Case




}//Fim void Loop



void desenhaMenu1(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Injetora=");
  lcd.print(strInjetora);
  lcd.setCursor(15,0);
  lcd.print("IP");
  lcd.print(ip[3]);
  lcd.setCursor(0,1);
  lcd.print("Ref=");
  lcd.print(strRef);
  lcd.setCursor(0,2);
  lcd.print("Caixa=");
  lcd.print(intCaixa);
  lcd.setCursor(12,2);
  lcd.print("At=");
  lcd.setCursor(0,3);
  lcd.print("TOTAL=");

}
