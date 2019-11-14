#include "YioRoon.h"
#include "../remote-software/sources/entities/mediaplayer.h"

Roon::Roon() :
    _log("roon"),
    _api(nullptr),
    _entities(nullptr),
    _discovery(_log, parent())
{}

void Roon::create(const QVariantMap &config, QObject *entities, QObject *notifications, QObject *api, QObject *configObj)
{
    QMap<QObject *, QVariant>   returnData;
    QVariantList                data;

    _api = qobject_cast<YioAPIInterface *>(api);
    _entities = qobject_cast<EntitiesInterface *>(entities);

    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == "data") {
            data = iter.value().toList();
            break;
        }
    }
    for (int i = 0; i < data.length(); i++)
    {
        YioRoon* roon = new YioRoon(this);
        roon->setup(data[i].toMap(), entities, notifications, api, configObj);

        QVariantMap d = data[i].toMap();
        d.insert("type", config.value("type").toString());
        returnData.insert(roon, d);
    }
    if (data.length() > 0)
        emit createDone(returnData);
    else {
        connect (&_discovery, &QtRoonDiscovery::roonDiscovered, this, &Roon::onRoonDiscovered);
        _discovery.startDiscovery(1000, true);
    }
}
void Roon::onRoonDiscovered (QMap<QString, QVariantMap> soodmaps)
{
    QVariantMap roonsmap;

    int idx = 1;
    for (QMap<QString, QVariantMap>::const_iterator iter = soodmaps.begin(); iter != soodmaps.end(); ++iter) {
        QVariantMap soodmap = iter->value(iter.key()).toMap();
        QVariantMap roonmap;
        roonmap["ip"] = iter.key();
        roonmap["type"] = "roon";
        roonmap["friendly_name"] = soodmap["name"];
        roonmap["id"] = "roon" + (idx > 1 ? QString::number(idx) : "");
        roonsmap["data"] = roonmap;
        idx++;
    }
    // @@@RIC
}

QLoggingCategory YioRoon::_log("roon");
YioRoon* YioRoon::_instance = nullptr;

YioRoon::YioRoon(QObject* parent) :
    _roonApi("ws://192.168.1.100:9100/api", "C:\\Temp\\", _reg, _log),
    _browseApi(_roonApi),
    _transportApi(_roonApi, transportCallback),
    _numItems(50),
    _requestId(0),
    _items(nullptr)
{
    setParent (parent);

    _actions.append(Action ("Play Genre",       "", ACT_ACTIONLIST, static_cast<EAction>(ACT_SHUFFLE | ACT_STARTRADIO)));
    _actions.append(Action ("Play Artist",      "", ACT_ACTIONLIST, static_cast<EAction>(ACT_SHUFFLE | ACT_ADDNEXT | ACT_QUEUE | ACT_STARTRADIO)));
    _actions.append(Action ("Play Album",       "", ACT_ACTIONLIST, static_cast<EAction>(ACT_PLAYNOW | ACT_ADDNEXT | ACT_QUEUE | ACT_STARTRADIO)));
    _actions.append(Action ("Play Playlist",    "", ACT_ACTIONLIST, static_cast<EAction>(ACT_PLAYNOW | ACT_ADDNEXT | ACT_QUEUE | ACT_STARTRADIO)));
    _actions.append(Action ("Play Now",         "PLAY", ACT_PLAYNOW, ACT_NONE));
    _actions.append(Action ("Shuffle",          "SHUFFLE", ACT_SHUFFLE, ACT_NONE));
    _actions.append(Action ("Add Next",         "NEXT", ACT_ADDNEXT, ACT_NONE));
    _actions.append(Action ("Queue",            "QUEUE", ACT_QUEUE, ACT_NONE));
    _actions.append(Action ("Start Radio",      "RADIO", ACT_QUEUE, ACT_NONE));
    _actions.append(Action ("TIDAL",            "", ACT_REJECT, ACT_NONE));
    _actions.append(Action ("Settings",         "", ACT_REJECT, ACT_NONE));

    _reg.display_name = "YIO Roon Control";
    _reg.display_version = "1.1";
    _reg.email = "ric@rts.co.at";
    _reg.extension_id = "com.roon.yiocontrol";
    _reg.publisher = "Christian Riedl";
    _reg.token = "";
    _reg.website = "https://www.christian-riedl.com";
    _reg.provided_services.append(QtRoonApi::ServicePairing);
    _reg.required_services.append(QtRoonApi::ServiceBrowse);
    _reg.required_services.append(QtRoonApi::ServiceTransport);
    //_reg.required_services.append(QtRoonApi::ServiceImage);
    //_reg.provided_services.append(QtRoonApi::ServicePing);
    _reg.paired = this;

    QObject::connect(&_transportApi, &QtRoonTransportApi::zonesChanged, this, &YioRoon::onZonesChanged);
    QObject::connect(&_transportApi, &QtRoonTransportApi::zoneSeekChanged, this, &YioRoon::onZoneSeekChanged);
    _instance = this;
}
YioRoon::~YioRoon()
{
    _instance = nullptr;
    if (_items != nullptr) {
        delete [] _items;
    }
}

