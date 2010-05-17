/*
 *  helper.c
 *  iusbcomm
 *
 *  Created by John Heaton on 5/16/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */

#include "helper.h"

#include <IOKit/usb/USB.h>

CFNumberRef AppleIncVendorID() {
	uint16_t appleID = kIOUSBVendorIDAppleComputer;
	return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, (const void *)&appleID);
}

CFNumberRef numberForUInt16(uint16_t value) {
	return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, (const void *)&value);
}