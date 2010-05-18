/*
 *  listen.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/16/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */

#include "listen.h"
#include "helper.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

struct __iUSBListener {
	int listenModes;
	struct {
		uint8_t subscribed;
		IONotificationPortRef notifyPort;
		iUSBRecoveryDeviceConnectionChangeCallback connectionCallback;
		iUSBRecoveryDeviceRef currentDevice;
	} recoveryVars;
};

HIDDEN iUSBRecoveryDeviceRef createRecoveryDevice(uint16_t pid, io_service_t service);
HIDDEN Boolean deviceOpen(iUSBRecoveryDeviceRef device, CFMutableDictionaryRef matching);

HIDDEN int subscribeToRecoveryConnections(iUSBListenerRef listener, uint16_t *pids, int pid_count); 
HIDDEN void recoveryDeviceAttached(void *refCon, io_iterator_t iterator);
HIDDEN void recoveryDeviceDetached(void *refCon, io_iterator_t iterator);

iUSBListenerRef iUSBListenerCreate(iUSBListenerType listenModes, iUSBRecoveryDeviceConnectionChangeCallback recoveryCallback) {
	iUSBListenerRef newListener = calloc(1, sizeof(struct __iUSBListener));
	if(recoveryCallback != NULL) newListener->recoveryVars.connectionCallback = recoveryCallback;
	newListener->listenModes = listenModes;
	
	return newListener;
}

Boolean iUSBListenerStartListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop_, CFStringRef runLoopMode_) {
	if(listener == NULL) return 0;
	
	if(listener->listenModes & kUSBListenerTypeRecovery) {
		if(!listener->recoveryVars.subscribed) {
			uint16_t pids[3] = {
				kUSBPIDRecovery,
				kUSBPIDDFU,
				kUSBPIDWTF
			};
			if(subscribeToRecoveryConnections(listener, pids, 3) < 0) return 0;
			listener->recoveryVars.subscribed = 1;
		}
	}

	CFRunLoopRef runLoop = (runLoop_ == NULL ? CFRunLoopGetCurrent() : runLoop_);
	CFStringRef runLoopMode = (runLoopMode_ == NULL ? kCFRunLoopDefaultMode : runLoopMode_);
	CFRunLoopSourceRef notifySource = IONotificationPortGetRunLoopSource(listener->recoveryVars.notifyPort);
	
	if(!CFRunLoopContainsSource(runLoop, notifySource, runLoopMode)) {
		CFRunLoopAddSource(runLoop, notifySource, runLoopMode);
	}
	
	return 1;
}

void iUSBListenerStopListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop_, CFStringRef runLoopMode_) {
	if(listener == NULL || !listener->recoveryVars.subscribed) return;
	
	CFRunLoopRef runLoop = (runLoop_ == NULL ? CFRunLoopGetCurrent() : runLoop_);
	CFStringRef runLoopMode = (runLoopMode_ == NULL ? kCFRunLoopDefaultMode : runLoopMode_);
	CFRunLoopSourceRef notifySource = IONotificationPortGetRunLoopSource(listener->recoveryVars.notifyPort);
	
	if(CFRunLoopContainsSource(runLoop, notifySource, runLoopMode)) {
		CFRunLoopRemoveSource(runLoop, notifySource, runLoopMode);
	}
	
	return;
}

void iUSBListenerRelease(iUSBListenerRef listener) {
	if(listener != NULL) {
		if(listener->recoveryVars.subscribed) {
			if(listener->recoveryVars.notifyPort) IONotificationPortDestroy(listener->recoveryVars.notifyPort);
		}
		
		free(listener);
		listener = NULL;
	}
}

HIDDEN int subscribeToRecoveryConnections(iUSBListenerRef listener, uint16_t *pids, int pid_count) {
	if(listener == NULL) return -1;

	listener->recoveryVars.notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
	
	int i;
	for(i = 0; i < pid_count; ++i) {
		CFMutableDictionaryRef matching = IOServiceMatching(kIOUSBDeviceClassName);
		if(matching == NULL) {
			return -1;
		}
		
		uint16_t idVendorApple = 0x05AC, PID = pids[i];
		CFNumberRef idVendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &idVendorApple);
		CFNumberRef idProduct = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &PID);
		
		CFDictionarySetValue(matching, CFSTR(kUSBVendorID), idVendor);
		CFDictionarySetValue(matching, CFSTR(kUSBProductID), idProduct);
		
		CFRelease(idVendor);
		CFRelease(idProduct);
		
		CFRetain(matching);
		
		io_iterator_t attachIterator;
		if(IOServiceAddMatchingNotification(listener->recoveryVars.notifyPort, kIOFirstMatchNotification, matching, recoveryDeviceAttached, listener, &attachIterator) != KERN_SUCCESS) {
			return -1;
		}
		
		recoveryDeviceAttached(listener, attachIterator);
		
		io_iterator_t detachIterator;
		if(IOServiceAddMatchingNotification(listener->recoveryVars.notifyPort, kIOTerminatedNotification, matching, recoveryDeviceDetached, listener, &detachIterator) != KERN_SUCCESS) {
			return -1;
		}
		
		recoveryDeviceDetached(listener, detachIterator);
	}
	
	return 0;
}

HIDDEN void recoveryDeviceAttached(void *refCon, io_iterator_t iterator) {
	iUSBListenerRef listener = refCon;
	if(listener != NULL) {
		io_service_t service;
		while(service = IOIteratorNext(iterator)) {
			if(listener->recoveryVars.connectionCallback != NULL) {
				CFNumberRef CFPID = IORegistryEntryCreateCFProperty(service, CFSTR(kUSBProductID), kCFAllocatorDefault, 0);
				uint16_t idProduct;
				CFNumberGetValue(CFPID, kCFNumberSInt16Type, &idProduct);
				CFRelease(CFPID);
			
				iUSBRecoveryDeviceRef newDevice = createRecoveryDevice(idProduct, service);
				if(!deviceOpen(newDevice, NULL)) {
					free(newDevice);
					IOObjectRelease(service);
					return;
				}
				
				listener->recoveryVars.currentDevice = newDevice;
				listener->recoveryVars.connectionCallback(listener->recoveryVars.currentDevice, kUSBConnected);
			}
		}
	}
}

HIDDEN void recoveryDeviceDetached(void *refCon, io_iterator_t iterator) {
	iUSBListenerRef listener = refCon;
	if(listener != NULL) {
		io_service_t service;
		while(service = IOIteratorNext(iterator)) {
			if(listener->recoveryVars.connectionCallback != NULL) {
				listener->recoveryVars.connectionCallback(listener->recoveryVars.currentDevice, kUSBDisconnected);
			}
		}
	}
}
