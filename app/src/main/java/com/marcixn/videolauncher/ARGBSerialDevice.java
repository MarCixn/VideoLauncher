package com.marcixn.videolauncher;

import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;

import java.io.IOException;

/**
 * Simple USB CDC Serial Device for STM32/CH340 communication
 */
public class ARGBSerialDevice {

    private UsbDevice device;
    private UsbDeviceConnection connection;
    private UsbInterface usbInterface;
    private UsbEndpoint endpointIn;
    private UsbEndpoint endpointOut;

    private static final int TIMEOUT = 3000;

    public ARGBSerialDevice(UsbDevice device, UsbDeviceConnection connection) {
        this.device = device;
        this.connection = connection;
    }

    public void open() throws IOException {
        // Find CDC interface and endpoints
        for (int i = 0; i < device.getInterfaceCount(); i++) {
            UsbInterface iface = device.getInterface(i);

            // Look for CDC Data interface or vendor specific
            if (iface.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA ||
                iface.getInterfaceClass() == UsbConstants.USB_CLASS_VENDOR_SPEC ||
                iface.getInterfaceClass() == 0xFF) {

                for (int j = 0; j < iface.getEndpointCount(); j++) {
                    UsbEndpoint ep = iface.getEndpoint(j);

                    if (ep.getType() == UsbConstants.USB_ENDPOINT_XFER_BULK) {
                        if (ep.getDirection() == UsbConstants.USB_DIR_IN) {
                            endpointIn = ep;
                        } else {
                            endpointOut = ep;
                        }
                    }
                }

                if (endpointIn != null && endpointOut != null) {
                    usbInterface = iface;
                    break;
                }
            }
        }

        // Try all interfaces if specific class not found
        if (usbInterface == null) {
            for (int i = 0; i < device.getInterfaceCount(); i++) {
                UsbInterface iface = device.getInterface(i);

                for (int j = 0; j < iface.getEndpointCount(); j++) {
                    UsbEndpoint ep = iface.getEndpoint(j);

                    if (ep.getType() == UsbConstants.USB_ENDPOINT_XFER_BULK) {
                        if (ep.getDirection() == UsbConstants.USB_DIR_IN && endpointIn == null) {
                            endpointIn = ep;
                            usbInterface = iface;
                        } else if (ep.getDirection() == UsbConstants.USB_DIR_OUT && endpointOut == null) {
                            endpointOut = ep;
                            usbInterface = iface;
                        }
                    }
                }
            }
        }

        if (usbInterface == null || endpointIn == null || endpointOut == null) {
            throw new IOException("Cannot find USB endpoints");
        }

        // Claim interface
        if (!connection.claimInterface(usbInterface, true)) {
            throw new IOException("Cannot claim interface");
        }

        // Set baud rate for CH340 or STM32 CDC
        int vendorId = device.getVendorId();
        if (vendorId == 0x1A86) { // CH340
            setCH340BaudRate(115200);
        } else if (vendorId == 0x0483 || vendorId == 0x1EAF) { // STM32 CDC lub Maple
            setCDCBaudRate(115200);
        }
    }

    private void setCH340BaudRate(int baudRate) {
        // CH340 baud rate setting
        int factor = 1532620800 / baudRate;
        int divisor = 3;

        while ((factor > 0xfff0) && (divisor > 0)) {
            factor >>= 3;
            divisor--;
        }

        factor = 0x10000 - factor;

        // CH340 control transfer
        connection.controlTransfer(
            0x40, // Request type
            0x9A, // Request
            0x1312, // Value
            (factor & 0xFF00) | divisor, // Index
            null, 0, TIMEOUT
        );

        connection.controlTransfer(
            0x40, 0x9A, 0x0F2C, factor & 0xFF, null, 0, TIMEOUT
        );
    }

    private void setCDCBaudRate(int baudRate) {
        // CDC ACM Set Line Coding
        byte[] lineCoding = new byte[7];
        lineCoding[0] = (byte) (baudRate & 0xFF);
        lineCoding[1] = (byte) ((baudRate >> 8) & 0xFF);
        lineCoding[2] = (byte) ((baudRate >> 16) & 0xFF);
        lineCoding[3] = (byte) ((baudRate >> 24) & 0xFF);
        lineCoding[4] = 0; // Stop bits: 1
        lineCoding[5] = 0; // Parity: None
        lineCoding[6] = 8; // Data bits: 8

        connection.controlTransfer(
            0x21, // Request type (class, interface, host to device)
            0x20, // SET_LINE_CODING
            0,
            0,
            lineCoding,
            lineCoding.length,
            TIMEOUT
        );

        // Set Control Line State (DTR, RTS)
        connection.controlTransfer(
            0x21, 0x22, 0x03, 0, null, 0, TIMEOUT
        );
    }

    public void write(byte[] data) throws IOException {
        if (connection == null || endpointOut == null) {
            throw new IOException("Not connected");
        }

        int result = connection.bulkTransfer(endpointOut, data, data.length, TIMEOUT);
        if (result < 0) {
            throw new IOException("Write: " + result);
        }
    }

    public int read(byte[] buffer) throws IOException {
        if (connection == null || endpointIn == null) {
            throw new IOException("Device not connected");
        }

        return connection.bulkTransfer(endpointIn, buffer, buffer.length, TIMEOUT);
    }

    public void close() {
        if (connection != null && usbInterface != null) {
            connection.releaseInterface(usbInterface);
        }
    }
}
