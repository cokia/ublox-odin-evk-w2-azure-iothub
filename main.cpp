#include <string>

#include "mbed.h"
#include "mbed_events.h"
#include "mbed-trace/mbed_trace.h"
#include "mbedtls/error.h"

#include "ntp-client/NTPClient.h"

#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTT_server_setting.h"


#define MQTT_MAX_CONNECTIONS    5
#define MQTT_MAX_PACKET_SIZE    1024

#define USE_WIFI        1


DigitalOut led1(LED1);
InterruptIn btn1(BUTTON1, PullUp);      // Must setting the Pullup option
NetworkInterface *net;
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;

MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>* mqttClient = NULL;
std::string mqtt_topic_pub;

#if USE_WIFI
WiFiInterface *wifi;

int Wifi_AP_connect()
{
    wifi = WiFiInterface::get_default_instance();
    if (!wifi)
    {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0)
    {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    printf("RSSI: %d\n\n", wifi->get_rssi());

    net = (NetworkInterface*)wifi;

    return 0;
}
#else
int ETH_connect()
{
    // Connect to the internet (DHCP is expected to be on)
    net = NetworkInterface::get_default_instance();

    nsapi_error_t status = net->connect();

    if (status != NSAPI_ERROR_OK)
    {
        printf("Connecting to the network failed %d!\r\n", status);
        return -1;
    }
    // Show the network address
    printf("Connected to the network successfully. IP address: %s\r\n", net->get_ip_address());

    return 0;
}
#endif

void Net_Disconnect()
{
    if(net)
        net->disconnect();
}


void Set_NTP()
{
    // sync the real time clock (RTC)
    NTPClient ntp(net);
    ntp.set_server("time.google.com", 123);
    time_t now = ntp.get_timestamp();
    set_time(now);
    printf("Time is now %s\r\n", ctime(&now));
}

void messageArrived(MQTT::MessageData& md)
{
    printf("sub topic\r\n");
    MQTT::Message &message = md.message;
    //    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", 
    //            message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
}

void btn_handler()
{
    led1 = 1;

#if 1
    static unsigned int id = 0;
    static unsigned int count = 0;
    const size_t len = 128;
    char buf[len];

    // When sending a message
    MQTT::Message message;
    message.retained = false;
    message.dup = false;
    snprintf(buf, len, "Message #%d from %s.", count, DEVICE_ID);
    message.payload = (void *)buf;
    message.qos = MQTT::QOS0;
    message.id = id++;
    message.payloadlen = strlen(buf);

    // Publish a message.
    printf("\r\nPublishing message to the topic %s:\r\n%s\r\n", mqtt_topic_pub.c_str(), buf);
    int rc = mqttClient->publish(mqtt_topic_pub.c_str(), message);
    if (rc != MQTT::SUCCESS)
    {
        printf("ERROR: rc from MQTT publish is %d\r\n", rc);
    }
    printf("Message published.\r\n");

    count++;
#else
    printf("btn\n");
#endif

    led1 = 0;
}

int main()
{
    int ret;
    //mbed_trace_init();

    printf("WiFi example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

#if USE_WIFI
    ret = Wifi_AP_connect();

#else
    ret = ETH_connect();
#endif

    if(ret != 0)
    {
        printf("Network connect failed!\n");
        return -1;
    }

    Set_NTP();

    /* Establish a network connection. */
    nsapi_error_t status;
    MQTTNetwork mqttNetwork(net);

    status = mqttNetwork.open();
    if (status != NSAPI_ERROR_OK)
    {
        printf("Mqtt open fail %d\r\n", status);
        Net_Disconnect();
        return -1;
    }

    printf("Connecting to host %s:%d ...\r\n", MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
    status = mqttNetwork.connect(MQTT_SERVER_HOST_NAME,
                                 MQTT_SERVER_PORT,
                                 SSL_CA_PEM,
                                 SSL_CLIENT_CERT_PEM,
                                 SSL_CLIENT_PRIVATE_KEY_PEM);
    if (status != NSAPI_ERROR_OK)
    {
        printf("Mqtt connect fail %d\r\n", status);
        mqttNetwork.close();
        Net_Disconnect();
        return -1;
    }
    printf("Connection established.\r\n\r\n");

    /* Establish a MQTT connection. */
    printf("MQTT client is connecting to the service ...\r\n");
    //MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS> mqttClient(mqttNetwork);
    mqttClient = new MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>(mqttNetwork);
    // Generate username, reference : https://docs.microsoft.com/ko-kr/azure/iot-hub/iot-hub-mqtt-support
    std::string username = std::string(MQTT_SERVER_HOST_NAME) + "/" + DEVICE_ID + "/?api-version=2018-06-30";

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
    data.clientID.cstring = (char*)DEVICE_ID;
    data.username.cstring = (char*)username.c_str();
    data.password.cstring = (char*)"ignored";

    int rc = mqttClient->connect(data);
    if (rc != MQTT::SUCCESS)
    {
        printf("ERROR: rc from MQTT connect is %d\r\n", rc);
        return -1;
    }
    printf("Client connected.\r\n\r\n");

    // Generates topic names from user's setting 
    mqtt_topic_pub = std::string("devices/") + DEVICE_ID + "/messages/events/";
    std::string mqtt_topic_sub = std::string("devices/") + DEVICE_ID + "/messages/devicebound/#";

    /* Subscribe a topic. */
    printf("Client is trying to subscribe a topic \"%s\".\r\n", mqtt_topic_sub.c_str());
    rc = mqttClient->subscribe(mqtt_topic_sub.c_str(), MQTT::QOS0, messageArrived);
    if (rc != MQTT::SUCCESS)
    {
        printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
        return -1;
    }
    printf("Client has subscribed a topic \"%s\".\r\n\r\n", mqtt_topic_sub.c_str());

    t.start(callback(&queue, &EventQueue::dispatch_forever));
    btn1.fall(queue.event(btn_handler));

    while(1)
    {
        if(!mqttClient->isConnected())
        {
            printf("MQTT Client disconnect\r\n");
            return -1;
        }
        /* Waits a message and handles keepalive. */
        if(mqttClient->yield(100) != MQTT::SUCCESS)
        {
            printf("failed mqtt yield\r\n");
            //return -1;
        }
    }
}