/*  =========================================================================
    autoconfig - Autoconfig

    Copyright (C) 2014 - 2017 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    autoconfig - Autoconfig
@discuss
@end
*/

#include "fty_alert_engine_classes.h"

#include <fstream>
#include <iostream>
#include <string>

#include <cxxtools/jsonserializer.h>
#include <cxxtools/jsondeserializer.h>

#include "fty_alert_engine_classes.h"

#define AUTOCONFIG "AUTOCONFIG"

std::string Autoconfig::StateFilePath;
std::string Autoconfig::RuleFilePath;
std::string Autoconfig::StateFile;
std::string Autoconfig::AlertEngineName;

static int
load_agent_info(std::string &info)
{
    if ( !shared::is_file (Autoconfig::StateFile.c_str ())) {
        log_error ("not a file");
        info = "";
        return -1;
    }
    std::ifstream f(Autoconfig::StateFile, std::ios::in | std::ios::binary);
    if (f) {
        f.seekg (0, std::ios::end);
        info.resize (f.tellg ());
        f.seekg (0, std::ios::beg);
        f.read (&info[0], info.size());
        f.close ();
        return 0;
    }
    log_error("Fail to read '%s'", Autoconfig::StateFile.c_str ());
    return -1;
}

static int
save_agent_info(const std::string& json)
{
    if (!shared::is_dir (Autoconfig::StateFilePath.c_str ())) {
        log_error ("Can't serialize state, '%s' is not directory", Autoconfig::StateFilePath.c_str ());
        return -1;
    }
    try {
        std::ofstream f(Autoconfig::StateFile);
        f.exceptions (~std::ofstream::goodbit);
        f << json;
        f.close();
    }
    catch (const std::exception& e) {
        log_error ("Can't serialize state, %s", e.what());
        return -1;
    }
    return 0;
}

inline void operator<<= (cxxtools::SerializationInfo& si, const AutoConfigurationInfo& info)
{
    si.setTypeName("AutoConfigurationInfo");
    si.addMember("type") <<= info.type;
    si.addMember("subtype") <<= info.subtype;
    si.addMember("operation") <<= info.operation;
    si.addMember("configured") <<= info.configured;
    si.addMember("date") <<= std::to_string (info.date);
    si.addMember("attributes") <<= info.attributes;
}

inline void operator>>= (const cxxtools::SerializationInfo& si, AutoConfigurationInfo& info)
{
    std::string temp;
    si.getMember("configured") >>= info.configured;
    si.getMember("type") >>= temp;
    si.getMember("subtype") >>= temp;
    si.getMember("operation") >>= temp;
    si.getMember("date") >>= temp;
    info.date = std::stoi (temp);
    si.getMember("attributes")  >>= info.attributes;
}

