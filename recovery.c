/*
 *  recovery.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/16/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */

#include "recovery.h"
#include "helper.h"

#include <sys/stat.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

struct __iUSBRecoveryDevice {
	uint16_t idProduct;
	io_service_t usbService;
	IOUSBDeviceInterface **deviceHandle;
	IOUSBInterfaceInterface **interfaceHandle;
	UInt8 responsePipeRef;
	CFDictionaryRef properties;
	Boolean open;
	iUSBRecoveryDeviceConnectionChangeCallback disconnectCallback;
	IONotificationPortRef disconnectNPort;
};

#define RECOVERY_DEVICE_SIZE sizeof(struct __iUSBRecoveryDevice)
#define HIDDEN __attribute__ ((visibility("hidden")))

HIDDEN int deviceGetStatus(iUSBRecoveryDeviceRef device, int flag);
HIDDEN Boolean deviceOpen(iUSBRecoveryDeviceRef device, CFMutableDictionaryRef matching);
HIDDEN void deviceDisconnected(void *refCon, io_iterator_t iterator);

iUSBRecoveryDeviceRef iUSBRecoveryDeviceCreate(uint16_t pid, iUSBRecoveryDeviceNotificationContext *context) {
	CFMutableDictionaryRef matching = IOServiceMatching(kIOUSBDeviceClassName);
	if(matching == NULL) 
		return NULL;
	
	CFNumberRef idVendor = AppleIncVendorID();
	CFNumberRef idProduct = numberForUInt16(pid);
	
	CFDictionarySetValue(matching, CFSTR(kUSBVendorID), (const void *)idVendor);
	CFDictionarySetValue(matching, CFSTR(kUSBProductID), (const void *)idProduct);
	
	CFRelease(idVendor);
	CFRelease(idProduct);
	
	CFRetain(matching);
	
	io_service_t usbService = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
	if(!usbService) 
		return NULL;
	
	iUSBRecoveryDeviceRef newDevice = calloc(1, RECOVERY_DEVICE_SIZE);
	newDevice->idProduct = pid;
	newDevice->usbService = usbService;
	
	if(!deviceOpen(newDevice, matching)) {
		free(newDevice);
		return NULL;
	}
	
	if(context != NULL) {
		if(context->disconnectCallback != NULL) {
			CFRunLoopSourceRef notifySource = IONotificationPortGetRunLoopSource(newDevice->disconnectNPort);
			
			newDevice->disconnectCallback = context->disconnectCallback;
			if(context->runLoop != NULL) {
				if(context->runLoopMode != NULL) {
					CFRunLoopAddSource(context->runLoop, notifySource, context->runLoopMode);
				} else {
					CFRunLoopAddSource(context->runLoop, notifySource, kCFRunLoopDefaultMode);
				}
			} else {
				if(context->runLoopMode != NULL) {
					CFRunLoopAddSource(CFRunLoopGetCurrent(), notifySource, context->runLoopMode);
				} else {
					CFRunLoopAddSource(CFRunLoopGetCurrent(), notifySource, kCFRunLoopDefaultMode);
				}
			}
		}
	}
	
	return newDevice;
}

void iUSBRecoveryDeviceRelease(iUSBRecoveryDeviceRef device) {
	if(device != NULL) {
		if(device->open) {
			if(device->deviceHandle) (*device->deviceHandle)->USBDeviceClose(device->deviceHandle);
			if(device->deviceHandle) (*device->deviceHandle)->Release(device->deviceHandle);
			if(device->interfaceHandle) (*device->interfaceHandle)->USBInterfaceClose(device->interfaceHandle);
			if(device->interfaceHandle) (*device->interfaceHandle)->Release(device->interfaceHandle);
			if(device->properties) CFRelease(device->properties);
			if(device->disconnectNPort) IONotificationPortDestroy(device->disconnectNPort);
		}
		if(device->usbService) IOObjectRelease(device->usbService);
		device->open = 0;
		
		free(device);
	}
}

