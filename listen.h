/*
 *  listen.h
 *  iusbcomm
 *
 *  Created by John Heaton on 5/16/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */

#ifndef IUSBCOMM_LISTEN_H
#define IUSBCOMM_LISTEN_H

#include "recovery.h"

typedef struct __iUSBListener *iUSBListenerRef;

typedef enum {
	kUSBListenerTypeNormal = 0x2,
	kUSBListenerTypeRecovery = 0x4
} iUSBListenerType;

iUSBListenerRef iUSBListenerCreate(iUSBListenerType listenModes, iUSBRecoveryDeviceConnectionChangeCallback recoveryCallback);
Boolean iUSBListenerStartListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop, CFStringRef runLoopMode);
void iUSBListenerStopListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop, CFStringRef runLoopMode);
void iUSBListenerRelease(iUSBListenerRef listener);

#endif /* IUSBCOMM_LISTEN_H */