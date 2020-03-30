#include "LedDeviceFile.h"

#include <chrono>
#include <iomanip>
#include <iostream>

LedDeviceFile::LedDeviceFile(const QJsonObject &deviceConfig)
	: LedDevice()
	, _ofs (nullptr)
{
	_devConfig = deviceConfig;
	_deviceReady = false;
	_printTimeStamp = false;
}

LedDeviceFile::~LedDeviceFile()
{
}

LedDevice* LedDeviceFile::construct(const QJsonObject &deviceConfig)
{
	return new LedDeviceFile(deviceConfig);
}

bool LedDeviceFile::init(const QJsonObject &deviceConfig)
{
	Debug(_log, "LedDeviceFile::init()");
	bool initOK = LedDevice::init(deviceConfig);

	_fileName = deviceConfig["output"].toString("/dev/null");
	_printTimeStamp = deviceConfig["printTimeStamp"].toBool(false);

	Debug(_log, "LedDeviceFile::init() [%d]", initOK);
	return initOK;
}

int LedDeviceFile::open()
{
	Debug(_log, "LedDeviceFile::open()");
	int retval = -1;
	QString errortext;
	_deviceReady = false;

	// open device physically
	_ofs.open( QSTRING_CSTR(_fileName));
	if ( _ofs.fail() )
	{
		errortext = QString ("Failed to open file (%1). Error message: %2").arg(_fileName, strerror(errno));
	}
	else
	{
		_deviceReady = true;
		retval = 0;
	}

	if ( retval < 0 )
	{
		this->setInError( errortext );
	}

	Debug(_log, "LedDeviceFile::open() [%d]", retval);
	return retval;
}

int LedDeviceFile::close()
{
	Debug(_log, "LedDeviceFile::close()");
	int retval = 0;

	_deviceReady = false;
	// Test, if device requies closing
	if ( _ofs )
	{
		// close device physically
		_ofs.close();
		if ( _ofs.fail() )
		{
			Error( _log, "Failed to close device (%s). Error message: %s", QSTRING_CSTR(_fileName), strerror(errno) );
		}
	}
	Debug(_log, "LedDeviceFile::close() [%d]", retval);
	return retval;
}

int LedDeviceFile::write(const std::vector<ColorRgb> & ledValues)
{
	//printLedValues (ledValues);
	if ( _printTimeStamp )
	{
		// get a precise timestamp as a string
		const auto now = std::chrono::system_clock::now();
		const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
		const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
							   now.time_since_epoch()) % 1000;

		const auto elapsedTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWriteTime);

		_ofs
			<< std::put_time(std::localtime(&nowAsTimeT), "%Y-%m-%d %T")
			<< '.' << std::setfill('0') << std::setw(3) << nowMs.count()
			<< " | +" << std::setfill('0') << std::setw(4) << elapsedTimeMs.count();

		lastWriteTime = now;
	}

	_ofs << " [";
	for (const ColorRgb& color : ledValues)
	{
		_ofs << color;
	}
	_ofs << "]" << std::endl;

	return 0;
}
