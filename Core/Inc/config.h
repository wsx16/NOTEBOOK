#ifndef __CONFIG_H__
#define __CONFIG_H__

/*============================================================
 *  Credentials Configuration
 *
 *  Copy this file or define these macros before building.
 *  DO NOT commit real credentials to version control.
 *============================================================*/

/* WiFi Access Point */
#ifndef WIFI_SSID
#define WIFI_SSID           "your_ssid"
#endif

#ifndef WIFI_PWD
#define WIFI_PWD            "your_password"
#endif

/* MQTT Broker */
#ifndef MQTTSERVER_IP
#define MQTTSERVER_IP       "broker.emqx.io"
#endif

#ifndef MQTTSERVER_PORT
#define MQTTSERVER_PORT     1883
#endif

#ifndef MQTTSERVER_CLIENT_ID
#define MQTTSERVER_CLIENT_ID "STM32U575_Client"
#endif

#ifndef MQTTSERVER_USER
#define MQTTSERVER_USER     "admin"
#endif

#ifndef MQTTSERVER_PASSWORD
#define MQTTSERVER_PASSWORD "public"
#endif

#ifndef MQTTSERVER_TOPIC
#define MQTTSERVER_TOPIC    "topic"
#endif

/* OneNET OTA Platform */
#ifndef ONENET_PRODUCT_ID
#define ONENET_PRODUCT_ID   "your_product_id"
#endif

#ifndef ONENET_DEVICE_NAME
#define ONENET_DEVICE_NAME  "your_device_name"
#endif

#ifndef ONENET_AUTHORIZATION
#define ONENET_AUTHORIZATION \
    "version=2022-05-01&res=userid%2Fxxxxx&et=xxxxxxxxxx&method=sha1&sign=xxxxxxxxxx"
#endif

#ifndef ONENET_HOST
#define ONENET_HOST         "iot-api.heclouds.com"
#endif

#ifndef ONENET_OTA_SERVER
#define ONENET_OTA_SERVER   "183.230.40.33"
#endif

#ifndef ONENET_OTA_PORT
#define ONENET_OTA_PORT     80
#endif

#ifndef ONENET_OTA_PATH
#define ONENET_OTA_PATH     "/fuse-ota/your_product/your_device"
#endif

#endif /* __CONFIG_H__ */
