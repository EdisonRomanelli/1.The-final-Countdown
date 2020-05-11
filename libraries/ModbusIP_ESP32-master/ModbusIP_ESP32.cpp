/*
    ModbusIP_ESP32.h - Header for Modbus IP ESP32 Library
	Adapted entirely from ModbusIP_ESP8266.h from André Sarmento Barbosa

*/

#include "ModbusIP_ESP32.h"


WiFiServer server(MODBUSIP_PORT);

ModbusIP::ModbusIP() {

}

void ModbusIP::config(const char* ssid, const char* password) {
	WiFi.begin(ssid, password);
	server.begin();
}

void ModbusIP::config(const char *ssid, const char *password,uint8_t ip[4],uint8_t gateway[4],uint8_t subnet[4]) {
	WiFi.config(IPAddress(ip), IPAddress(gateway), IPAddress(subnet));
	WiFi.begin(ssid, password);
	server.begin();
}

void ModbusIP::task() {
	
	
	//Serial.println("Inicio de task: ");
	//Serial.println("WiFiClient client = server.available();");
	WiFiClient client = server.available();

	int raw_len = 0;
	
 	////Serial.println("Antes gambiarra");
	if (millis()<10000) {
		////Serial.println("dentro gambiarra");	
		return;//gambiarra
	}
	////Serial.println("Depois gambiarra");
	
	//Serial.println("	if (client) {...");
    if (client) {
		//Serial.println("		if (client.connected()) {...");
		if (client.connected()) {
		    for (int x = 0; x < 300; x++) { // Time to have data available
				//Serial.println("			if (client.available()) {...");
				if (client.available()) {
					//Serial.println("				while (client.available() > raw_len) {...");
					while (client.available() > raw_len) {  //Computes data length
						raw_len = client.available();
						//Serial.println("					Dentro do while");
						delay(1);
					}
					break;
				}
				delay(10);
			}
		}
		//Serial.println("		if (raw_len > 7) {...");
		if (raw_len > 7) {
			//Serial.println("			for (int i=0; i<7; i++)	_MBAP[i] = client.read(); //Get MBAP");
			for (int i=0; i<7; i++)	_MBAP[i] = client.read(); //Get MBAP

			_len = _MBAP[4] << 8 | _MBAP[5];
			_len--; // Do not count with last byte from MBAP
			if (_MBAP[2] !=0 || _MBAP[3] !=0) return;   //Not a MODBUSIP packet
			if (_len > MODBUSIP_MAXFRAME) return;      //Length is over MODBUSIP_MAXFRAME
			_frame = (byte*) malloc(_len);

			raw_len = raw_len - 7;
			//Serial.println("			for (int i=0; i< raw_len; i++)	_frame[i] = client.read(); //Get Modbus PDU"); //****
			for (int i=0; i< raw_len; i++)	_frame[i] = client.read(); //Get Modbus PDU
			//Serial.println("						this->receivePDU(_frame);");//**************
			
			this->receivePDU(_frame);//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			
			//Serial.println("			client.flush();");
			client.flush();

			//Serial.println("			if (_reply != MB_REPLY_OFF) {...");
			if (_reply != MB_REPLY_OFF) {
			    //MBAP
				_MBAP[4] = (_len+1) >> 8;     //_len+1 for last byte from MBAP
				_MBAP[5] = (_len+1) & 0x00FF;

				size_t send_len = (unsigned int)_len + 7;
				uint8_t sbuf[send_len];

				for (int i=0; i<7; i++)	    sbuf[i] = _MBAP[i];
				for (int i=0; i<_len; i++)	sbuf[i+7] = _frame[i];

				//Serial.println("				client.write(sbuf, send_len);");
				client.write(sbuf, send_len);
			}
			
			//Serial.println("			client.stop();");
			client.stop();
			free(_frame);
			_len = 0;
		}
	}
	//Serial.println("Fim de task! ");
}
