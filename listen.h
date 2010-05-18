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

/*!
 @enum iUSBListenerType
 @field kUSBListenerTypeNormal - Devices in normal mode
 @field kUSBListenerTypeRecovery - Devices in recovery/dfu mode
 */
typedef enum {
	kUSBListenerTypeNormal = 0x2,
	kUSBListenerTypeRecovery = 0x4
} iUSBListenerType;

/*!
 @function iUSBListenerCreate
 Create a new listener object that will deliver notifications of device connections to the specified callbacks, 
 for the specified modes.
 @param listenModes - kUSBListenerType value(s). ex: (kUSBListenerTypeNormal | kUSBListenerTypeRecovery)
 @param recoveryCallback - The callback for recovery device connections
 Note: Do *NOT* release a iUSBRecoveryDeviceRef object until you receive a detach notification. It will cause issues.
 @result A new listener object which the caller is responsible for releasing
 */
iUSBListenerRef iUSBListenerCreate(iUSBListenerType listenModes, iUSBRecoveryDeviceConnectionChangeCallback recoveryCallback);

/*!
 @function iUSBListenerStartListeningOnRunLoop
 Setup for listening and/or add the notification port to your run loop in the specified mode.
 @param listener - The listener object to add to your run loop
 @param runLoop - The run loop to add the notifications to. If NULL, CFRunLoopGetCurrent() will be assumed.
 @param runLoopMode - The mode of your run loop to add to. If NULL, kCFRunLoopDefaultMode will be assumed.
 */
Boolean iUSBListenerStartListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop, CFStringRef runLoopMode);

/*!
 @function iUSBListenerStopListeningOnRunLoop
 Remove the listener from a run loop in the specified mode
 @param listener - The listener to remove from the run loop.
 @param runLoop - The run loop to remove the listener from.
 @param runLoopMode - The mode of the run loop to remove the listener from.
 */
void iUSBListenerStopListeningOnRunLoop(iUSBListenerRef listener, CFRunLoopRef runLoop, CFStringRef runLoopMode);

/*!
 @function iUSBListenerRelease
 Safely clean up and deallocate a listener object
 @param listenr - The listener to deallocate
 */
void iUSBListenerRelease(iUSBListenerRef listener);

#endif /* IUSBCOMM_LISTEN_H */