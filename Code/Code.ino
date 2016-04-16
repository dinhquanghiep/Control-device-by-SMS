	#include "SIM900.h"
	#include "GSM.h"
	#include <SoftwareSerial.h>
	#include "sms.h"
	#include <String.h>
	#include <EEPROM.h>
	
	//Define pin
	const char DEVICE[8] = {13, 12, 11, 10, 9, 8, 7, 6};
	const char PASSADDRESS = 0;		//1-10char and #
	const char TIMETBADDRESS = 11; //4char 
	const char PHONENUMBER1 = 15;	//13for each phone number and #
	const char PHONENUMBER2 = 30;
	const unsigned long S2M = 60000;

	char phoneNum1[15] = {0}, phoneNum2[15] = "0938590105";
	char wellcomeText[] = "He thong khoi dong thanh cong";
	char smsBuffer[160] = {0};
	char smsNum[20] = {0};
	boolean autoMode = false; 
	unsigned long xtime[4] = {60000, 60000, 60000, 60000};
	unsigned long breakTime = 0;
	char val[5] = "0000";
	char pass[11]="hiepdq";
	
	SMSGSM sms;
	//Define function
	
	// Reset function
	void(* resetFunc) (void) = 0;
	
void InitPIN(){
	for (int i=0;i<8;i++){
		pinMode(DEVICE[i], OUTPUT);
		digitalWrite(DEVICE[i], HIGH);
	}
}

void ClearBuffer(){
	for (int i=0; i<160; i++){
		smsBuffer[i]=NULL;
	}
	for (int i=0; i<20; i++){
		smsNum[i]=NULL;
	}
}

//Ban do vung nho EEprom
//0-10:	luu pass
//11, 12, 13, 14:	luu khoang thoi gian giua cac lan chay
//15-29: luu so dien thoai 1
//30 - 44: luu so dien thoai 2
void RestoreDataFromEEprom(){
	char c=NULL;
	//Restore pass
	if (EEPROM.read(PASSADDRESS) != 0xFF){
		for (int i=0; i<11; i++){
			c = (char)EEPROM.read(PASSADDRESS+i);
			if (c!=0) *(pass+i)=c;
			else{
				*(pass+i) = 0;
				break;
			}
		}
	}

	//Xu ly phan thoi gian
	if (EEPROM.read(TIMETBADDRESS) != 0xFF){
		for (int i=0, j; i<4; i++){
			j = EEPROM.read(TIMETBADDRESS+i);
			if (j==0) *(xtime+i) = 10*S2M;
			else *(xtime+i) = j*S2M;
		}
	}

	//Restore Phonenumber
	if (EEPROM.read(PHONENUMBER1)!= 0xFF){
		for(int i=0; i<15; i++){
			c = (char)EEPROM.read(PHONENUMBER1+i);
			if(c!=0) *(phoneNum1+i) = c; 
			else{
				*(phoneNum1+i) = 0;
				break;
			} 
		}
	}	

	if (EEPROM.read(PHONENUMBER2)!= 0xFF){
		for(int i=0; i<15; i++){
			c = (char)EEPROM.read(PHONENUMBER2+i);
			if(c!=0) *(phoneNum2+i) = c; 
			else {
				*(phoneNum2+i) = 0;
				break;
			}
		}
	}
}


void InitModuleSIM(){
	gsm.begin(9600);
	gsm.SendATCmdWaitResp("ATE0", 500, 100, str_ok, 3);
	gsm.SendATCmdWaitResp("AT+CMGF=1", 500, 100, str_ok, 3);
	gsm.SendATCmdWaitResp("AT+CLIP=1", 500, 100, str_ok, 3);
	//gsm.SendATCmdWaitResp("AT+CNMI=2, 0", 500, 100, str_ok, 3);
	gsm.InitSMSMemory();
	// Delete all message
	gsm.SendATCmdWaitResp("AT+CMGDA=\"DEL ALL\"", 500, 100, str_ok, 3);
	gsm.SetCommLineStatus(CLS_FREE);
	//Send SMS to control phone
	if (strlen(phoneNum1)>0){
		sms.SendSMS(phoneNum1, wellcomeText);    
	}

	if (strlen(phoneNum2)>0){
		sms.SendSMS(phoneNum2, wellcomeText);    
	}
	
}

