#include <leddevice/LedDevice.h>
#include <sstream>
#include <unistd.h>

//QT include
#include <QResource>
#include <QStringList>
#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QTimer>

#include "hyperion/Hyperion.h"
#include <utils/JsonUtils.h>

LedDevice::LedDevice(const QJsonObject& config, QObject* parent)
	: QObject(parent)
	, _devConfig(config)
	, _log(Logger::getInstance("LEDDEVICE"))
	, _ledBuffer(0)
	, _deviceReady(false)
	, _deviceInError(false)
	, _refresh_timer(new QTimer(this))
	, _refresh_timer_interval(0)
	, _last_write_time(QDateTime::currentMSecsSinceEpoch())
	, _latchTime_ms(0)
	, _enabled(false)
	, _componentRegistered(false)
	, _refresh_enabled (false)
{
	// setup refreshTimer
	_refresh_timer->setTimerType(Qt::PreciseTimer);
	_refresh_timer->setInterval( _refresh_timer_interval );
	connect(_refresh_timer, SIGNAL(timeout()), this, SLOT(rewriteLeds()));
}

LedDevice::~LedDevice()
{
	_refresh_timer->deleteLater();
}

void LedDevice::start()
{
	Debug(_log, "");
	close();
	// General initialisation and configuration of LedDevice
	if ( init(_devConfig) )
	{
		// Everything is OK -> enable device
		//_deviceReady = true;
		setEnable(true);
	}
	Debug(_log, "[void]");
}

void LedDevice::stop()
{
	Debug(_log, "");
	setEnable(false);
	this->stopRefreshTimer();
	Debug(_log, "[void]");
}

int LedDevice::open()
{
	Debug(_log, "");

	_deviceReady = true;
	int retval = 0;

	Debug(_log, "[%d]", retval);
	return retval;
}

int LedDevice::close()
{
	Debug(_log, "");

	_deviceReady = false;
	int retval = 0;

	Debug(_log, "[%d]", retval);
	return retval;
}

