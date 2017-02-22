/*
Class controlling lower-level communication with Helios Laser DACs.
By Gitle Mikkelsen, Creative Commons Attribution-NonCommercial 4.0 International Public License

See HeliosDacAPI.h instead for top level functions

Dependencies: Libusb 1.0 (GNU Lesser General Public License, see libusb.h)
*/

#include "HeliosDac.h"

#ifdef __linux__
	#include <memory.h>
#endif

HeliosDac::HeliosDac()
{
	inited = false;
}

HeliosDac::~HeliosDac()
{
	CloseDevices();
}


//attempts to open connection and initialize dacs
//returns number of connected devices
int HeliosDac::OpenDevices()
{
	CloseDevices();

	for (int i = 0; i < HELIOS_MAX_DEVICES; i++)
		deviceList[i] = NULL;

	int result = libusb_init(NULL);
	if (result < 0)
		return result;

	libusb_device** devs;
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int)cnt;

	inited = true;

	int devNum = 0;
	for (int i = 0; ((i < cnt) && (devNum < HELIOS_MAX_DEVICES)); i++)
	{
		struct libusb_device_descriptor devDesc;
		int result = libusb_get_device_descriptor(devs[i], &devDesc);
		if (result < 0)
			continue;

		if ((devDesc.idProduct != HELIOS_PID) || (devDesc.idVendor != HELIOS_VID))
			continue;

		libusb_device_handle* devHandle;
		result = libusb_open(devs[i], &devHandle);
		if (result < 0)
			continue;

		result = libusb_claim_interface(devHandle, 0);
		if (result < 0)
			continue;

		result = libusb_set_interface_alt_setting(devHandle, 0, 1);
		if (result < 0)
			continue;

		//successfully opened, add to device list
		status[devNum] = true;
		deviceList[devNum] = devHandle;

		std::thread statusHandlerThread(&HeliosDac::InterruptTransferHandler, this, devNum);
		statusHandlerThread.detach();

		devNum++;
	}

	libusb_free_device_list(devs, 1);

	numOfDevices = devNum;

	for (int u = 0; u < 10000000; u++);

	//todo fix
	//if (numOfDevices > 0)
	//{
	//	//This will time out. Used as delay because the DAC won't accept frames for about 100ms after initialization
	//	uint8_t buf;
	//	int len;
	//	libusb_bulk_transfer(deviceList[0], EP_INT_IN, &buf, 1, &len, 150);
	//}

	return devNum;
}

//sends a raw frame buffer (implemented as bulk transfer) to a dac device
//returns 1 if transfer succeeds
int HeliosDac::SendFrame(int devNum, uint8_t* bufferAddress, int bufferSize)
{
	if ((bufferAddress == NULL) || (!inited) || (devNum > numOfDevices) || (bufferSize > HELIOS_MAX_POINTS * 7 + 5))
		return 0;

	int actualLength = 0;

	int transferResult = libusb_bulk_transfer(deviceList[devNum], EP_BULK_OUT, bufferAddress, bufferSize, &actualLength, 500);

	if ((transferResult == 0) && (actualLength == bufferSize))
	{
		status[devNum] = false;
		return 1;
	}
	else
		return 0;
}

//Gets status of DAC, true means DAC is ready to receive frame
bool HeliosDac::GetStatus(int devNum)
{
	if (!inited)
		return false;

	return status[devNum];
}

//sends a raw control signal (implemented as interrupt transfer) to a dac device
//returns 1 if successful
int HeliosDac::SendControl(int devNum, uint8_t* bufferAddress, int length)
{
	if ((bufferAddress == NULL) || (!inited) || (devNum >= numOfDevices) || (length > 32) || (length <= 0))
		return 0;

	int actualLength = 0;
	int transferResult = libusb_interrupt_transfer(deviceList[devNum], EP_INT_OUT, bufferAddress, length, &actualLength, 32);

	if ((transferResult == 0) && (actualLength == length))
		return 1;
	else
		return transferResult;
}

//Attempts to receive a response to a previous control transfer.
//Returns length of packet >0 , and populates bufferAddress on success
int HeliosDac::GetControlResponse(int devNum, uint8_t* bufferAddress, int length)
{
	if ((bufferAddress == NULL) || (!inited) || (devNum >= numOfDevices) || (length > 32) || (length <= 0))
		return 0;

	uint8_t data[32];
	int actualLength = 0;
	int transferResult = libusb_bulk_transfer(deviceList[devNum], EP_BULK_IN, &data[0], length, &actualLength, 32);

	if (transferResult < 0)
	{
		return transferResult;
	}
	else
	{
		memcpy(bufferAddress, &data[0], length);
		return 1;
	}
}

//closes and frees all devices
//returns true if successful
int HeliosDac::CloseDevices()
{
	if (!inited)
		return 0;

	for (int i = 0; i < numOfDevices; i++)
	{
		libusb_close(deviceList[i]);
	}

	libusb_exit(NULL);
	inited = false;
	numOfDevices = 0;

	return 1;
}

//buffers interrupt usb transfers (status)
void HeliosDac::InterruptTransferHandler(int devNum)
{
	while (inited)
	{
		uint8_t data[2];
		int actualLength = 0;
		int transferResult = libusb_interrupt_transfer(deviceList[devNum], EP_INT_IN, &data[0], 2, &actualLength, 0);

		if (transferResult == 0) //if transfer valid
		{
			if ((data[0]) == 0x83) //status transfer code
			{
				//todo mutex
				status[devNum] = (data[1] == 1);
				//MessageBoxW(NULL, L"status", L"cap", MB_OK);
				printf("X");
			}
		}
	}
}