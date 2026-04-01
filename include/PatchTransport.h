#pragma once

#include <QString>

class QByteArray;

namespace PatchTransport {

/** Send UTF-8 JSON payload over TCP (e.g. localhost service for PC audio stack). */
bool sendTcp(const QString &host, quint16 port, const QByteArray &jsonPayload, QString *errorMessage = nullptr);

/** Send UTF-8 JSON payload over a serial port (e.g. embedded MCU). Optional compile-time. */
bool sendSerial(const QString &portName, int baudRate, const QByteArray &jsonPayload, QString *errorMessage = nullptr);

bool serialSupported();

} // namespace PatchTransport
