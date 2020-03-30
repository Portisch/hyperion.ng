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

private slots:

	/// Unblock the device after a connection delay
	void writeTimeout();
	void unblockAfterDelay();
	void error(QSerialPort::SerialPortError setInError);
	void bytesWritten(qint64 bytes);
	void readyRead();

signals:
	void receivedData(QByteArray data);

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

	QString findSerialDevice();

	// tries to open device if not opened
	bool tryOpen(const int delayAfterConnect_ms);


	/// The name of the output device
	QString _deviceName;

	/// The used baudrate of the output device
	qint32 _baudRate_Hz;

	/// Sleep after the connect before continuing
	int _delayAfterConnect_ms;

	/// The RS232 serial-device
	QSerialPort _rs232Port;

	/// A timeout timer for the asynchronous connection
	QTimer _writeTimeout;

	bool _blockedForDelay;
	
	bool _stateChanged;

	qint64 _bytesToWrite;
	qint64 _frameDropCounter;
	QSerialPort::SerialPortError _lastError;
	qint64                       _preOpenDelayTimeOut;
	int                          _preOpenDelay;
	bool                         _enableAutoDeviceName;
};
