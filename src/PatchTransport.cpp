#include "PatchTransport.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QTcpSocket>

#ifdef DSP_TUNER_HOST_HAS_SERIAL
#include <QSerialPort>
#endif

namespace PatchTransport {

bool sendTcp(const QString &host, quint16 port, const QByteArray &jsonPayload, QString *errorMessage) {
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(5000)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        return false;
    }
    const qint64 n = socket.write(jsonPayload);
    if (n < 0) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        return false;
    }
    socket.write("\n");
    if (!socket.waitForBytesWritten(5000)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        return false;
    }
    socket.disconnectFromHost();
    if (socket.state() != QAbstractSocket::UnconnectedState) {
        socket.waitForDisconnected(2000);
    }
    return true;
}

bool sendSerial(const QString &portName, int baudRate, const QByteArray &jsonPayload, QString *errorMessage) {
#ifndef DSP_TUNER_HOST_HAS_SERIAL
    Q_UNUSED(portName);
    Q_UNUSED(baudRate);
    Q_UNUSED(jsonPayload);
    if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Serial port support was not built in. Install qt6-serialport-dev (Qt 6) or libqt5serialport5-dev (Qt 5), "
            "then re-run CMake and rebuild.");
    }
    return false;
#else
    QSerialPort port;
    port.setPortName(portName);
    port.setBaudRate(baudRate);
    if (!port.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = port.errorString();
        }
        return false;
    }
    const qint64 n = port.write(jsonPayload);
    if (n < 0) {
        if (errorMessage) {
            *errorMessage = port.errorString();
        }
        port.close();
        return false;
    }
    port.write("\n");
    if (!port.waitForBytesWritten(5000)) {
        if (errorMessage) {
            *errorMessage = port.errorString();
        }
        port.close();
        return false;
    }
    port.close();
    return true;
#endif
}

bool serialSupported() {
#ifdef DSP_TUNER_HOST_HAS_SERIAL
    return true;
#else
    return false;
#endif
}

} // namespace PatchTransport
