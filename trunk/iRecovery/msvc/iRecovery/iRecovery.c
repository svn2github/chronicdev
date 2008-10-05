/**
  * iRecovery - Utility for DFU, WTF and Recovery
  * Copyright (C) 2008  wEsTbAeR--, tom3q
  * 
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  * 
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  * 
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "usb.h"
#define DFU_MODE	0x1222
#define WTF_MODE	0x1227
#define RECOVERY_MODE	0x1281

/*
 * A helper function dumping c bytes of a.
 */
void iHexDump(char *a, int c)
{
	int b;
	
	for(b=0;b<c;b++) {
		if(b%16==0&&b!=0)
			printf("\n");
		printf("%2.2X ",a[b]);
	}
	printf("\n");
}

/*
 * A helper function imitating GNU readline a bit...
 * (Places read line into *line)
 */
char *iReadLine(char *line)
{
	int c=0;
	char temp=0;

	scanf("%c",&temp);
	while(temp!='\n') {
		//printf("%2.2x\n",temp);
		line[c]=temp;
		c++;
		scanf("%c",&temp);
	}
	line[c]='\0';

	return line;
}

/*
 * A helper function initializing libusb and finding our iPhone/iPod which has given DeviceID
 */
struct usb_dev_handle *iUSBInit(int devid)
{
	struct usb_dev_handle *devPhone = 0;
	struct usb_device *dev;
	struct usb_bus *bus;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	printf("Got USB\n");
	for(bus = usb_get_busses(); bus; bus = bus->next) {
		//printf("Found BUS\n");
		for(dev = bus->devices; dev; dev = dev->next) {
			//printf("\t%4.4X %4.4X\n", dev->descriptor.idVendor, dev->descriptor.idProduct);
			if(dev->descriptor.idVendor == 0x5AC && dev->descriptor.idProduct == devid) {
				// z0mg!!!! iPhone 4th Generation
//				printf("Found iPhone/iPod in DFU 2.0\n");
				devPhone = usb_open(dev);
			}
		}
	}

	return devPhone;
}

/*
 * A helper function closing the USB connection
 */
void iUSBDeInit(struct usb_dev_handle *devPhone)
{
	printf("Closing USB connection...\n");
	usb_close(devPhone);
}

/*
 * Internal command help
 */
void iHelp()
{
	printf("List of internal commands:\n");
	printf("/sendfile <file>\tsend <file> to the phone\n");
	printf("/help\t\t\tshow this help\n");
	printf("/exit\t\t\texit the shell\n");
}
void iSendFile(char *filename);

/*
 * Internal command parser for -s mode
 */
void iParseCmd(char *cmd)
{
	char *command, *parameters;
	command = strtok(cmd, " ");
	parameters = strtok(NULL, " ");

	if(!strcmp("/help", command)) {
		iHelp();
	} else if(!strcmp("/sendfile", command)) {
		iSendFile(parameters);
	} else if(!strcmp("/exit", command)) {
		return;
	} else {
		printf("Unknown internal command. See /help\n");
	}
}

/*
 * Main function of -s mode
 */
void iStartConsole(void)
{
	struct usb_dev_handle *devPhone;
	int ret, length, skip_recv;
	char *buf, *sendbuf, *cmd, *response;

	devPhone = iUSBInit(RECOVERY_MODE);

	if(devPhone == 0) {
		printf("No iPhone/iPod found.\n");
		exit(EXIT_FAILURE);
	}

	if((ret = usb_set_configuration(devPhone, 1)) < 0)
		printf("Error %d when setting configuration\n", ret);

	if((ret = usb_claim_interface(devPhone, 1)) < 0)
		printf("Error %d when claiming interface\n", ret);

	if((ret = usb_set_altinterface(devPhone, 1)) < 0)
		printf("Error %d when setting altinterface\n", ret);

	buf = malloc(0x10001);
	sendbuf = malloc(160);
	cmd = malloc(160);

	skip_recv = 0;

	do {
		if(!skip_recv) {
			ret = usb_bulk_read(devPhone, 0x81, buf, 0x10000, 1000);
			if(ret > 0) {
				response = buf;
				while(response < buf + ret)
				{
					printf("%s", response);
					response += strlen(response) + 1;
				}
			}
		} else {
			skip_recv = 0;
		}
		printf("(Recovery) iPhone$ ");
		iReadLine(cmd);

		if(cmd[0] == '/') {
			iParseCmd(cmd);
			skip_recv = 1;
		} else {
			length = (int)(((strlen(cmd)-1)/0x10)+1)*0x10;
			memset(sendbuf, 0, length);
			memcpy(sendbuf, cmd, strlen(cmd));
			if(!usb_control_msg(devPhone, 0x40, 0, 0, 0, sendbuf, length, 1000)) {
				printf("[ ERROR ] %s", usb_strerror());
			}
		}
	} while(strcmp("/exit", cmd) != 0);

	usb_release_interface(devPhone, 0);
	free(buf);
	free(sendbuf);
	free(cmd);
	iUSBDeInit(devPhone);
}

