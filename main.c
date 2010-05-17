/*
 *  main.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/15/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */


#include "recovery.h"

int main() {
	iUSBRecoveryDeviceRef device = iUSBRecoveryDeviceCreateWithPID(kUSBPIDRecovery);
	if(device != NULL) {
		printf("Sending command...\n");
		iUSBRecoveryDeviceSendFile(device, CFSTR("/Users/John/Desktop/ibecv.dfu"), NULL);
		iUSBRecoveryDeviceSendCommand(device, CFSTR("go"));
		iUSBRecoveryDeviceRelease(device);
		sleep(4);
		device = iUSBRecoveryDeviceCreateWithPID(kUSBPIDRecovery);
		CFStringRef response = iUSBRecoveryDeviceReadResponse(device, 1000, 1000);
		CFShow(response);
		if(response) CFRelease(response);
		iUSBRecoveryDeviceRelease(device);
	} else {
		printf("No device connected\n");
	}
	
	return 0;
}