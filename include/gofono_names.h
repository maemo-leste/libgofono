/*
 * Copyright (C) 2014-2016 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the Jolla Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef GOFONO_NAMES_H
#define GOFONO_NAMES_H

#define OFONO_BUS_TYPE                            G_BUS_TYPE_SYSTEM

#define OFONO_SERVICE                             "org.ofono"
#define OFONO_(interface)                         OFONO_SERVICE "." interface

#define OFONO_MANAGER_INTERFACE_NAME              OFONO_("Manager")
#define OFONO_MODEM_INTERFACE_NAME                OFONO_("Modem")
#define OFONO_SIMMGR_INTERFACE_NAME               OFONO_("SimManager")
#define OFONO_CONNMGR_INTERFACE_NAME              OFONO_("ConnectionManager")
#define OFONO_CONNCTX_INTERFACE_NAME              OFONO_("ConnectionContext")
#define OFONO_NETREG_INTERFACE_NAME               OFONO_("NetworkRegistration")

#define OFONO_AUDIO_SETTINGS_INTERFACE_NAME       OFONO_("AudioSettings")
#define OFONO_CALL_BARRING_INTERFACE_NAME         OFONO_("CallBarring")
#define OFONO_CALL_FORWARDING_INTERFACE_NAME      OFONO_("CallForwarding")
#define OFONO_CALL_METER_INTERFACE_NAME           OFONO_("CallMeter")
#define OFONO_CALL_SETTINGS_INTERFACE_NAME        OFONO_("CallSettings")
#define OFONO_CALL_VOLUME_INTERFACE_NAME          OFONO_("CallVolume")
#define OFONO_CELL_BROADCAST_INTERFACE_NAME       OFONO_("CellBroadcast")
#define OFONO_HANDSFREE_INTERFACE_NAME            OFONO_("Handsfree")
#define OFONO_LOCATION_REPORTING_INTERFACE_NAME   OFONO_("LocationReporting")
#define OFONO_MESSAGE_MANAGER_INTERFACE_NAME      OFONO_("MessageManager")
#define OFONO_MESSAGE_WAITING_INTERFACE_NAME      OFONO_("MessageWaiting")
#define OFONO_NETREG_INTERFACE_NAME               OFONO_("NetworkRegistration")
#define OFONO_PHONEBOOK_INTERFACE_NAME            OFONO_("Phonebook")
#define OFONO_PUSH_NOTIFICATION_INTERFACE_NAME    OFONO_("PushNotification")
#define OFONO_RADIO_SETTINGS_INTERFACE_NAME       OFONO_("RadioSettings")
#define OFONO_SMART_MESSAGING_INTERFACE_NAME      OFONO_("SmartMessaging")
#define OFONO_SIM_TOOLKIT_INTERFACE_NAME          OFONO_("SimToolkit")
#define OFONO_TEXT_TELEPHONY_INTERFACE_NAME       OFONO_("TextTelephony")
#define OFONO_VOICECALL_MANAGER_INTERFACE_NAME    OFONO_("VoiceCallManager")

/* org.ofono.Modem */
#define OFONO_MODEM_PROPERTY_POWERED              "Powered"
#define OFONO_MODEM_PROPERTY_ONLINE               "Online"
#define OFONO_MODEM_PROPERTY_LOCKDOWN             "Lockdown"
#define OFONO_MODEM_PROPERTY_EMERGENCY            "Emergency"
#define OFONO_MODEM_PROPERTY_NAME                 "Name"
#define OFONO_MODEM_PROPERTY_MANUFACTURER         "Manufacturer"
#define OFONO_MODEM_PROPERTY_MODEL                "Model"
#define OFONO_MODEM_PROPERTY_REVISION             "Revision"
#define OFONO_MODEM_PROPERTY_SERIAL               "Serial"
#define OFONO_MODEM_PROPERTY_TYPE                 "Type"
#define OFONO_MODEM_PROPERTY_FEATURES             "Features"
#define OFONO_MODEM_PROPERTY_INTERFACES           "Interfaces"

/* org.ofono.SimManager */
#define OFONO_SIMMGR_PROPERTY_PRESENT             "Present"
#define OFONO_SIMMGR_PROPERTY_IMSI                "SubscriberIdentity"
#define OFONO_SIMMGR_PROPERTY_MCC                 "MobileCountryCode"
#define OFONO_SIMMGR_PROPERTY_MNC                 "MobileNetworkCode"
#define OFONO_SIMMGR_PROPERTY_SPN                 "ServiceProviderName"

/* org.ofono.ConnectionManager */
#define OFONO_CONNMGR_PROPERTY_ATTACHED           "Attached"
#define OFONO_CONNMGR_PROPERTY_ROAMING_ALLOWED    "RoamingAllowed"
#define OFONO_CONNMGR_PROPERTY_POWERED            "Powered"

/* org.ofono.ConnectionContext */
#define OFONO_CONNCTX_PROPERTY_TYPE               "Type"
#define OFONO_CONNCTX_PROPERTY_ACTIVE             "Active"
#define OFONO_CONNCTX_PROPERTY_APN                "AccessPointName"
#define OFONO_CONNCTX_PROPERTY_AUTH               "AuthenticationMethod"
#define OFONO_CONNCTX_PROPERTY_NAME               "Name"
#define OFONO_CONNCTX_PROPERTY_USERNAME           "Username"
#define OFONO_CONNCTX_PROPERTY_PASSWORD           "Password"
#define OFONO_CONNCTX_PROPERTY_PROTOCOL           "Protocol"
#define OFONO_CONNCTX_PROPERTY_MMS_PROXY          "MessageProxy"
#define OFONO_CONNCTX_PROPERTY_MMS_CENTER         "MessageCenter"

#define OFONO_CONNCTX_PROPERTY_SETTINGS           "Settings"
#define OFONO_CONNCTX_PROPERTY_IPV6_SETTINGS      "IPv6.Settings"
#define OFONO_CONNCTX_SETTINGS_INTERFACE          "Interface"
#define OFONO_CONNCTX_SETTINGS_METHOD             "Method"
#define OFONO_CONNCTX_SETTINGS_ADDRESS            "Address"
#define OFONO_CONNCTX_SETTINGS_NETMASK            "Netmask"
#define OFONO_CONNCTX_SETTINGS_GATEWAY            "Gateway"
#define OFONO_CONNCTX_SETTINGS_PREFIX_LENGTH      "PrefixLength"
#define OFONO_CONNCTX_SETTINGS_DNS                "DomainNameServers"

/* org.ofono.NetworkRegistration */
#define OFONO_NETREG_PROPERTY_STATUS              "Status"
#define OFONO_NETREG_PROPERTY_MODE                "Mode"
#define OFONO_NETREG_PROPERTY_CELL_ID             "CellId"
#define OFONO_NETREG_PROPERTY_LOCATION_AREA_CODE  "LocationAreaCode"
#define OFONO_NETREG_PROPERTY_TECHNOLOGY          "Technology"
#define OFONO_NETREG_PROPERTY_MCC                 "MobileCountryCode"
#define OFONO_NETREG_PROPERTY_MNC                 "MobileNetworkCode"
#define OFONO_NETREG_PROPERTY_NAME                "Name"
#define OFONO_NETREG_PROPERTY_STRENGTH            "Strength"

#endif /* GOFONO_NAMES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