void MasterReset(){
	//reset EEprom
	for (int i=0; i<1024; i++) EEPROM.write(i, 0xFF);

	//reset function
	resetFunc();
}

void TurnOff(){
	for (int i=0; i<4; i++) *(val+i)='0';
	digitalWrite(DEVICE[0], HIGH);
	delay(3000);
	for(int i=1; i<5; i++){
		digitalWrite(DEVICE[i], HIGH);
	}
}

char kttk(char *phoneNum){
	char ret_val = -1;
	char *p_char;
	char *p_char1;
	char c=0;
	byte len;

	if (CLS_FREE != gsm.GetCommLineStatus()) return (ret_val);
	gsm.SetCommLineStatus(CLS_ATCMD);
	
	//send "AT+CUSD=1,"*101#" to check the balance
	gsm.SimpleWriteln(F("AT+CUSD=1,\"*101#\""));
	// 5000 msec. for initial comm tmout
	// 100 msec. for inter character tmout
	if (RX_FINISHED_STR_RECV==gsm.WaitResp(10000, 5000, "+CUSD")) {

		// receive the data from post and send it to phoneNum
		// ---------------------------
		p_char = strchr((char *)(gsm.comm_buf), '"');
		Serial.println(p_char);
		p_char1 = p_char+1; // we are on the first data character
		p_char = strchr((char *)(p_char1), '"');
		if (p_char != NULL) {
		  *p_char = 0; // end of string
		  Serial.println(p_char1);
		  len = strlen(p_char1);
			if(len < 160){
				sms.SendSMS(phoneNum, p_char1);
			}else{
				c = *(p_char1+160);
				*(p_char1+160) = 0;
				sms.SendSMS(phoneNum, p_char1);
				*(p_char1+160) = c;
				p_char1 += 160;
				sms.SendSMS(phoneNum, p_char1);
			}
		  ret_val = 1;
		}
	}
	gsm.SetCommLineStatus(CLS_FREE);
	return (ret_val);
}

