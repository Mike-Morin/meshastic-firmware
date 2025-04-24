#include "TextMessageModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "configuration.h"
TextMessageModule *textMessageModule;

ProcessMessage TextMessageModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#ifdef DEBUG_PORT
    auto &p = mp.decoded;
    LOG_INFO("Received text msg from=0x%0x, id=0x%x, msg=%.*s", mp.from, mp.id, p.payload.size, p.payload.bytes);
#endif
    // We only store/display messages destined for us.
    // Keep a copy of the most recent text message.
    devicestate.rx_text_message = mp;
    devicestate.has_rx_text_message = true;

    powerFSM.trigger(EVENT_RECEIVED_MSG);
    notifyObservers(&mp);

    // Get the short form of the NodeNum (e.g., "!0123ABCD")
    NodeNum deviceNodeNum = nodeDB->getNodeNum();
    char shortNodeId[10]; // 8 hex digits + '!' + null terminator
    snprintf(shortNodeId, sizeof(shortNodeId), "!%08X", deviceNodeNum);

    // Check if the message contains "<shortNodeId> wip"
    std::string message(p.payload.bytes, p.payload.bytes + p.payload.size);
    if (message.find(std::string(shortNodeId) + " wipe") != std::string::npos) {
        LOG_WARN("Received wipe command for this device. Erasing pre-shared keys.");

        // Iterate through all non-default channels and erase pre-shared keys
        for (int i = 1; i < MAX_NUM_CHANNELS; i++) {
            auto &ch = channels.getByIndex(i);
            if (ch.settings.psk.size > 0) {
                memset(ch.settings.psk.bytes, 0, ch.settings.psk.size);
                ch.settings.psk.size = 0;
                LOG_DEBUG("Erased PSK for channel %d", i);
            }
        }

        // Save changes to disk
        channels.onConfigChanged();
        service->reloadConfig(SEGMENT_CHANNELS);

        LOG_INFO("All pre-shared keys erased successfully.");
    }


    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

bool TextMessageModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}