void LedDevice::setInError(const QString& errorMsg)
{
	_deviceInError = true;
	_deviceReady = false;
	_enabled = false;
	this->stopRefreshTimer();

	Error(_log, "Device disabled, device '%s' signals error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(errorMsg));
	emit enableStateChanged(_enabled);
}



void LedDevice::setEnable(bool enable)
{
	Debug(_log, "enable [%d], enabled [%d], _deviceReady [%d]", enable, this->enabled(), _deviceReady);

	bool isSwitched = false;
	// switch off device when disabled, default: set black to leds when they should go off
	if ( _enabled  && !enable)
	{
		isSwitched = switchOff();
	}
	else
	{
		// switch on device when enabled
		if ( !_enabled  && enable)
		{
			isSwitched = switchOn();
		}
	}

	if ( isSwitched )
	{
		_enabled = enable;
		emit enableStateChanged(enable);
	}

	Debug(_log, "isSwitched [%d],_enabled [%d], _deviceReady [%d], _deviceInError[%d]", isSwitched, enabled(), _deviceReady, _deviceInError);
}

void LedDevice::setActiveDeviceType(const QString& deviceType)
{
	_activeDeviceType = deviceType;
}

bool LedDevice::init(const QJsonObject &deviceConfig)
{
	Debug(_log, "deviceConfig: [%s]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData() );

	_colorOrder = deviceConfig["colorOrder"].toString("RGB");
	_activeDeviceType = deviceConfig["type"].toString("file").toLower();
	setLedCount(static_cast<unsigned int>( deviceConfig["currentLedCount"].toInt(1) )); // property injected to reflect real led count

	_latchTime_ms =deviceConfig["latchTime"].toInt( _latchTime_ms );
	_refresh_timer_interval =  deviceConfig["rewriteTime"].toInt( _refresh_timer_interval);

	if ( _refresh_timer_interval > 0 )
	{
		_refresh_enabled = true;

		if (_refresh_timer_interval <= _latchTime_ms )
		{
			int new_refresh_timer_interval = _latchTime_ms + 10;
			Warning(_log, "latchTime(%d) is bigger/equal rewriteTime(%d), set rewriteTime to %dms", _latchTime_ms, _refresh_timer_interval, new_refresh_timer_interval);
			_refresh_timer_interval = new_refresh_timer_interval;
			_refresh_timer->setInterval( _refresh_timer_interval );
		}

		Debug(_log, "Refresh interval = %dms",_refresh_timer_interval );
		_refresh_timer->setInterval( _refresh_timer_interval );

		_last_write_time = QDateTime::currentMSecsSinceEpoch();

		this->startRefreshTimer();
	}
	return true;
}

void LedDevice::startRefreshTimer()
{
	//std::cout << "LedDevice::startRefreshTimer(), _deviceReady [" << _deviceReady << "], _enabled [" << _enabled << "]" << std::endl;
	if ( _deviceReady && _enabled )
	{
		_refresh_timer->start();
	}
}

void LedDevice::stopRefreshTimer()
{
	//std::cout << "LedDevice::stopRefreshTimer(), _deviceReady [" << _deviceReady << "], _enabled [" << _enabled << "]" << std::endl;
	_refresh_timer->stop();
}

int LedDevice::updateLeds(const std::vector<ColorRgb>& ledValues)
{
	//std::cout << "LedDevice::updateLeds(), _enabled [" << _enabled << "], _deviceReady [" << _deviceReady << "], _deviceInError [" << _deviceInError << "]" << std::endl;
	int retval = 0;
	if (  !enabled() || !_deviceReady || _deviceInError)
	{
		//std::cout << "LedDevice::updateLeds(), LedDevice NOT ready!" <<  std::endl;
		return -1;
	}
	else
	{
		qint64 elapsedTime = QDateTime::currentMSecsSinceEpoch() - _last_write_time;
		if (_latchTime_ms == 0 || elapsedTime >= _latchTime_ms)
		{
			//std::cout << "LedDevice::updateLeds(), Elapsed time since last write (" << elapsedTime << ") ms > _latchTime_ms (" << _latchTime_ms << ") ms" << std::endl;
			retval = write(ledValues);
			_last_write_time = QDateTime::currentMSecsSinceEpoch();

			// if device requires refreshing, save Led-Values and restart the timer
			if ( _refresh_enabled && _enabled)
			{
				this->startRefreshTimer();
				_last_ledValues = ledValues;
			}
		}
		else
		{
			//std::cout << "LedDevice::updateLeds(), Skip write. elapsedTime (" << elapsedTime << ") ms < _latchTime_ms (" << _latchTime_ms << ") ms" << std::endl;
			if ( _refresh_enabled )
			{
				//Stop timer to allow for next non-refresh update
				this->stopRefreshTimer();
			}
		}
	}
	return retval;
}

int LedDevice::writeBlack()
{
	Debug(_log, "enabled [%d], _deviceReady [%d]", this->enabled(), _deviceReady);
	int rc = -1;

	if ( _latchTime_ms > 0 )
	{
		// Wait latchtime before writing black
		QEventLoop loop;
		QTimer::singleShot( _latchTime_ms, &loop, SLOT( quit() ) );
		loop.exec();
	}

	usleep(20 * 1000);
	rc = write(std::vector<ColorRgb>(static_cast<unsigned long>(_ledCount), ColorRgb::BLACK ));

	Debug(_log, "[%d]", rc);
	return rc;
}

bool LedDevice::switchOn()
{
	Debug(_log, "enabled [%d], _deviceReady [%d]", this->enabled(), _deviceReady);
	bool rc = true;
	if ( ! _deviceReady && ! this->enabled() )
	{
		_deviceInError = false;
		if ( open() < 0 )
		{
			rc = false;
		}
		else
		{
			_enabled = true;
		}
	}

	Debug(_log, "[%d], enabled [%d], _deviceReady [%d]", rc, this->enabled(), _deviceReady);
	return rc;
}

bool LedDevice::switchOff()
{
	Debug(_log, "");
	// Disbale device to ensure no updates are wriiten processed
	_enabled = false;

	this->stopRefreshTimer();

	// Write a final "Black" to have a defined outcome
	writeBlack();

	bool rc = true;
	if ( close() < 0 )
	{
		rc = false;
	}
	Debug(_log, "[%d]", rc);
	return rc;
}

void LedDevice::setLedCount(unsigned int ledCount)
{
	_ledCount     = ledCount;
	_ledRGBCount  = _ledCount * sizeof(ColorRgb);
	_ledRGBWCount = _ledCount * sizeof(ColorRgbw);
}

void LedDevice::setLatchTime( int latchTime_ms )
{
	_latchTime_ms = latchTime_ms;
	Debug(_log, "LatchTime updated to %dms", this->getLatchTime());
}

int LedDevice::rewriteLeds()
{
	int retval = -1;

	if ( _deviceReady && _enabled )
	{
//		qint64 elapsedTime = QDateTime::currentMSecsSinceEpoch() - _last_write_time;
//		std::cout << "LedDevice::rewriteLeds(): Rewrite Leds now, elapsedTime [" << elapsedTime << "] ms" << std::endl;
//		//:TESTING: Inject "white" output records to differentiate from normal writes
//		_last_ledValues.clear();
//		_last_ledValues.resize(static_cast<unsigned long>(_ledCount), ColorRgb::WHITE);
//		printLedValues(_last_ledValues);
//		//:TESTING:

		retval = write(_last_ledValues);
		_last_write_time = QDateTime::currentMSecsSinceEpoch();
	}
	else
	{
		// If Device is not ready stop timer
		this->stopRefreshTimer();
	}
	return retval;
}

void LedDevice::printLedValues(const std::vector<ColorRgb>& ledValues )
{
	std::cout << "LedValues [" << ledValues.size() <<"] [";
	for (const ColorRgb& color : ledValues)
	{
		std::cout << color;
	}
	std::cout << "]" << std::endl;
}