uint16_t iUSBRecoveryDeviceGetPID(iUSBRecoveryDeviceRef device) {
	if(device == NULL) 
		return 0;
	
	return device->idProduct;
}

Boolean iUSBRecoveryDeviceSendCommand(iUSBRecoveryDeviceRef device, CFStringRef command) {
	if(device == NULL || command == NULL || !device->open || !iUSBRecoveryDeviceIsInRecoveryMode(device))
		return 0;
	
	int bufsize = (CFStringGetLength(command)+1);
	char *cmdBuf = calloc(1, bufsize);
	CFStringGetCString(command, cmdBuf, bufsize, kCFStringEncodingUTF8);
	
	IOUSBDevRequest request;
	request.bmRequestType = kUSBRequestCommand;
	request.bRequest = 0x0;
	request.wValue = 0x0;
	request.wIndex = 0x0;
	request.wLength = (UInt16)bufsize;
	request.pData = (void *)cmdBuf;
	request.wLenDone = 0x0;
	
	Boolean retVal;
	
	if((*device->deviceHandle)->DeviceRequest(device->deviceHandle, &request) != kIOReturnSuccess) {
		retVal = 0;
	} else {
		retVal = 1;
	}
	
	free(cmdBuf);
	
	return retVal;
}

Boolean iUSBRecoveryDeviceSendFile(iUSBRecoveryDeviceRef device, CFStringRef filePath, iUSBRecoveryDeviceTransferProgressCallback progressCallback) {
	if(device == NULL || filePath == NULL || !device->open)
		return 0;
	
	int bufsize = (CFStringGetLength(filePath) + 1);
	char path[bufsize];
	CFStringGetCString(filePath, path, bufsize, kCFStringEncodingUTF8);
	
	unsigned char *buf;
	unsigned int packet_size = 0x800;
	struct stat check;
	
	if(stat(path, &check) != 0) {
		return 0;
	}
	
	buf = malloc(check.st_size);
	memset(buf, '\0', check.st_size);
	
	FILE *file = fopen(path, "r");
	if(file == NULL) {
		return 0;
	}
	
	if(fread((void *)buf, check.st_size, 1, file) == 0) {
		fclose(file);
		free(buf);
		return 0;
	}
	
	fclose(file);
	
	unsigned int packets, current;
	packets = (check.st_size / packet_size);
	if(check.st_size % packet_size) {
		packets++;
	}
	
	for(current = 0; current < packets; ++current) {
		int size = (current + 1 < packets ? packet_size : (check.st_size % packet_size));
		
		IOUSBDevRequest file_request;
		
		file_request.bmRequestType = kUSBRequestFile;
		file_request.bRequest = 0x1;
		file_request.wValue = current;
		file_request.wIndex = 0x0;
		file_request.wLength = (UInt16)size;
		file_request.pData = (void *)&buf[current * packet_size];
		file_request.wLenDone = 0x0;
		
		if((*device->deviceHandle)->DeviceRequest(device->deviceHandle, &file_request) != kIOReturnSuccess) {
			free(buf);
			return 0;
		}
		
		if(deviceGetStatus(device, 5) != 0) {
			free(buf);
			return 0;
		}
	
		if(progressCallback)  {
			float progress = (((current + 1) * 100) / packets);
			progressCallback(progress);
		}
	}
	
	IOUSBDevRequest checkup;
	checkup.bmRequestType = kUSBRequestFile;
	checkup.bRequest = 0x1;
	checkup.wValue = current;
	checkup.wIndex = 0x0;
	checkup.wLength = 0x0;
	checkup.pData = buf;
	checkup.wLenDone = 0x0;
	
	(*device->deviceHandle)->DeviceRequest(device->deviceHandle, &checkup);
	
	for(current = 6; current < 8; ++current) {
		if(deviceGetStatus(device, current) != 0) {
			free(buf);
			return 0;
		}
	}
	
	free(buf);
	
	return 1;
}

