#pragma once

#include <cstddef>
#include <cstdint>

#include "../vendor/raknet/BitStream.h"
#include "../vendor/raknet/NetworkTypes.h"
#include "../vendor/raknet/PacketEnumerations.h"
#include "../vendor/raknet/PacketPriority.h"
#include "../vendor/raknet/RakClientInterface.h"

namespace NetTransport {

unsigned char PacketId(const Packet* packet);

Packet* Receive(RakClientInterface* client);
void Deallocate(RakClientInterface* client, Packet* packet);
void Disconnect(RakClientInterface* client, unsigned int blockDuration, unsigned char orderingChannel = 0);

bool Send(RakClientInterface* client,
          RakNet::BitStream* stream,
          PacketPriority priority,
          PacketReliability reliability,
          char orderingChannel,
          const char* tag);

bool Rpc(RakClientInterface* client,
         int* uniqueId,
         RakNet::BitStream* stream,
         PacketPriority priority,
         PacketReliability reliability,
         char orderingChannel,
         bool shiftTimestamp,
         NetworkID networkId,
         RakNet::BitStream* replyFromTarget,
         const char* tag);

bool ShouldSendDelta(uint32_t& lastTick,
                     const void* previous,
                     const void* current,
                     size_t size,
                     uint32_t minIntervalMs = 500);

} // namespace NetTransport
