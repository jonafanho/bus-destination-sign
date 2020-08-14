#include "Core.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

Core<U8X8_SSD1322_NHD_256X64_4W_HW_SPI> front(256, 64, D3, D4);
Core<U8X8_SSD1322_NHD_256X64_4W_HW_SPI> side(256, 64, D1, D4);
Core<U8X8_SH1106_128X64_VCOMH0_4W_HW_SPI> back(128, 64, D0, D4);
const uint8_t DISPLAY_COUNT = 3;
uint8_t fileIndices[DISPLAY_COUNT] = {0};
uint8_t selectedGroup = 0;
ESP8266WebServer server(80);
File fileUpload;

String getContentType(String filename) {
	if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else return "text/plain";
}

uint8_t char2int(char input) {
	if (input >= '0' && input <= '9') return input - '0';
	else if (input >= 'A' && input <= 'F') return input - 'A' + 10;
	else if (input >= 'a' && input <= 'f') return input - 'a' + 10;
	else return 0;
}

void uploadFile() {
	HTTPUpload &upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		char fileName[16];
		sprintf(fileName, "/%s", upload.filename.c_str());
		fileUpload = SPIFFS.open(fileName, "w");
	} else if (upload.status == UPLOAD_FILE_WRITE && fileUpload) {
		fileUpload.write(upload.buf, upload.currentSize);
	} else if (upload.status == UPLOAD_FILE_END && fileUpload) {
		fileUpload.close();
		server.send(200, "text/html", "{\"status\":\"success\"}");
	} else {
		server.send(500, "text/html", "{\"status\":\"couldn't write file\"}");
	}
}

void setup() {
	front.display.begin();
	side.display.begin();
	back.display.begin();

	SPIFFS.begin();

	// wifi
	File wifiFile = SPIFFS.open("/wifi.txt", "r");
	if (wifiFile) {
		char ssid[64], password[64];
		wifiFile.readStringUntil('=');
		ssid[wifiFile.readBytesUntil('\r', ssid, sizeof(ssid) - 1)] = 0;
		wifiFile.readStringUntil('=');
		password[wifiFile.readBytesUntil('\r', password, sizeof(password) - 1)] = 0;
		wifiFile.close();

		WiFi.begin(ssid, password);
		for (uint8_t i = 0; i < 20; i++) {
			if (WiFi.status() != WL_CONNECTED) {
				delay(500);
				front.display.setFont(u8x8_font_5x7_f);
				char connecting[128];
				sprintf(connecting, "Connecting to %s...", ssid);
				front.display.drawString(0, 0, connecting);
			} else {
				server.onNotFound([]() {
					String path = server.uri();
					if (!SPIFFS.exists(path)) {
						path = "/index.html";
					}
					File file = SPIFFS.open(path, "r");
					server.streamFile(file, getContentType(path));
					file.close();
				});
				server.on("/upload", HTTP_POST, []() {
					server.send(200, "text/html", "{\"status\":\"ready\"}");
				}, uploadFile);
				server.on("/select", HTTP_POST, []() {
					selectedGroup = server.arg("group").toInt();
					server.send(200, "text/html", "{\"status\":\"ready\"}");
				});
				server.on("/delete", HTTP_POST, []() {
					const uint8_t group = server.arg("group").toInt();
					const uint8_t display = server.arg("display").toInt();
					const uint8_t index = server.arg("index").toInt();
					char path[16];
					sprintf(path, "/%d-%d-%d.txt", group, display, index);
					if (SPIFFS.exists(path)) {
						SPIFFS.remove(path);
					}
					server.send(200, "text/html", "{\"status\":\"success\"}");
				});
				server.begin();

				char connected[128];
				sprintf(connected, "http://%s", WiFi.localIP().toString().c_str());
				char mac[32];
				sprintf(mac, "MAC: %s", WiFi.macAddress().c_str());
				front.display.clear();
				front.display.drawString(0, 0, connected);
				front.display.drawString(0, 1, mac);
				delay(2000);
				break;
			}
		}
	}
}

void loop() {
	server.handleClient();
	for (uint8_t display = 0; display < DISPLAY_COUNT; display++) {
		char path[16];
		while (true) {
			sprintf(path, "/%d-%d-%d.txt", selectedGroup, display, fileIndices[display]);
			if (!SPIFFS.exists(path)) {
				if (fileIndices[display] > 0) {
					fileIndices[display] = 0;
				} else {
					for (uint16_t i = 0; i < 2048; i++) {
						switch (display) {
							default:
								front.setImage(i, 0);
								break;
							case 1:
								side.setImage(i, 0);
								break;
							case 2:
								back.setImage(i, 0);
								break;
						}
					}
					break;
				}
			} else {
				File file = SPIFFS.open(path, "r");
				if (file) {
					uint16_t i = 0;
					while (file.available() && i < 2048) {
						uint8_t data = char2int(file.read()) + (char2int(file.read()) << 4);
						switch (display) {
							default:
								front.setImage(i, data);
								break;
							case 1:
								side.setImage(i, data);
								break;
							case 2:
								back.setImage(i, data);
								break;
						}
						i++;
					}
					fileIndices[display]++;
				}
				file.close();
				break;
			}
		}
	}
	for (uint16_t i = 0; i < 256; i++) {
		front.step(i);
		side.step(i);
		back.step(i);
		server.handleClient();
	}
	for (uint8_t i = 0; i < 32; i++) {
		delay(100);
		server.handleClient();
	}
}