#ifndef _ATHRILL_EXDEV_HEADER_H_
#define _ATHRILL_EXDEV_HEADER_H_

#if defined(_WIN32)
#define ATHRILL_EXDEV_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define ATHRILL_EXDEV_EXPORT __attribute__((visibility("default")))
#else
#define ATHRILL_EXDEV_EXPORT
#endif

#define ATHRILL_EXTERNAL_DEVICE_MAGICNO		0xBEEFDEAD
#define ATHRILL_EXTERNAL_DEVICE_VERSION		0x00000004
typedef struct {
	unsigned int magicno; /* ATHRILL_EXTERNAL_DEVICE_MAGICNO */
	unsigned int version; /* ATHRILL_EXTERNAL_DEVICE_VERSION */
	int memory_size; /* Bytes */
} AthrillExDeviceHeaderType;


#endif /* _ATHRILL_EXDEV_HEADER_H_ */
