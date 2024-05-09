#ifndef __SMI_VERSION_H__
#define __SMI_VERSION_H__

#include <linux/version.h>
#ifdef RHEL_MAJOR
#undef LINUX_VERSION_CODE
#if RHEL_MAJOR==7
#if RHEL_MINOR==6
#define LINUX_VERSION_CODE 0x041113//4.17.19
#elif RHEL_MINOR==7
#define LINUX_VERSION_CODE 0x05000A//5.0.10
#elif RHEL_MINOR==8
#define LINUX_VERSION_CODE 0x05000A//5.0.10
#elif RHEL_MINOR==9
#define LINUX_VERSION_CODE 0x05000A//5.0.10
#endif//RHEL_MAJOR==7

#endif//RHEL_MAJOR

#endif




#endif