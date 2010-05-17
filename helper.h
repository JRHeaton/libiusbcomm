/*
 *  helper.h
 *  iusbcomm
 *
 *  Created by John Heaton on 5/16/10.
 *  Copyright 2010 Gojohnnyboi. All rights reserved.
 *
 */

#ifndef IUSBCOMM_HELPER_H
#define IUSBCOMM_HELPER_H

#include <CoreFoundation/CoreFoundation.h>

enum iUSBRequest {
	kUSBRequestCommand = 0x40,
	kUSBRequestFile = 0x21,
	kUSBRequestStatus = 0xA1
};

CFNumberRef AppleIncVendorID();
CFNumberRef numberForUInt16(uint16_t value);

#endif /* IUSBCOMM_HELPER_H */