CFStringRef iUSBRecoveryDeviceReadResponse(iUSBRecoveryDeviceRef device, UInt32 noDataTimeout, UInt32 completionTimout) {
	if(device == NULL || !device->open || !iUSBRecoveryDeviceIsInRecoveryMode(device))
		return NULL;
	
	UInt32 buf_size = 0x800;
	char buf[buf_size];
	
	if(((IOUSBInterfaceInterface182 *)(*device->interfaceHandle))->ReadPipeTO(device->interfaceHandle, device->responsePipeRef, buf, &buf_size, noDataTimeout, completionTimout) != 0)
		return NULL;

	if(buf[0] == '\0') return NULL;
	
	char fixed_buf[buf_size];
	
	int bi, fi;
	for(bi = 0, fi = 0; bi < buf_size; ++bi) {
		if(buf[bi] == '\0') {
			if(buf[bi+1] == '\0' && buf[bi+2] == '\0') {
				fixed_buf[fi] = '\0';
				break;
			}
		} else {
			fixed_buf[fi] = buf[bi];
			fi++;
		}
	}
	
	CFStringRef response = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)fixed_buf, kCFStringEncodingUTF8);
	
	return response;
}

Boolean iUSBRecoveryDeviceSendControlMessage(iUSBRecoveryDeviceRef device, UInt8 bmRequestType, UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt16 wLength, void *pData, UInt32 wLenDone) {
	if(!device->open)
		return 0;
	
	IOUSBDevRequest request;
	request.bmRequestType = bmRequestType;
	request.bRequest = bRequest;
	request.wValue = wValue;
	request.wIndex = wIndex;
	request.wLength = wLength;
	request.pData = pData;
	request.wLenDone = wLenDone;
	
	if((*device->deviceHandle)->DeviceRequest(device->deviceHandle, &request) != kIOReturnSuccess) {
		return 0;
	}
	
	return 1;
}

Boolean iUSBRecoveryDeviceIsInRecoveryMode(iUSBRecoveryDeviceRef device) {
	return (device->idProduct == kUSBPIDRecovery ? 1 : 0);
}

void iUSBRecoveryDeviceReboot(iUSBRecoveryDeviceRef device) {
	iUSBRecoveryDeviceSendCommand(device, CFSTR("reboot"));
}

void iUSBRecoveryDeviceSetAutoBoot(iUSBRecoveryDeviceRef device, Boolean autoBoot) {
	iUSBRecoveryDeviceSendCommand(device, (autoBoot ? CFSTR("setenv auto-boot true") : CFSTR("setenv auto-boot false")));
	iUSBRecoveryDeviceSendCommand(device, CFSTR("saveenv"));
}

HIDDEN int deviceGetStatus(iUSBRecoveryDeviceRef device, int flag) { 
	if(!device->open)
		return -1;
	
	IOUSBDevRequest status_request;
	char response[6];
	
	status_request.bmRequestType = kUSBRequestStatus;
	status_request.bRequest = 0x3;
	status_request.wValue = 0x0;
	status_request.wIndex = 0x0;
	status_request.wLength = 0x6;
	status_request.pData = (void *)response;
	status_request.wLenDone = 0x0;
	
	if((*device->deviceHandle)->DeviceRequest(device->deviceHandle, &status_request) != kIOReturnSuccess) {
		return -1;
	}
	
	if(response[4] != flag) {
		return -1;
	}
	
	return 0;
}

