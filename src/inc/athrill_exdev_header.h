#ifndef _ATHRILL_EXDEV_HEADER_H_
#define _ATHRILL_EXDEV_HEADER_H_

#define ATHRILL_EXTERNAL_DEVICE_MAGICNO		0xBEAFDEAD
#define ATHRILL_EXTERNAL_DEVICE_VERSION		0x00000002
typedef struct {
	unsigned int magicno; /* ATHRILL_EXTERNAL_DEVICE_MAGICNO */
	unsigned int version; /* ATHRILL_EXTERNAL_DEVICE_VERSION */
	int memory_size; /* Bytes */
} AthrillExDeviceHeaderType;


#endif /* _ATHRILL_EXDEV_HEADER_H_ */
