# Mbed_Azure

## 1. Make Certification with Azure toolkit

1-1. git clone https://github.com/Azure/azure-iot-sdk-c.git
"azure-iot-sdk-c/tools/CACeritificates" is provide tool for make test Certification 

1-2. https://github.com/Azure/azure-iot-sdk-c/blob/master/tools/CACertificates/CACertificateOverview.md
follow this document.

1-3. upload certificates on azure iot hub and verification

1-4. make device as x509.CA signed

## 2. modify mbed project

2-1. mbed-app.json modify to your wifi credential

2-2. **MQTT_server_setting.h modify**
Device_ID => IOT hub's device name
MQTT_SERVER_HOST_NAME => IOT HUB NAME (<Iot hub name>.azure-devices.net)
SSL_CLIENT_CERT_PEM: "new-device-full-chain.cert.pem"
 SSL_CLIENT_PRIVATE_KEY_PEM: "new-device.key.pem"
  
## 3. Let's compile

mbed compile -m Ublox_evk_odin_w2 -t GCC_ARM

Let's use device explorer OR IoT Hub Explorer for check