void smsProcess(char *mData, char *mNum, char *mPass){
	char *p_result;
	char temp[160] = {0};
	int intTemp = 0;
	String strtemp = "";
	strlwr(mData);
	//Check password
	p_result = strstr(mData, "version?");
	if (p_result!=NULL){
		strtemp = "Ban dang su dung bo dieu khien qua tin nhan duoc thiet ke va phat trien boi Dinh Quang Hiep. Version 1.0";
		strtemp.toCharArray(temp, 160);
		sms.SendSMS(mNum, temp);
	}
	if (strncmp(mData, mPass, strlen(mPass))==0 && *(mData+strlen(mPass))=='+'){
		
		//Check command control output state
		p_result = strstr(mData, "+hto");
		if(p_result != NULL){
			if (*(p_result+4)=='n'){
				TurnOff();
				autoMode = true;
				breakTime = millis();
			}else{
				autoMode = false;
				TurnOff();
				sms.SendSMS(mNum, "He thong tuoi da tat");
			}
		}

		p_result = strstr(mData, "+httg");
		if(p_result != NULL){
			for (int i=6, j=0; i<19; i+=4, j++){
				strncpy(temp, p_result+i, 3);
				intTemp = atoi(temp); // "1a5" ==> 1, "10a5" ==> 10
				EEPROM.write(TIMETBADDRESS + j, (byte)intTemp);
				xtime[j] = intTemp*S2M;
			}
			strtemp = "Cai dat thoi gian thanh cong: \n\r";
			for (int i=0 ; i<4; i++){
				strtemp += "Thiet bi " + String(i+1) + ": " + String(EEPROM.read(TIMETBADDRESS+i)) + "phut \n\r";
			}	
			strtemp.toCharArray(temp, 160);
			sms.SendSMS(mNum, temp);
		}

		p_result = strstr(mData, "+allo");
		if(p_result != NULL){
			if (*(p_result+5)=='n'){
				for (int i=5; i<8; i++) digitalWrite(DEVICE[i], LOW);
			}else{
				for (int i=5; i<8; i++) digitalWrite(DEVICE[i], HIGH);
			}
		}

		for (int i=0; i<strlen(mData)-4; i++){
			if (*(mData+i)=='+'){
				if(!strncmp(mData+i, "+tb", 3)){
					if (*(mData+i+3)<'5' && *(mData+i+3)>'0'){
						if (autoMode){
							strtemp = "Tat tuoi tu dong truoc khi tuoi thu cong";
							strtemp.toCharArray(temp, 100);
							sms.SendSMS(mNum, temp);
						}else {
							if (*(mData+i+5)=='n') val[*(mData+i+3)-'1'] = '1';
							else val[*(mData+i+3)-'1'] = '0';
						}
					}else if (*(mData+i+3)>'4' && *(mData+i+3)<'8'){
						if (*(mData+i+5)=='n') digitalWrite(DEVICE[*(mData+i+3)-'0'], LOW);
						else digitalWrite(DEVICE[*(mData+i+3)-'0'], HIGH);
					}
				}
			}
		}

		p_result = strstr(mData, "+tttb");
		if (p_result != NULL){
			strtemp = "Trang thai ngo ra:\n\r";
			for (int i=0; i<8; i++){
				if (digitalRead(DEVICE[i])==LOW) strtemp += "Thiet bi " + String(i) + "_ON_\n\r";
				else strtemp += "Thiet bi " + String(i) + "_OFF_\n\r";
			}
			if (autoMode) strtemp += "Dang tuoi tu dong";
			if (strncmp(val, "0000", 4)!=0) strtemp += "Dang tuoi thu cong";
			strtemp.toCharArray(temp, 160);
			sms.SendSMS(mNum, temp);
		}

		p_result = strstr(mData, "+tttg");
		if (p_result != NULL){
			strtemp = "Thoi gian cai dat:\n\r";
			for (int i=0 ; i<4; i++){
				strtemp += "Thiet bi " + String(i+1) + ": " + String(EEPROM.read(TIMETBADDRESS+i)) + "phut\n\r";
			}	
			strtemp.toCharArray(temp, 160);
			sms.SendSMS(mNum, temp);
		}

		p_result = strstr(mData, "+pass:");
		if (p_result != NULL){
			intTemp = strlen(mPass);
			for (int i=0; i<intTemp; i++) *(mPass+i) = 0;
			for (int i=0; (*(p_result+6+i))!='#' && i<10; i++){
				*(mPass+i)=*(p_result+6+i);
				EEPROM.write(PASSADDRESS+i, *(mPass+i));
			}
			EEPROM.write(PASSADDRESS+strlen(pass), 0);
			Serial.println(pass);
		}

		p_result = strstr(mData, "+num1#");
		if (p_result != NULL){
			intTemp = strlen(phoneNum1);
			for (int i=0; i<intTemp; i++) *(phoneNum1+i) = 0;
			strncpy(phoneNum1, mNum, strlen(mNum));
			for (int i=0; i<strlen(mNum); i++){
				EEPROM.write(PHONENUMBER1+i, *(mNum+i));
			}
			EEPROM.write(PHONENUMBER1 + strlen(mNum), 0);
		}

		p_result = strstr(mData, "+num2#");
		if (p_result != NULL){
			intTemp = strlen(phoneNum2);
			for (int i=0; i<intTemp; i++) *(phoneNum2+i) = 0;
			strncpy(phoneNum2, mNum, strlen(mNum));
			for (int i=0; i<strlen(mNum); i++){
				EEPROM.write(PHONENUMBER2+i, *(mNum+i));
			}
			EEPROM.write(PHONENUMBER2 + strlen(mNum), 0);
		}

		p_result = strstr(mData, "+num2--#");
		if (p_result != NULL){
			intTemp = strlen(phoneNum2);
			for (int i=0; i<intTemp; i++) *(phoneNum2+i) = 0;
			EEPROM.write(PHONENUMBER2, 0);
		}

		p_result = strstr(mData, "+kttk");
		if (p_result != NULL){
			if (kttk(mNum)!=1) sms.SendSMS(mNum, "Khong the kiem tra tai khoan, vui long thu lai sau");
		}

		p_result = strstr(mData, "+naptien");
		if (p_result != NULL){
			intTemp = *(p_result+9+13);
			*(p_result+9+13) = 0;
			gsm.SimpleWrite("ATD+*100*");
			gsm.SimpleWrite(p_result+9);
			gsm.SimpleWriteln('#');
			*(p_result+9+13) = (char) intTemp;
		}
	}
}
void setup() {
	// put your setup code here, to run once:
	InitPIN();
	Serial.begin(9600);
	//Read Pass and time from EEprom
	RestoreDataFromEEprom();
	//Initial Module SIM 900
	delay(3000);
	InitModuleSIM();
}

