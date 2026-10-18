#include "nettransport.h"

#include "../main.h"

#include <cstring>

namespace NetTransport {
namespace {

struct TransportStats {
	uint32_t lastLogTick;
	uint32_t sendCount;
	uint32_t rpcCount;
	uint32_t receiveCount;
	uint32_t deallocateCount;
	uint32_t disconnectCount;
	uint32_t failCount;
	uint32_t bytesOut;
	uint32_t bytesIn;
};

TransportStats g_stats{};

bool IsKnownPacketId(unsigned char id)
{
	switch (id) {
		case ID_RPC:
		case ID_AUTH_KEY:
		case ID_CONNECTION_ATTEMPT_FAILED:
		case ID_NO_FREE_INCOMING_CONNECTIONS:
		case ID_DISCONNECTION_NOTIFICATION:
		case ID_CONNECTION_LOST:
		case ID_CONNECTION_REQUEST_ACCEPTED:
		case ID_FAILED_INITIALIZE_ENCRIPTION:
		case ID_CONNECTION_BANNED:
		case ID_INVALID_PASSWORD:
		case ID_PLAYER_SYNC:
		case ID_VEHICLE_SYNC:
		case ID_AIM_SYNC:
		case ID_BULLET_SYNC:
		case ID_MARKERS_SYNC:
		case ID_UNOCCUPIED_SYNC:
		case ID_TRAILER_SYNC:
		case ID_PASSENGER_SYNC:
		case ID_CUSTOM_SYNC:
		case 251:
			return true;
		default:
			return false;
	}
}

void MaybeLogStats(const char* reason)
{
	const uint32_t now = GetTickCount();
	if (now - g_stats.lastLogTick < 2000) {
		return;
	}

	g_stats.lastLogTick = now;
	FLog("[NET_API64] reason=%s tx=%u rpc=%u rx=%u free=%u drop=%u fail=%u out=%u in=%u",
		 reason ? reason : "?",
		 g_stats.sendCount,
		 g_stats.rpcCount,
		 g_stats.receiveCount,
		 g_stats.deallocateCount,
		 g_stats.disconnectCount,
		 g_stats.failCount,
		 g_stats.bytesOut,
		 g_stats.bytesIn);
}

uint32_t StreamBytes(const RakNet::BitStream* stream)
{
	return stream ? static_cast<uint32_t>(stream->GetNumberOfBytesUsed()) : 0u;
}

} // namespace

unsigned char PacketId(const Packet* packet)
{
	if (!packet || !packet->data || packet->length == 0) {
		return 255;
	}

	const unsigned int len = packet->length;
	const unsigned char head = static_cast<unsigned char>(packet->data[0]);
	if (head != ID_TIMESTAMP) {
		return head;
	}

	auto idAt = [&](unsigned int offset) -> unsigned char {
		return (offset < len) ? static_cast<unsigned char>(packet->data[offset]) : 255;
	};

	const unsigned char idNative = idAt(sizeof(unsigned char) + sizeof(RakNetTime));
	const unsigned char id32 = idAt(sizeof(unsigned char) + 4u);
	const unsigned char id64 = idAt(sizeof(unsigned char) + 8u);

	if (id64 == ID_CUSTOM_SYNC || id64 == 251) {
		return id64;
	}
	if (id32 == ID_CUSTOM_SYNC || id32 == 251) {
		return id32;
	}

	const bool nativeKnown = IsKnownPacketId(idNative);
	const bool id32Known = IsKnownPacketId(id32);
	const bool id64Known = IsKnownPacketId(id64);

	if (nativeKnown) {
		return idNative;
	}
	if (id64Known && !id32Known) {
		return id64;
	}
	if (id32Known) {
		return id32;
	}
	if (id64Known) {
		return id64;
	}

	return idNative;
}

Packet* Receive(RakClientInterface* client)
{
	if (!client) {
		++g_stats.failCount;
		MaybeLogStats("receive-null");
		return nullptr;
	}

	Packet* packet = client->Receive();
	if (packet) {
		++g_stats.receiveCount;
		g_stats.bytesIn += packet->length;
		MaybeLogStats("receive");
	}
	return packet;
}

void Deallocate(RakClientInterface* client, Packet* packet)
{
	if (!client || !packet) {
		return;
	}

	++g_stats.deallocateCount;
	client->DeallocatePacket(packet);
	MaybeLogStats("free");
}

void Disconnect(RakClientInterface* client, unsigned int blockDuration, unsigned char orderingChannel)
{
	if (!client) {
		++g_stats.failCount;
		MaybeLogStats("disconnect-null");
		return;
	}

	++g_stats.disconnectCount;
	client->Disconnect(blockDuration, orderingChannel);
	MaybeLogStats("disconnect");
}

bool Send(RakClientInterface* client,
          RakNet::BitStream* stream,
          PacketPriority priority,
          PacketReliability reliability,
          char orderingChannel,
          const char* tag)
{
	if (!client || !stream) {
		++g_stats.failCount;
		MaybeLogStats(tag ? tag : "send-null");
		return false;
	}

	const uint32_t bytes = StreamBytes(stream);
	const bool ok = client->Send(stream, priority, reliability, orderingChannel);
	++g_stats.sendCount;
	g_stats.bytesOut += bytes;
	if (!ok) {
		++g_stats.failCount;
	}
	MaybeLogStats(tag ? tag : "send");
	return ok;
}

bool Rpc(RakClientInterface* client,
         int* uniqueId,
         RakNet::BitStream* stream,
         PacketPriority priority,
         PacketReliability reliability,
         char orderingChannel,
         bool shiftTimestamp,
         NetworkID networkId,
         RakNet::BitStream* replyFromTarget,
         const char* tag)
{
	if (!client || !uniqueId || !stream) {
		++g_stats.failCount;
		MaybeLogStats(tag ? tag : "rpc-null");
		return false;
	}

	const uint32_t bytes = StreamBytes(stream);
	const bool ok = client->RPC(uniqueId,
	                            stream,
	                            priority,
	                            reliability,
	                            orderingChannel,
	                            shiftTimestamp,
	                            networkId,
	                            replyFromTarget);
	++g_stats.rpcCount;
	g_stats.bytesOut += bytes;
	if (!ok) {
		++g_stats.failCount;
	}
	MaybeLogStats(tag ? tag : "rpc");
	return ok;
}

bool ShouldSendDelta(uint32_t& lastTick,
                     const void* previous,
                     const void* current,
                     size_t size,
                     uint32_t minIntervalMs)
{
	const uint32_t now = GetTickCount();
	if (now - lastTick <= minIntervalMs &&
		previous &&
		current &&
		std::memcmp(previous, current, size) == 0) {
		return false;
	}

	lastTick = now;
	return true;
}

} // namespace NetTransport