void Autoconfig::main (zsock_t *pipe, char *name)
{
    if ( _client ) mlm_client_destroy( &_client );
    _client = mlm_client_new ();
    assert (_client);

    zpoller_t *poller = zpoller_new (pipe, msgpipe (), NULL);
    assert (poller);
    _timestamp = zclock_mono ();
    zsock_signal (pipe, 0);

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, _timeout);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted ()");
                break;
            }
            if (zpoller_expired (poller)) {
                onPoll ();
                _timestamp = zclock_mono ();
                continue;
            }
            _timestamp = zclock_mono ();
            log_warning ("zpoller_wait () returned NULL while at the same time zpoller_terminated == 0, zsys_interrupted == 0, zpoller_expired == 0");
            continue;
        }

        int64_t now = zclock_mono ();
        if (now - _timestamp >= _timeout) {
            onPoll ();
            _timestamp = zclock_mono ();
        }

        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);

            if (streq (cmd, "$TERM")) {
                log_debug ("%s: $TERM received", name);
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                break;
            }
            else
            if (streq (cmd, "TEMPLATES_DIR")) {
                log_debug ("TEMPLATES_DIR received");
                char* dirname = zmsg_popstr (msg);
                if (dirname) {
                    Autoconfig::RuleFilePath = std::string (dirname);
                }
                else {
                    log_error ("%s: in TEMPLATES_DIR command next frame is missing", name);
                }
                zstr_free (&dirname);
            }
            else
            if (streq (cmd, "CONFIG")) {
                log_debug ("CONFIG received");
                char* dirname = zmsg_popstr (msg);
                if (dirname) {
                    Autoconfig::StateFilePath = std::string (dirname);
                    Autoconfig::StateFile = Autoconfig::StateFilePath + "/state";
                }
                else {
                    log_error ("%s: in CONFIG command next frame is missing", name);
                }
                zstr_free (&dirname);
            }
            else
            if (streq (cmd, "CONNECT")) {
                log_debug ("CONNECT received");
                char* endpoint = zmsg_popstr (msg);
                int rv = mlm_client_connect (_client, endpoint, 1000, name);
                if (rv == -1)
                    log_error ("%s: can't connect to malamute endpoint '%s'", name, endpoint);
                zstr_free (&endpoint);
            }
            else
            if (streq (cmd, "CONSUMER")) {
                log_debug ("CONSUMER received");
                char* stream = zmsg_popstr (msg);
                char* pattern = zmsg_popstr (msg);
                int rv = mlm_client_set_consumer (_client, stream, pattern);
                if (rv == -1)
                    log_error ("%s: can't set consumer on stream '%s', '%s'", name, stream, pattern);
                zstr_free (&pattern);
                zstr_free (&stream);
            }
            else
            if (streq (cmd, "ALERT_ENGINE_NAME")) {
                log_debug ("ALERT_ENGINE_NAME received");
                char* alert_engine_name = zmsg_popstr (msg);
                if (alert_engine_name) {
                    Autoconfig::AlertEngineName = std::string (alert_engine_name);
                }
                else {
                    log_error ("%s: in ALERT_ENGINE_NAME command next frame is missing", name);
                }
                zstr_free (&alert_engine_name);
            }

            zstr_free (&cmd);
            zmsg_destroy (&msg);
            continue;
        }

        zmsg_t *message = recv ();
        if (!message) {
            log_warning ("recv () returned NULL; zsys_interrupted == '%s'; command = '%s', subject = '%s', sender = '%s'",
                    zsys_interrupted ? "true" : "false", command (), subject (), sender ());
            continue;
        }
        if (is_fty_proto (message)) {
            fty_proto_t *bmessage = fty_proto_decode (&message);
            if (!bmessage ) {
                log_error ("can't decode message with subject %s, ignoring", subject ());
                continue;
            }

            if (fty_proto_id (bmessage) == FTY_PROTO_ASSET) {
                if (!streq(fty_proto_operation(bmessage), FTY_PROTO_ASSET_OP_INVENTORY)) {
                    onSend (&bmessage);
                }
                fty_proto_destroy (&bmessage);
                continue;
            }
            else {
                log_warning ("Weird fty_proto msg received, id = '%d', command = '%s', subject = '%s', sender = '%s'",
                        fty_proto_id (bmessage), command (), subject (), sender ());
                fty_proto_destroy (&bmessage);
                continue;
            }
        }
        else {
            // this should be a message from ALERT_ENGINE_NAME (fty-alert-engine or fty-alert-flexible)
            if (streq (sender (), "fty-alert-engine") ||
                streq (sender (), "fty-alert-flexible"))
            {
                char *reply = zmsg_popstr (message);
                if (streq (reply, "OK")) {
                    char *details = zmsg_popstr (message);
                    log_debug ("Received OK for rule '%s'", details);
                    zstr_free (&details);
                }
                else {
                    if (streq (reply, "ERROR")) {
                        char *details = zmsg_popstr (message);
                        log_error ("Received ERROR : '%s'", details);
                        zstr_free (&details);
                    }
                    else
                        log_warning ("Unexpected message received, command = '%s', subject = '%s', sender = '%s'",
                            command (), subject (), sender ());
                }
                zstr_free (&reply);
            }
            else
                log_warning ("Message from unknown sender received: sender = '%s', command = '%s', subject = '%s'.",
                    sender (), command (), subject ());
            zmsg_destroy (&message);
        }
    }
    zpoller_destroy (&poller);
}

const std::string Autoconfig::getEname (const std::string &iname)
{
    std::string ename;
    auto search = _containers.find (iname); // iname | ename
    if (search != _containers.end ())
        ename = search->second;
    return ename;
}