void YioRoon::setup (const QVariantMap& config, QObject *entities, QObject *notifications, QObject* api, QObject *configObj)
{
    Q_UNUSED(notifications)
    Q_UNUSED(api)
    Q_UNUSED(configObj)

    _log.setEnabled(QtMsgType::QtDebugMsg, false);     // Default, only debug disabled
    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == "friendly_name")
            setFriendlyName(iter.value().toString());
        else if (iter.key() == "id")
            setIntegrationId(iter.value().toString());
        else if (iter.key() == "ip") {
            QString ip = iter.value().toString();
            if (!ip.contains(':'))
                ip += ":9100";
            _url = "ws://" + ip + "/api";
            _imageUrl = "http://" + ip + "/api/image/";
        }
        else if (iter.key() == "log") {
            const QString& severity = iter.value().toString();
            if (severity == "debug") {
                _log.setEnabled(QtMsgType::QtDebugMsg, true);
            }
            else if (severity == "info") {
                _log.setEnabled(QtMsgType::QtDebugMsg, false);
                _log.setEnabled(QtMsgType::QtInfoMsg, true);
            }
            else if (severity == "warning") {
                _log.setEnabled(QtMsgType::QtDebugMsg, false);
                _log.setEnabled(QtMsgType::QtInfoMsg, false);
                _log.setEnabled(QtMsgType::QtWarningMsg, true);
            }
        }
    }
    _entities = qobject_cast<EntitiesInterface *>(entities);
    _roonApi.setup(_url);
}

