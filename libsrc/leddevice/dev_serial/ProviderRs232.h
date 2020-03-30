#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QString>

// LedDevice includes
#include <leddevice/LedDevice.h>

///
/// The ProviderRs232 implements an abstract base-class for LedDevices using a RS232-device.
///
class ProviderRs232 : public LedDevice
{
	Q_OBJECT

public:
	///
	/// Constructs specific LedDevice
	///
	ProviderRs232();

	///
	/// Sets configuration
	///
	/// @param deviceConfig the json device config
	/// @return true if success
	virtual bool init(const QJsonObject &deviceConfig) override;

	///
	/// Destructor of the LedDevice; closes the output device if it is open
	///
	virtual ~ProviderRs232() override;

//	/// Switch the device on
//	virtual int switchOn() override;

//	/// Switch the device off
//	virtual int switchOff() override;

public slots:

	///
	/// Stops the output device.
	/// Includes switching-off the device and stopping refreshes
	///
	virtual void stop() override;

protected:
	/**
	 * Writes the given bytes to the RS232-device and
	 *
	 * @param[in[ size The length of the data
	 * @param[in] data The data
	 *
	 * @return Zero on success else negative
	 */
	int writeBytes(const qint64 size, const uint8_t *data);

	///
	/// Opens the output device
	///
	/// @return Zero on succes (i.e. device is ready) else negative
	///
	virtual int open() override;

	///
	/// Closes the output device.
	///
	/// @return Zero on succes (i.e. device is closed) else negative
	///
	virtual int close() override;

	/// Set device in error state
	///
	/// @param errorMsg The error message to be logged
	///
	virtual void setInError( const QString& errorMsg) override;

	QString findSerialDevice();

	// tries to open device if not opened
	bool tryOpen(const int delayAfterConnect_ms);

	/// The name of the output device
	QString _deviceName;
	/// The RS232 serial-device
	QSerialPort _rs232Port;
	/// The used baudrate of the output device
	qint32 _baudRate_Hz;

	/// Sleep after the connect before continuing
	int _delayAfterConnect_ms;

	qint64 _frameDropCounter;
	bool _enableAutoDeviceName;
};