void
Autoconfig::onSend (fty_proto_t **message)
{
    if (!message || ! *message)
        return;

    AutoConfigurationInfo info;
    std::string device_name (fty_proto_name (*message));
    info.type.assign (fty_proto_aux_string (*message, "type", ""));
    info.subtype.assign (fty_proto_aux_string (*message, "subtype", ""));
    info.operation.assign (fty_proto_operation (*message));
    info.update_ts.assign (fty_proto_ext_string(*message, "update_ts", ""));

    if (_configurableDevices.find(device_name)!=_configurableDevices.end() &&
            0 != strcmp(fty_proto_ext_string(*message, "update_ts", ""),_configurableDevices[device_name].update_ts.c_str())) {
        log_debug("Changed asset, updating");
        info.configured = false;
    }

    if (streq (fty_proto_aux_string (*message, "type", ""), "datacenter") ||
        streq (fty_proto_aux_string (*message, "type", ""), "room") ||
        streq (fty_proto_aux_string (*message, "type", ""), "row") ||
        streq (fty_proto_aux_string (*message, "type", ""), "rack"))
    {
        if (info.operation != "delete") {
            _containers[device_name] = fty_proto_ext_string (*message, "name", "");
        } else {
            try {
                _containers.erase(device_name);
            } catch (const std::exception &e) {
                log_error( "can't erase container %s: %s", device_name.c_str(), e.what() );
            }
        }
    }
    if (info.type.empty ()) {
        log_debug("extracting attributes from asset message failed.");
        return;
    }

    if (streq(info.type.c_str(), "datacenter") || streq(info.type.c_str(), "room") ||
            streq(info.type.c_str(), "row") || streq(info.type.c_str(), "rack")) {
        _containers.emplace(device_name, fty_proto_ext_string(*message, "name", ""));
    }

    log_debug("Decoded asset message - device name = '%s', type = '%s', subtype = '%s', operation = '%s'",
            device_name.c_str (), info.type.c_str (), info.subtype.c_str (), info.operation.c_str ());
    info.attributes = utils::zhash_to_map(fty_proto_ext (*message));
    if (info.operation != "delete") {
        _configurableDevices[device_name] = info;
    } else {
        try {
            _configurableDevices.erase(device_name);
        } catch (const std::exception &e) {
            log_error( "can't erase device %s: %s", device_name.c_str(), e.what() );
        }

        if (info.subtype == "sensorgpio" || info.subtype == "gpo") {
            // don't do anything
        }
        else {
            const char *dest = Autoconfig::AlertEngineName.c_str ();
            // delete all rules for this asset
            zmsg_t *message = zmsg_new ();
            zmsg_addstr (message, "DELETEALL");
            zmsg_addstr (message, device_name.c_str());
            log_error ("Sending DELETEALL for %s to %s", device_name.c_str(), dest);
            if (sendto (dest, "rfc-evaluator-rules", &message) != 0) {
                log_error ("mlm_client_sendto (address = '%s', subject = '%s', timeout = '5000') failed.",
                            dest, "rfc-evaluator-rules");
            }
        }
    }
    saveState ();
    setPollingInterval();
}

void Autoconfig::onPoll( )
{
    static TemplateRuleConfigurator iTemplateRuleConfigurator;

    bool save = false;

    for (auto& it : _configurableDevices) {
        if (it.second.configured) {
            continue;
        }

        bool device_configured = true;
        if (zsys_interrupted)
            return;

        if ((&iTemplateRuleConfigurator)->isApplicable (it.second))
        {
            std::string la;
            for (auto &i : it.second.attributes)
            {
                if (i.first == "logical_asset")
                    la = i.second;
            }

            device_configured &= (&iTemplateRuleConfigurator)->configure (it.first, it.second, Autoconfig::getEname (la), client ());
        }
        else
            log_info ("No applicable configurator for device '%s', not configuring", it.first.c_str ());

        if (device_configured) {
            log_debug ("Device '%s' configured successfully", it.first.c_str ());
            it.second.configured = true;
            save = true;
        }
        else {
            log_debug ("Device '%s' NOT configured yet.", it.first.c_str ());
        }
        it.second.date = zclock_mono ();
    }

    if (save) {
        cleanupState();
        saveState();
    }
    setPollingInterval();
}

// autoconfig agent private methods

void Autoconfig::setPollingInterval( )
{
    _timeout = -1;
    for ( auto &it : _configurableDevices) {
        if ( ! it.second.configured ) {
            if ( it.second.date == 0 ) {
                // there is device that we didn't try to configure
                // let's try to do it soon
                _timeout = 5000;
                return;
            } else {
                // we failed to configure some device
                // let's try after one minute again
                _timeout = 60000;
            }
        }
    }
}

void Autoconfig::loadState()
{
    std::string json = "";
    int rv = load_agent_info(json);
    if ( rv != 0 || json.empty() )
        return;

    try {
        std::istringstream in(json);
        _configurableDevices.clear();
        cxxtools::JsonDeserializer deserializer(in);
        deserializer.deserialize(_configurableDevices);
    } catch (const std::exception &e) {
        log_error( "can't parse state: %s", e.what() );
    }
}

void Autoconfig::cleanupState()
{
    log_debug ("State file size before cleanup '%zu'", _configurableDevices.size ());

    // Just set the state file to empty
    save_agent_info("");
    return;
}

void Autoconfig::saveState()
{
    std::ostringstream stream;
    cxxtools::JsonSerializer serializer(stream);
    log_debug ("%s: State file size = '%zu'", __FUNCTION__, _configurableDevices.size ());
    serializer.serialize( _configurableDevices );
    serializer.finish();
    std::string json = stream.str();
    log_debug (json.c_str ());
    save_agent_info(json );
}

void autoconfig (zsock_t *pipe, void *args )
{
    char *name = (char *)args;
    log_info ("autoconfig agent started");
    Autoconfig agent( AUTOCONFIG );
    agent.run(pipe, name);
    log_info ("autoconfig agent exited");
}

void
autoconfig_test (bool verbose)
{
    printf (" * autoconfig: ");
    printf ("OK\n");
}