void YioRoon::connect()
{
    if (_entityIds.length() == 0) {
        QVariantList emptyList;
        QStringList emptyButtons = {"", "", ""};
        QList<QObject*> list = _entities->getByIntegration(integrationId());
        for (int i = 0; i < list.length(); i++) {
            Entity* entity = static_cast<Entity*>(list[i]);
            _entityIds.append(entity->entity_id());
            _friendlyNames.append(entity->friendly_name());
            _zoneIds.append("");
            _forcedActions.append("");
            QVariantMap     map;
            map["browseItems"] = emptyList;
            map["browseCmds"] = emptyButtons;
            _entities->update(entity->entity_id(), map);
        }
        _items = new QList<QtRoonBrowseApi::BrowseItem> [static_cast<size_t>(list.length())];
    }
    _roonApi.open();
}
void YioRoon::disconnect()
{
    //@@@
}
void YioRoon::sendCommand(const QString& type, const QString& id, const QString& cmd, const QVariant& param)
{
/*
    "SOURCE", "APP_NAME",
    "VOLUME", "VOLUME_UP", "VOLUME_DOWN", "VOLUME_SET","MUTE","MUTE_SET",
    "MEDIA_TYPE", "MEDIA_TITLE", "MEDIA_ARTIST", "MEDIA_ALBUM", "MEDIA_DURATION", "MEDIA_POSITION", "MEDIA_IMAGE",
    "PLAY", "PAUSE", "STOP", "PREVIOUS", "NEXT", "SEEK", "SHUFFLE", "TURN_ON", "TURN_OFF"
*/
    if (_log.isDebugEnabled())
        qCDebug(_log) << "ROON sendCommand " << type << " " << id << " " << cmd << " " << param.toString();

    int idx = _entityIds.indexOf(id);
    if (idx < 0 || idx >= _zoneIds.length()) {
        qCWarning(_log) << "ROON can't find id " << id;
        return;
    }
    const QString& currentZoneId = _zoneIds[idx];

    const QtRoonTransportApi::Zone& zone = _transportApi.getZone(currentZoneId);
    const QtRoonTransportApi::Output& output = zone.outputs[0];
    if (cmd == "VOLUME") {
        _transportApi.changeVolume(output.output_id, QtRoonTransportApi::EValueMode::absolute, param.toInt());
    }
    else if (cmd == "VOLUME_UP") {
        _transportApi.changeVolume(output.output_id, QtRoonTransportApi::EValueMode::relative_step, 1);
    }
    else if (cmd == "VOLUME_DOWN") {
        _transportApi.changeVolume(output.output_id, QtRoonTransportApi::EValueMode::relative_step, -1);
    }
    else if (cmd == "MUTE_SET") {
        _transportApi.mute(output.output_id, param.toBool() ? QtRoonTransportApi::EMute::mute : QtRoonTransportApi::EMute::unmute);
    }
    else if (cmd == "PLAY") {
        _transportApi.control(currentZoneId, QtRoonTransportApi::EControl::play);
    }
    else if (cmd == "PAUSE") {
        _transportApi.control(currentZoneId, QtRoonTransportApi::EControl::pause);
    }
    else if (cmd == "STOP") {
        _transportApi.control(currentZoneId, QtRoonTransportApi::EControl::stop);
    }
    else if (cmd == "PREVIOUS") {
        _transportApi.control(currentZoneId, QtRoonTransportApi::EControl::previous);
    }
    else if (cmd == "NEXT") {
        _transportApi.control(currentZoneId, QtRoonTransportApi::EControl::next);
    }
    else if (cmd == "BROWSE") {
        _lastBrowseId = id;
        QString cmd = param.toString();
        if (cmd == "TOP")
            browse(currentZoneId, true);
        else if (cmd == "BACK")
            browseBack(currentZoneId);
        else {
            const QList<QtRoonBrowseApi::BrowseItem>& items = _items[idx];
            for (int i = 0; i < items.length(); i++) {
                if (items[i].item_key == cmd) {
                    browse(currentZoneId, items, i, false);
                    return;
                }
            }
            const Action* action = getActionYio(cmd);
            if (action == nullptr) {
                qCWarning(_log) << "Can't find Browse command " << cmd;
                return;
            }
            for (int i = 0; i < items.length(); i++) {
                if (items[i].hint == "action_list") {
                    const Action* actionList = getActionRoon(items[i].title);
                    if (actionList != nullptr) {
                        _forcedActions[idx] = cmd;
                        browseAction (currentZoneId, items[i].item_key);
                        return;
                    }
                }
                if (items[i].hint == "action") {
                    if (items[i].title == action->roonName) {
                        browse(currentZoneId, items, i, true);
                        return;
                    }
                }
            }
            qCWarning(_log) << "Can't find action list" << action->roonName;
        }
    }
}
void YioRoon::stateHandler(int state)
{
    if (state == CONNECTED) {
        setState(CONNECTED);
    } else if (state == CONNECTING) {
        setState(CONNECTING);
    } else if (state == DISCONNECTED) {
        setState(DISCONNECTED);
    }
}

