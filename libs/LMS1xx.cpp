/*
 * LMS1xx.cpp
 *
 *  Created on: 09-08-2010
 *  Author: Konrad Banachowicz
 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/

#include <sys/types.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "ofMain.h"

#include "LMS1xx.h"

LMS1xx::LMS1xx() :
	connected(false) {
	debug = false;
}

LMS1xx::~LMS1xx() {

}

void LMS1xx::connect(std::string host, int port) {
	if (!connected) {
        connected = tcpClient.setup(host,port,true);
        if(debug) tcpClient.setVerbose(true);
	}
}

void LMS1xx::disconnect() {
	if (connected) {
        tcpClient.close();
		connected = false;
	}
}

bool LMS1xx::isConnected() {
	return connected;
}

void LMS1xx::startMeas() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sMN LMCstartmeas", 0x03);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
}

void LMS1xx::stopMeas() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sMN LMCstopmeas", 0x03);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
}

status_t LMS1xx::queryStatus() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sRN STlms", 0x03);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
  
	int ret;
	sscanf((buf + 10), "%d", &ret);
	return (status_t) ret;
}

void LMS1xx::login() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sMN SetAccessMode 03 F4724744", 0x03);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
}

scanCfg LMS1xx::getScanCfg(){
	scanCfg cfg;
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sRN LMPscancfg", 0x03);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);

	sscanf(buf + 1, "%*s %*s %X %*d %X %X %X", &cfg.scaningFrequency,
			&cfg.angleResolution, &cfg.startAngle, &cfg.stopAngle);
	return cfg;
}

void LMS1xx::setScanCfg(const scanCfg &cfg) {
	char buf[100];
	sprintf(buf, "%c%s %X +1 %X %X %X%c", 0x02, "sMN mLMPsetscancfg",
			cfg.scaningFrequency, cfg.angleResolution, cfg.startAngle,
			cfg.stopAngle, 0x03);

    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);

	buf[len - 1] = 0;
}

void LMS1xx::setScanDataCfg(const scanDataCfg &cfg) {
	char buf[100];
	sprintf(buf, "%c%s %02X 00 %d %d 0 %02X 00 %d %d 0 %d +%d%c", 0x02,
			"sWN LMDscandatacfg", cfg.outputChannel, cfg.remission ? 1 : 0,
			cfg.resolution, cfg.encoder, cfg.position ? 1 : 0,
			cfg.deviceName ? 1 : 0, cfg.timestamp ? 1 : 0, cfg.outputInterval, 0x03);
	if(debug)
		printf("%s\n", buf);
  
    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
	buf[len - 1] = 0;
}

void LMS1xx::scanContinous(int start) {
	char buf[100];
	sprintf(buf, "%c%s %d%c", 0x02, "sEN LMDscandata", start, 0x03);

    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);

	if (buf[0] != 0x02)
		printf("invalid packet recieved\n");

	if (debug) {
		buf[len] = 0;
		printf("%s\n", buf);
	}

	if (start == 0) {
		for (int i = 0; i < 10; i++)
            tcpClient.receiveRawBytes(buf,100);
	}
}

vector<char> leftovers;

string repeat(string str, int n) {
	string out = "";
	for(int i = 0; i < n; i++) {
		out += str;
	}
	return out;
}

// originally was 20000
#define DATA_BUF_LEN 40000
void LMS1xx::getData(scanData& data) {
	char raw[DATA_BUF_LEN];
	
	// step 1: read everything available from the network
	// step 2: read local buffer from STX 0x02 to ETX 0x03
	// step 3: parse most oldest data in fixed size queue
	
	char buf[DATA_BUF_LEN];
	int len = 0;
	
	if(leftovers.size() > 0) {
		if(debug)
			cout << "copying " << leftovers.size() << " leftovers, starting with 0x" << ofToHex(leftovers[0]) << endl;
		for(int i = 0; i < leftovers.size(); i++) {
			buf[len] = leftovers[i];
			len++;
		}
		leftovers.clear();
	}
	
	unsigned long start;
	if(debug) {
		start = ofGetSystemTime();
		cout << "inside getData()" << endl;
	}
	while(true) {
		if(debug)
			cout << "inside do while. ";

        int curLen = tcpClient.receiveRawBytes(raw,DATA_BUF_LEN);
		if(debug)cout << "(" << curLen << " chars) ";
      
		bool done = false;
		for(int i = 0; i < curLen; i++) {
			if(raw[i] == 0x03) { // found an ETX
				if(debug)
					cout << "ETX at " << i << " ";
				leftovers.assign(raw + i + 1, raw + curLen); // copy remaining to leftovers
				done = true;
				break;
			} else { // copy a single char
				buf[len] = raw[i];
				len++;
			}
		}
		if(debug)
			cout << endl;
		if(done) {
			break;
        }
	}
	if(debug) {
		unsigned long stop = ofGetSystemTime();
		cout << "receive time: " << (stop - start) << " with " << len << " bytes received and " << leftovers.size() << " leftover" << endl;
	}

	buf[len - 1] = 0;
	char* tok = strtok(buf, " "); //Type of command
	tok = strtok(NULL, " "); //Command
	tok = strtok(NULL, " "); //VersionNumber
	tok = strtok(NULL, " "); //DeviceNumber
	tok = strtok(NULL, " "); //Serial number
	tok = strtok(NULL, " "); //DeviceStatus
	tok = strtok(NULL, " "); //MessageCounter
	tok = strtok(NULL, " "); //ScanCounter
	tok = strtok(NULL, " "); //PowerUpDuration
	tok = strtok(NULL, " "); //TransmissionDuration
	tok = strtok(NULL, " "); //InputStatus
	tok = strtok(NULL, " "); //OutputStatus
	tok = strtok(NULL, " "); //ReservedByteA
	tok = strtok(NULL, " "); //ScanningFrequency
	tok = strtok(NULL, " "); //MeasurementFrequency
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); //NumberEncoders
	int NumberEncoders;
	sscanf(tok, "%d", &NumberEncoders);
	for (int i = 0; i < NumberEncoders; i++) {
		tok = strtok(NULL, " "); //EncoderPosition
		tok = strtok(NULL, " "); //EncoderSpeed
	}

	tok = strtok(NULL, " "); //NumberChannels16Bit
	int NumberChannels16Bit;
	sscanf(tok, "%d", &NumberChannels16Bit);
	if (debug)
		printf("NumberChannels16Bit : %d\n", NumberChannels16Bit);
	for (int i = 0; i < NumberChannels16Bit; i++) {
		int type = -1; // 0 DIST1 1 DIST2 2 RSSI1 3 RSSI2
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1")) {
			type = 0;
		} else if (!strcmp(content, "DIST2")) {
			type = 1;
		} else if (!strcmp(content, "RSSI1")) {
			type = 2;
		} else if (!strcmp(content, "RSSI2")) {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (debug)
			printf("NumberData : %d\n", NumberData);

		if (type == 0) {
			data.dist_len1 = NumberData;
		} else if (type == 1) {
			data.dist_len2 = NumberData;
		} else if (type == 2) {
			data.rssi_len1 = NumberData;
		} else if (type == 3) {
			data.rssi_len2 = NumberData;
		}

		for (int i = 0; i < NumberData; i++) {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
				data.dist1[i] = dat;
			} else if (type == 1) {
				data.dist2[i] = dat;
			} else if (type == 2) {
				data.rssi1[i] = dat;
			} else if (type == 3) {
				data.rssi2[i] = dat;
			}

		}
	}

	tok = strtok(NULL, " "); //NumberChannels8Bit
	int NumberChannels8Bit;
	sscanf(tok, "%d", &NumberChannels8Bit);
	if (debug)
		printf("NumberChannels8Bit : %d\n", NumberChannels8Bit);
	for (int i = 0; i < NumberChannels8Bit; i++) {
		int type = -1;
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1")) {
			type = 0;
		} else if (!strcmp(content, "DIST2")) {
			type = 1;
		} else if (!strcmp(content, "RSSI1")) {
			type = 2;
		} else if (!strcmp(content, "RSSI2")) {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (debug)
			printf("NumberData : %d\n", NumberData);

		if (type == 0) {
			data.dist_len1 = NumberData;
		} else if (type == 1) {
			data.dist_len2 = NumberData;
		} else if (type == 2) {
			data.rssi_len1 = NumberData;
		} else if (type == 3) {
			data.rssi_len2 = NumberData;
		}
		for (int i = 0; i < NumberData; i++) {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
				data.dist1[i] = dat;
			} else if (type == 1) {
				data.dist2[i] = dat;
			} else if (type == 2) {
				data.rssi1[i] = dat;
			} else if (type == 3) {
				data.rssi2[i] = dat;
			}
		}
	}

}

void LMS1xx::saveConfig() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sMN mEEwriteall", 0x03);

    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
}

void LMS1xx::startDevice() {
	char buf[100];
	sprintf(buf, "%c%s%c", 0x02, "sMN Run", 0x03);

    tcpClient.sendRawBytes(buf,strlen(buf));
    int len = tcpClient.receiveRawBytes(buf,100);
}