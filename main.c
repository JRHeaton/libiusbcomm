/*
 *  main.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/15/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */


#include "listen.h"

int color = 0;

void connection(iUSBRecoveryDeviceRef device, uint8_t newConnectionState) {
	if(newConnectionState == kUSBConnected) {
		printf("Connected\n");
		if(color == 0) {
			color = 1;
			iUSBRecoveryDeviceSendCommand(device, CFSTR("bgcolor 255 0 0"));
		} else {
			color = 0;
			iUSBRecoveryDeviceSendCommand(device, CFSTR("bgcolor 0 0 255"));
		}
	} else {
		printf("Disconnected\n");
		iUSBRecoveryDeviceRelease(device);
	}
}

int main() {
	
	iUSBListenerRef listener = iUSBListenerCreate(kUSBListenerTypeRecovery, connection);
	if(listener != NULL) {
		if(!iUSBListenerStartListeningOnRunLoop(listener, NULL, NULL)) {
			printf("Failed to start listening\n");
			iUSBListenerRelease(listener);
		} else {
			printf("Listening...\n");
		}
	}
	
	CFRunLoopRun();
	
	return 0;
}