void YioRoon::browse(const QString& zoneId, bool fromTop)
{
    QtRoonBrowseApi::BrowseOption opt;
    opt.zone_or_output_id = zoneId;
    if (fromTop)
        opt.pop_all = true;
    opt.set_display_offset = 0;
    _requestId = _browseApi.browse(opt, browseCallback);
}
void YioRoon::browse(const QString& zoneId, const QList<QtRoonBrowseApi::BrowseItem>& items, int itemIndex, bool action)
{
    if (itemIndex < items.count()) {
        QtRoonBrowseApi::BrowseOption opt;
        opt.zone_or_output_id = zoneId;
        opt.item_key = items[itemIndex].item_key;
        if (!action)
            opt.set_display_offset = 0;
        _requestId = _browseApi.browse(opt, browseCallback);
    }
}
void YioRoon::browseAction(const QString& zoneId, const QString& item_key)
{
    QtRoonBrowseApi::BrowseOption opt;
    opt.zone_or_output_id = zoneId;
    opt.item_key = item_key;
    _requestId = _browseApi.browse(opt, actionBrowseCallback);
}
void YioRoon::browseBack (const QString& zoneId)
{
    QtRoonBrowseApi::BrowseOption opt;
    opt.zone_or_output_id = zoneId;
    opt.pop_levels = 1;
    opt.set_display_offset = 0;
    _requestId = _browseApi.browse(opt, browseCallback);
}
void YioRoon::browseRefresh(const QString& zoneId)
{
    QtRoonBrowseApi::BrowseOption opt;
    opt.zone_or_output_id = zoneId;
    opt.refresh_list = true;
    opt.set_display_offset = 0;
    _requestId = _browseApi.browse(opt, browseCallback);
}

void YioRoon::onZonesChanged()
{
    QMap<QString, QtRoonTransportApi::Zone>& zones = _transportApi.zones();

    for (QMap<QString, QtRoonTransportApi::Zone>::iterator i = zones.begin(); i != zones.end(); ++i) {
        const QtRoonTransportApi::Zone& zone = i.value();
        int idx = _friendlyNames.indexOf(zone.display_name);
        if (idx < 0)
            qCDebug (_log) << "Zone Not found " << zone.display_name;
        else {
            _zoneIds[idx] = zone.zone_id;
            updateZone(_entityIds[idx], zone, false);
        }
    }

    /*
    for (auto e : zones.keys())
    {
        const QtRoonTransportApi::Zone& zone = zones.value(e);
        int idx = _friendlyNames.indexOf(zone.display_name);
        if (idx < 0)
            qCDebug (_log) << "Zone Not found " << zone.display_name;
        else {
            while (idx >= _zoneIds.length())
                _zoneIds.append("");
            _zoneIds[idx] = zone.zone_id;
            updateZone(_entityIds[idx], zone, false);
        }
    }
    */
}

void YioRoon::onZoneSeekChanged(const QtRoonTransportApi::Zone& zone)
{
    int idx = _zoneIds.indexOf(zone.zone_id);
    if (idx < 0 || idx >= _entityIds[idx])
        qCDebug (_log) << "Seek Zone Not found " << zone.display_name;
    else
        updateZone (_entityIds[idx], zone, true);
}

void YioRoon::OnPaired(const RoonCore& core)
{
    Q_UNUSED(core)
    _transportApi.subscribeZones();
}

void YioRoon::OnUnpaired(const RoonCore& core)
{
    Q_UNUSED(core)
}