/*
 * Main function of -f mode
 */
void iSendFile(char *filename)
{
	struct usb_dev_handle *devPhone;
	FILE *file;
	int packets, len, last, i, a, c, sl;
	char *fbuf, buf[6];
	
	if(!filename)
		return;

	file = fopen(filename, "rb");
	if(file == NULL) {
		printf("File %s not found.\n", filename);
		exit(EXIT_FAILURE);
	}
	fseek(file, 0, 0);
	fclose(file);

	devPhone = iUSBInit(WTF_MODE);
	if(!devPhone) {
		devPhone = iUSBInit(RECOVERY_MODE);
		if(devPhone)
			printf("Found iPhone/iPod in Recovery mode\n");
	} else {
		printf("Found iPhone/iPod in DFU/WTF mode\n");
	}

	if(!devPhone) {
		printf("No iPhone/iPod found.\n");
		exit(EXIT_FAILURE);
	}

	if(usb_set_configuration(devPhone,	1))
		printf("Error setting configuration\n");

	printf("\n");

	file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	len = ftell(file);
	fseek(file, 0, 0);

	packets = len / 0x800;
	if(len % 0x800)
		packets++;
	last = len % 0x800;

	printf("Loaded image file (len: 0x%x, packets: %d, last: 0x%x).\n", len, packets, last);

	fbuf = malloc(packets * 0x800);
	if(!last)
		last = 0x800;
	fread(fbuf, 1, len, file);
	fclose(file);

	printf("Sending 0x%x bytes\n", len);

	for(i=0, a=0, c=0; i<packets; i++, a+=0x800, c++) {
		sl = 0x800;

		if(i == packets-1)
			sl = last;

		printf("Sending 0x%x bytes in packet %d... ", sl, c);

		if(usb_control_msg(devPhone, 0x21, 1, c, 0, &fbuf[a], sl, 1000)) {
			printf(" OK\n");
		} else{
			printf(" x\n");
		}

		if(usb_control_msg(devPhone, 0xA1, 3, 0, 0, buf, 6, 1000) != 6){
			printf("Error receiving status!\n");
		} else {
			iHexDump(buf, 6);
			if(buf[4]!=5)
				printf("Status error!\n");
		}
	}

	printf("Successfully uploaded file!\nExecuting it...\n");	

	usb_control_msg(devPhone, 0x21, 1, c, 0, fbuf, 0, 1000);

	for(i=6; i<=8; i++) {
		if(usb_control_msg(devPhone, 0xA1, 3, 0, 0, buf, 6, 1000) != 6){
			printf("Error receiving status!\n");
		} else {
			iHexDump(buf, 6);
			if(buf[4]!=i)
				printf("Status error!\n");
		}
	}

	free(fbuf);
	iUSBDeInit(devPhone);
}

/*
 * Shows the Arguments of the application.
 */
int iShowUsage(void)
{
		printf("./iRecovery [args]\n");
		printf("\t-f <file>\t\tupload file in DFU, WTF and Recovery modes\n");
		printf("\t-s\t\t\tstarts a shell in Recovery mode\n\n");
}

/*
 * Main function of the application
 */
int main(int argc, char *argv[])
{
	printf("iRecovery - Recovery Utility\n");
	printf("by wEsTbAeR-- and Tom3q\n\n");
	if(argc < 2) {
		iShowUsage();
		exit(EXIT_FAILURE);
	}

	if(argv[1][0] != '-') {
		iShowUsage();
		exit(EXIT_FAILURE);
	}
	
	if(strcmp(argv[1], "-f") == 0) {
		if(argc < 3) {
			// error when 3rd arg for file upload is not set
			printf("No valid file set.\n");
			exit(EXIT_FAILURE);
		} 
		iSendFile(argv[2]);
	} else if(strcmp(argv[1], "-s") == 0) {
		iStartConsole();
	}
	
	return 0;
}