HIDDEN Boolean deviceOpen(iUSBRecoveryDeviceRef device, CFMutableDictionaryRef matching) {
	IOCFPlugInInterface **pluginInterface;
	IOUSBDeviceInterface **deviceHandle;
	IOUSBInterfaceInterface **interfaceHandle;
	
	io_service_t service = device->usbService;
	
	SInt32 score;
	if(IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &pluginInterface, &score) != 0) {
		IOObjectRelease(service);
		return 0;
	}
	
	if((*pluginInterface)->QueryInterface(pluginInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&deviceHandle) != 0) {
		IOObjectRelease(service);
		return 0;
	}
	
	(*pluginInterface)->Release(pluginInterface);
	
	if((*deviceHandle)->USBDeviceOpen(deviceHandle) != 0) { 
		IOObjectRelease(service);
		(*deviceHandle)->Release(deviceHandle);
		return 0;
	}
	
	if((*deviceHandle)->SetConfiguration(deviceHandle, 1) != 0) {
		IOObjectRelease(service);
		(*deviceHandle)->USBDeviceClose(deviceHandle);
		(*deviceHandle)->Release(deviceHandle);
		return 0;
	}
	
	io_iterator_t iterator;
	IOUSBFindInterfaceRequest interfaceRequest;
	
	interfaceRequest.bAlternateSetting 
	= interfaceRequest.bInterfaceClass 
	= interfaceRequest.bInterfaceProtocol 
	= interfaceRequest.bInterfaceSubClass 
	= kIOUSBFindInterfaceDontCare;
	
	if((*deviceHandle)->CreateInterfaceIterator(deviceHandle, &interfaceRequest, &iterator) != 0) {
		IOObjectRelease(service);
		(*deviceHandle)->USBDeviceClose(deviceHandle);
		(*deviceHandle)->Release(deviceHandle);
		return -1;
	}
	
	io_service_t usbInterface;
	UInt8 found_interface = 0, index = 0;
	while(usbInterface = IOIteratorNext(iterator)) {
		if(index < 1) {
			index++;
			continue;
		}
		
		IOCFPlugInInterface **iodev;
		
		SInt32 score;
		if(IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score) != 0) {
			IOObjectRelease(usbInterface);
			continue;
		}
		
		if((*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID)&interfaceHandle) != 0) {
			(*iodev)->Release(iodev);
			IOObjectRelease(usbInterface);
			continue;
		}
		(*iodev)->Release(iodev);
		
		if((*interfaceHandle)->USBInterfaceOpen(interfaceHandle) != 0) {
			(*interfaceHandle)->Release(interfaceHandle);
			IOObjectRelease(usbInterface);
			continue;
		}
		
		UInt8 pipes;
		(*interfaceHandle)->SetAlternateInterface(interfaceHandle, 1);
		(*interfaceHandle)->GetNumEndpoints(interfaceHandle, &pipes);
		
		for(UInt8 i=0;i<=pipes;++i) {
			UInt8 ind = i;
			UInt8 direction, number, transferType, interval;
			UInt16 maxPacketSize;
			
			(*interfaceHandle)->GetPipeProperties(interfaceHandle, ind, &direction, &number, &transferType, &maxPacketSize, &interval);
			if(transferType == kUSBBulk && direction == kUSBIn) {
				found_interface = i;
				IOObjectRelease(usbInterface);
				break;
			}
		}
		
		IOObjectRelease(usbInterface);
	}
	IOObjectRelease(iterator);
	
	CFMutableDictionaryRef properties;
	IORegistryEntryCreateCFProperties(device->usbService, &properties, kCFAllocatorDefault, 0);
	
	device->deviceHandle = deviceHandle;
	device->interfaceHandle = interfaceHandle;
	device->open = 1;
	device->properties = (CFDictionaryRef)properties;
	device->responsePipeRef = found_interface;
	device->disconnectNPort = IONotificationPortCreate(kIOMasterPortDefault);
	
	io_iterator_t detachedIterator;
	IOServiceAddMatchingNotification(device->disconnectNPort, kIOTerminatedNotification, matching, deviceDisconnected, device, &detachedIterator);
	deviceDisconnected(device, detachedIterator);
	
	return 1;
}

HIDDEN void deviceDisconnected(void *refCon, io_iterator_t iterator) {
	iUSBRecoveryDeviceRef device = refCon;
	io_service_t service;
	while(service = IOIteratorNext(iterator)) {
		IOObjectRelease(service);
		if(device != NULL) {
			if(device->disconnectCallback != NULL) {
				IOObjectRelease(iterator);
				device->disconnectCallback(device, kUSBDisconnected);
			}
		}
	}
}