void YioRoon::transportCallback(int requestId, const QString& msg)
{
    Q_UNUSED(requestId)
    Q_UNUSED(msg)
}
void YioRoon::loadCallback(int requestId, const QString& err, const QtRoonBrowseApi::LoadResult& result)
{
    Q_UNUSED(err)
    if (_instance != nullptr && _instance->_requestId == requestId) {
        _instance->_requestId = 0;
        _instance->updateItems(result);
    }
}
void YioRoon::browseCallback(int requestId, const QString& err, const QtRoonBrowseApi::BrowseResult& result)
{
    Q_UNUSED(err)
    if (_instance != nullptr && _instance->_requestId == requestId) {
        _instance->_requestId = 0;
        if (result.action == "list" && result.list != nullptr) {
            int listoffset = result.list->display_offset > 0 ? result.list->display_offset : 0;
                QtRoonBrowseApi::LoadOption opt;
                opt.offset = listoffset;
                opt.count = _instance->_numItems;
                opt.set_display_offset = listoffset;
                _instance->_requestId = _instance->_browseApi.load(opt, loadCallback);
        }
    }
}
void YioRoon::actionLoadCallback(int requestId, const QString& err, const QtRoonBrowseApi::LoadResult& result)
{
    Q_UNUSED(err)
    if (_instance != nullptr && _instance->_requestId == requestId) {
        _instance->_requestId = 0;
        for (int i = 0; i < _instance->_forcedActions.length(); i++) {
            const QString & action = _instance->_forcedActions[i];
            if (!action.isEmpty()) {
                const Action* a = _instance->getActionYio(action);
                for (int j = 0; j < result.items.length(); j++) {
                    const QtRoonBrowseApi::BrowseItem& item = result.items[j];
                    if (item.title == a->roonName) {
                        // thats it
                        _instance->browse(_instance->_zoneIds[i], result.items, j, true);
                        _instance->_forcedActions[i] = "";
                    }
                }
            }
        }
        qWarning(_instance->_log) << "Can't find forced action";
    }
}
void YioRoon::actionBrowseCallback(int requestId, const QString& err, const QtRoonBrowseApi::BrowseResult& result)
{
    Q_UNUSED(err)
    if (_instance != nullptr && _instance->_requestId == requestId) {
        _instance->_requestId = 0;
        if (result.action == "list" && result.list != nullptr) {
            int listoffset = result.list->display_offset > 0 ? result.list->display_offset : 0;
            QtRoonBrowseApi::LoadOption opt;
            opt.offset = listoffset;
            opt.count = _instance->_numItems;
            opt.set_display_offset = listoffset;
            _instance->_requestId = _instance->_browseApi.load(opt, actionLoadCallback);
        }
    }
}
void YioRoon::updateZone (const QString& id, const QtRoonTransportApi::Zone& zone, bool seekChanged) {
    QVariantMap map;
    /*
        Q_PROPERTY  (states         state       READ    state       NOTIFY      stateChanged)
        Q_PROPERTY  (double         volume      READ    volume      NOTIFY      volumeChanged)
        Q_PROPERTY  (bool           muted       READ    muted       NOTIFY      mutedChanged)
        Q_PROPERTY  (QString        mediaType   READ    mediaType   NOTIFY      mediaTypeChanged)
        Q_PROPERTY  (QString        mediaTitle  READ    mediaTitle  NOTIFY      mediaTitleChanged)
        Q_PROPERTY  (QString        mediaArtist READ    mediaArtist NOTIFY      mediaArtistChanged)
        Q_PROPERTY  (QString        mediaImage  READ    mediaImage  NOTIFY      mediaImageChanged)
        Q_PROPERTY  (QString        source      READ    source      NOTIFY      sourceChanged)
    */
    if (!seekChanged) {
        MediaPlayer::states state;
        switch (zone.state) {
            case QtRoonTransportApi::EState::unknown:
            case QtRoonTransportApi::EState::stopped:   state = MediaPlayer::OFF;       break;
            case QtRoonTransportApi::EState::playing:   state = MediaPlayer::PLAYING;   break;
            case QtRoonTransportApi::EState::paused:    state = MediaPlayer::ON;        break;
            case QtRoonTransportApi::EState::loading:   state = MediaPlayer::IDLE;      break;
        }
        map["state"] = state;
        if (zone.outputs.length() > 0 && zone.outputs[0].volume != nullptr) {
            const QtRoonTransportApi::Volume* volume = zone.outputs[0].volume;
            map["volume"] = volume->value;
            map["muted"] = volume->is_muted;
        }
        if (zone.now_playing != nullptr) {
            const QtRoonTransportApi::NowPlaying* np = zone.now_playing;
            if (!np->image_key.isEmpty())
                map["mediaImage"] = _imageUrl + np->image_key + "?scale=fit&width=256&height=256";
            else
                map["mediaImage"] = "";
            map["mediaArtist"] = np->two_line->line2;
            map["mediaTitle"] = np->two_line->line1;
        }
        else {
            map["mediaImage"] = "";
            map["mediaArtist"] = "";
            map["mediaTitle"] = "";
        }
        map["source"] = "Source";
        map["mediaType"] = "MediaType";
        if (_log.isInfoEnabled()) {
            QString line = id;
            for (QVariantMap::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
                line += " ";
                line += iter.key();
                line += ":";
                line += iter.value().toString();
            }
            qCInfo(_log) << "ROON update " << line;
        }
        _entities->update(id, map);
    }
}
void YioRoon::updateItems (const QtRoonBrowseApi::LoadResult& result) {
    QVariantList    list;
    QStringList     buttons;
    int             level = 0;

    if (result.list != nullptr) {
        level = result.list->level;
        if (level > 0) {
            buttons.append("TOP");
            buttons.append("BACK");
        }
    }
    int idx = _entityIds.indexOf(_lastBrowseId);
    if (idx < 0) {
        qCWarning(_log) << "updateItems not found " << _lastBrowseId;
        _lastBrowseId.clear();
        return;
    }
    QList<QtRoonBrowseApi::BrowseItem>& items = _items[idx];
    items.clear();
    for (int i = 0; i < result.items.length(); i++) {
        const QtRoonBrowseApi::BrowseItem& item = result.items[i];
        items.append(item);
        if (item.input_prompt != nullptr)
            continue;
        if (item.hint == "action_list" || level == 0) {
            const Action* action = getActionRoon(item.title);
            if (action != nullptr) {
                if (action->action & ACT_REJECT)
                    continue;
                if (action->action & ACT_ACTIONLIST) {
                    QStringList btns = getForcedActions(action->forcedActions);
                    buttons.append(btns);
                    continue;
                }
            }
        }
        if (item.hint == "action") {
            const Action* action = getActionRoon(item.title);
            if (action != nullptr) {
                buttons.append(action->yioName);
                continue;
            }
        }
        QVariantMap entityItem;
        entityItem["item_key"] = item.item_key;
        entityItem["title"] = item.title;
        entityItem["sub_title"] = item.subtitle;
        if (!item.image_key.isEmpty())
            entityItem["image_url"] = _imageUrl + item.image_key + "?scale=fit&width=64&height=64";
        else
            entityItem["image_url"] = "";
        list.append(entityItem);
    }
    while (buttons.length() < 3)
        buttons.append("");

    QVariantMap     map;
    map["browseItems"] = list;
    map["browseCmds"] = buttons;

    _entities->update(_lastBrowseId, map);
    _lastBrowseId.clear();
}
const YioRoon::Action* YioRoon::getActionRoon (const QString& roonName) {
    for (QList<Action>::const_iterator iter = _actions.begin(); iter != _actions.end(); ++iter) {
        if (iter->roonName == roonName)
            return &*iter;
    }
    return nullptr;
}
const YioRoon::Action* YioRoon::getActionYio (const QString& yioName) {
    for (QList<Action>::const_iterator iter = _actions.begin(); iter != _actions.end(); ++iter) {
        if (iter->yioName == yioName)
            return &*iter;
    }
    return nullptr;
}
QStringList YioRoon::getForcedActions (EAction forcedActions) {
    EAction actions[] = { ACT_PLAYNOW, ACT_SHUFFLE, ACT_ADDNEXT, ACT_QUEUE, ACT_STARTRADIO };
    QStringList list;
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        if (forcedActions & actions[i]) {
            for (QList<Action>::const_iterator iter = _actions.begin(); iter != _actions.end(); ++iter) {
                if (iter->action == actions[i]) {
                    list.append(iter->yioName);
                    break;
                }
            }
        }
    }
    return list;
}