void loop() {
	// put your main code here, to run repeatedly:
	//Wait for SMS and read SMS
	char c=NULL;
	int smsPosition=0;
	// Serial.println("Khoi tao thanh cong");
	do{
		if (autoMode){
			if (digitalRead(DEVICE[0])==HIGH){
				sms.SendSMS(phoneNum1, "Bat dau tuoi tu dong");
				digitalWrite(DEVICE[1], LOW);
				delay(3000);
				digitalWrite(DEVICE[0], LOW);
			}else if (millis() > breakTime && digitalRead(DEVICE[4])==LOW){
				sms.SendSMS(phoneNum1, "Ket thuc tuoi du dong");
				autoMode = false;
				TurnOff();
			}else if (millis() > breakTime && digitalRead(DEVICE[3])==LOW){
				breakTime = millis()+ xtime[3];
				if (digitalRead(DEVICE[4])==HIGH){
					digitalWrite(DEVICE[4], LOW);
					delay(3000);
					digitalWrite(DEVICE[3], HIGH);
				}
			}else if (millis() > breakTime && digitalRead(DEVICE[2])==LOW){
				breakTime = millis() + xtime[2];
				if (digitalRead(DEVICE[3])==HIGH){
					digitalWrite(DEVICE[3], LOW);
					delay(3000);
					digitalWrite(DEVICE[2], HIGH);
				}	
			}else if (millis() > breakTime + xtime[0] && digitalRead(DEVICE[1])==LOW){
				breakTime = millis()+ xtime[1];
				if (digitalRead(DEVICE[2])==HIGH){
					digitalWrite(DEVICE[2], LOW);
					delay(3000);
					digitalWrite(DEVICE[1], HIGH);
				}
			}
		}else{
			if (!strncmp(val, "0000", 4)){
				if (digitalRead(DEVICE[0])==LOW) TurnOff();
			}else{
				for (int i=0; i<4; i++){
					if (val[i]=='1'){
						digitalWrite(DEVICE[i+1], LOW);
					}
				}
				delay(3000);
				for (int i=0; i<4; i++){
					if (val[i]=='0'){
						digitalWrite(DEVICE[i+1], HIGH);
					}
				}
				delay(1000);
				digitalWrite(DEVICE[0], LOW);
			}

		}
		delay(100);

		smsPosition = sms.IsSMSPresent(SMS_ALL);
	} while (smsPosition<1);
	sms.GetSMS(smsPosition, smsNum, 20, smsBuffer, 160);
	
	//Delete message that we have read, do not use delete all because it
	//maybe also delete unread message
	//Debug SMSbuffer and SMSnumber
	// Serial.println(smsBuffer);
	// Serial.println(smsNum);
	
	sms.DeleteSMS(smsPosition);

	// //Find command in SMS, read pass from EEprom
	smsProcess(smsBuffer, smsNum, pass);

	//display output pin status
	// for (int i=6; i<14; i++) Serial.println(digitalRead(i));
	ClearBuffer();
	//gsm.SendATCmdWaitResp("AT+CMGDA=\"DEL ALL\"", 500, 100, str_ok, 3);
}
