// Local Hyperion includes
#include "ProviderRs232.h"

// Qt includes
#include <QSerialPortInfo>
#include <QEventLoop>

// Constants
const int WRITE_TIMEOUT	= 1000;		// device write timout in ms
const int OPEN_TIMEOUT	= 5000;		// device open timout in ms
const int MAX_WRITE_TIMEOUTS = 5;	// maximum number of allowed timeouts

ProviderRs232::ProviderRs232()
	: _rs232Port(this)
	  , _frameDropCounter(0)
	  , _enableAutoDeviceName(false)
{
}

bool ProviderRs232::init(const QJsonObject &deviceConfig)
{
	Debug(_log, "");

	bool isInitOK = LedDevice::init(deviceConfig);

	Debug(_log, "DeviceType   : %s", QSTRING_CSTR( this->getActiveDeviceType() ));
	Debug(_log, "LedCount     : %u", this->getLedCount());
	Debug(_log, "ColorOrder   : %s", QSTRING_CSTR( this->getColorOrder() ));
	Debug(_log, "RefreshTime  : %d", _refresh_timer_interval);
	Debug(_log, "LatchTime    : %d", this->getLatchTime());

	_deviceName           = deviceConfig["output"].toString("auto");
	_enableAutoDeviceName = _deviceName == "auto";
	_baudRate_Hz          = deviceConfig["rate"].toInt();
	_delayAfterConnect_ms = deviceConfig["delayAfterConnect"].toInt(1500);

	Debug(_log, "deviceName   : %s", QSTRING_CSTR(_deviceName));
	Debug(_log, "AutoDevice   : %d", _enableAutoDeviceName);
	Debug(_log, "baudRate_Hz  : %d", _baudRate_Hz);
	Debug(_log, "delayAfCon ms: %d", _delayAfterConnect_ms);

	Debug(_log, "[%d]", isInitOK);
	return isInitOK;
}

ProviderRs232::~ProviderRs232()
{
}

int ProviderRs232::open()
{
	Debug(_log, "");

	int retval = -1;
	_deviceReady = false;

	// open device physically
	if ( tryOpen(_delayAfterConnect_ms) )
	{
		// Everything is OK -> enable device
		_deviceReady = true;
		retval = 0;
	}

	Debug(_log, "[%d]", retval);
	return retval;
}

int ProviderRs232::close()
{
	Debug(_log, "");

	int retval = 0;

	_deviceReady = false;

	// Test, if device requies closing
	if (_rs232Port.isOpen())
	{
		Debug(_log,"Close UART: %s", QSTRING_CSTR(_deviceName) );
		_rs232Port.close();
		// Everything is OK -> device is closed
	}

	Debug(_log, "[%d]", retval);
	return retval;
}

void ProviderRs232::stop()
{
	Debug(_log, "");
	LedDevice::stop();
	Debug(_log, "[void]");
}

QString ProviderRs232::findSerialDevice()
{
	Debug(_log, "");

	// take first available usb serial port - currently no probing!
	for( auto port : QSerialPortInfo::availablePorts())
	{
		if (port.hasProductIdentifier() && port.hasVendorIdentifier() && !port.isBusy())
		{
			Info(_log, "found serial device: %s", QSTRING_CSTR(port.systemLocation()) );
			return port.systemLocation();
		}
	}
	return "";
}

bool ProviderRs232::tryOpen(const int delayAfterConnect_ms)
{
	Debug(_log, "");

	if (_deviceName.isEmpty() || _rs232Port.portName().isEmpty())
	{
		if ( _enableAutoDeviceName )
		{
			_deviceName = findSerialDevice();
			if ( _deviceName.isEmpty() )
			{
				this->setInError( QString ("No serial device found automatically!") );
				return false;
			}
		}
		Info(_log, "Opening UART: %s", QSTRING_CSTR(_deviceName));
		_rs232Port.setPortName(_deviceName);
	}

	if ( ! _rs232Port.isOpen() )
	{
		Debug(_log, "! _rs232Port.isOpen(): %s", QSTRING_CSTR(_deviceName) );
		_frameDropCounter = 0;


		_rs232Port.setBaudRate( _baudRate_Hz );

		Debug(_log, "_rs232Port.open(QIODevice::WriteOnly): %s", QSTRING_CSTR(_deviceName));
		if ( ! _rs232Port.open(QIODevice::WriteOnly) )
		{
			this->setInError( _rs232Port.errorString() );
			return false;
		}
	}

	if (delayAfterConnect_ms > 0)
	{

		Debug(_log, "delayAfterConnect for %d ms - start", delayAfterConnect_ms);

		// Wait delayAfterConnect_ms before allowing write
		QEventLoop loop;
		QTimer::singleShot( delayAfterConnect_ms, &loop, SLOT( quit() ) );
		loop.exec();

		Debug(_log, "delayAfterConnect for %d ms - finished", delayAfterConnect_ms);
	}

	return _rs232Port.isOpen();
}

void ProviderRs232::setInError(const QString& errorMsg)
{
	_rs232Port.clearError();
	this->close();

	LedDevice::setInError( errorMsg );
}

int ProviderRs232::writeBytes(const qint64 size, const uint8_t * data)
{
	Debug(_log, "enabled [%d], _deviceReady [%d], _frameDropCounter [%d]", this->enabled(), _deviceReady, _frameDropCounter);

	int rc = 0;
	if (!_rs232Port.isOpen())
	{
		Debug(_log, "!_rs232Port.isOpen()");

		if ( !tryOpen(OPEN_TIMEOUT) )
		{
			return -1;
		}
	}

	qint64 bytesWritten = _rs232Port.write(reinterpret_cast<const char*>(data), size);
	if (bytesWritten == -1 || bytesWritten != size)
	{
		this->setInError( QString ("Rs232 SerialPortError: %1").arg(_rs232Port.errorString()) );
		rc = -1;
	}
	else
	{
		if (!_rs232Port.waitForBytesWritten(WRITE_TIMEOUT))
		{
			if ( _rs232Port.error() == QSerialPort::TimeoutError )
			{
				Debug(_log, "Timeout after %dms: %d frames already dropped", WRITE_TIMEOUT, _frameDropCounter);

				++_frameDropCounter;

				// Check,if number of timeouts in a given timeframe is greater than defined
				// TODO: Add timeframe
				if ( _frameDropCounter > MAX_WRITE_TIMEOUTS )
				{
					this->setInError( QString ("Timeout writing data to %1").arg(_deviceName) );
					rc = -1;
				}
				else
				{
					//give it another try
					_rs232Port.clearError();
				}
			}
			else
			{
				this->setInError( QString ("Rs232 SerialPortError: %1").arg(_rs232Port.errorString()) );
				rc = -1;
			}
		}
		else
		{
			Debug(_log, "bytesWritten [%d], _rs232Port.error() [%d], %s", bytesWritten, _rs232Port.error(), _rs232Port.error() == QSerialPort::NoError ? "No Error" : QSTRING_CSTR(_rs232Port.errorString()) );
		}
	}

	Debug(_log, "[%d], enabled [%d], _deviceReady [%d]", rc, this->enabled(), _deviceReady);
	return rc;
}
