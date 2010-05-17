/*
 *  main.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/15/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */


#include "recovery.h"

void disconnect(iUSBRecoveryDeviceRef device, uint8_t newConnectionState) {
	printf("Device disconnected...\n");
	iUSBRecoveryDeviceRelease(device);
}

int main() {
	iUSBRecoveryDeviceNotificationContext context;
	context.disconnectCallback = disconnect;
	context.runLoop = NULL;
	context.runLoopMode = NULL;
	
	iUSBRecoveryDeviceRef device = iUSBRecoveryDeviceCreate(kUSBPIDRecovery, &context);
	if(device != NULL) {
		printf("Sending command...\n");
		iUSBRecoveryDeviceSendCommand(device, CFSTR("setenv auto-boot false"));
		iUSBRecoveryDeviceSendCommand(device, CFSTR("saveenv"));
		iUSBRecoveryDeviceSendCommand(device, CFSTR("reboot"));
		CFRunLoopRun();
	} else {
		printf("No device connected\n");
	}
	
	return 0;
}