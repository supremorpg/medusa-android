#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "game/GTASAEngineApi.h"
#include "game/World.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>

extern CGame* pGame;
extern CNetGame* pNetGame;

namespace {
ImU32 SampTextColorToImU32(uint32_t color)
{
	const int r = static_cast<int>((color >> 24) & 0xFF);
	const int g = static_cast<int>((color >> 16) & 0xFF);
	const int b = static_cast<int>((color >> 8) & 0xFF);
	const int a = static_cast<int>(color & 0xFF);
	return IM_COL32(r, g, b, a == 0 ? 255 : a);
}

int HexNibble(char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	return -1;
}

bool TryReadInlineColor(const char* cursor, const char* end, ImU32& color)
{
	if (!cursor || cursor + 7 > end || cursor[0] != '{' || cursor[7] != '}') {
		return false;
	}

	int value = 0;
	for (int i = 1; i <= 6; ++i) {
		const int nibble = HexNibble(cursor[i]);
		if (nibble < 0) return false;
		value = (value << 4) | nibble;
	}

	color = IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
	return true;
}

bool WorldToScreenTextLabel(const CVector& world, ImVec2& screen)
{
	CVector out{};
	if (!GTASAEngineApi::WorldToScreen(world, out)) return false;
	screen = ImVec2(out.x, out.y);
	return true;
}

void DrawTextSegmentOutlined(ImDrawList* draw, ImFont* font, float size, const ImVec2& pos,
							 ImU32 color, const char* begin, const char* end)
{
	if (!draw || !font || !begin || begin == end) return;
	const ImU32 shadow = IM_COL32(0, 0, 0, 220);
	draw->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y), shadow, begin, end);
	draw->AddText(font, size, ImVec2(pos.x - 1.0f, pos.y), shadow, begin, end);
	draw->AddText(font, size, ImVec2(pos.x, pos.y + 1.0f), shadow, begin, end);
	draw->AddText(font, size, ImVec2(pos.x, pos.y - 1.0f), shadow, begin, end);
	draw->AddText(font, size, pos, color, begin, end);
}

float CalcInlineLineWidth(ImFont* font, float size, const char* begin, const char* end)
{
	if (!font || !begin || begin >= end) return 0.0f;

	float width = 0.0f;
	const char* segment = begin;
	for (const char* cursor = begin; cursor < end; ++cursor) {
		ImU32 ignored = 0;
		if (TryReadInlineColor(cursor, end, ignored)) {
			if (segment < cursor) {
				width += font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment, cursor).x;
			}
			cursor += 7;
			segment = cursor + 1;
		} else if (*cursor == '\t') {
			if (segment < cursor) {
				width += font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment, cursor).x;
			}
			width += size;
			segment = cursor + 1;
		}
	}

	if (segment < end) {
		width += font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment, end).x;
	}
	return width;
}

void DrawInlineLineCentered(ImDrawList* draw, ImFont* font, float size, const ImVec2& center,
							ImU32 baseColor, const char* begin, const char* end)
{
	if (!draw || !font || !begin || begin >= end) return;

	float x = center.x - CalcInlineLineWidth(font, size, begin, end) * 0.5f;
	ImU32 currentColor = baseColor;
	const char* segment = begin;

	for (const char* cursor = begin; cursor < end; ++cursor) {
		ImU32 inlineColor = 0;
		if (TryReadInlineColor(cursor, end, inlineColor)) {
			if (segment < cursor) {
				DrawTextSegmentOutlined(draw, font, size, ImVec2(x, center.y), currentColor, segment, cursor);
				x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment, cursor).x;
			}
			currentColor = inlineColor;
			cursor += 7;
			segment = cursor + 1;
		} else if (*cursor == '\t') {
			if (segment < cursor) {
				DrawTextSegmentOutlined(draw, font, size, ImVec2(x, center.y), currentColor, segment, cursor);
				x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment, cursor).x;
			}
			x += size;
			segment = cursor + 1;
		}
	}

	if (segment < end) {
		DrawTextSegmentOutlined(draw, font, size, ImVec2(x, center.y), currentColor, segment, end);
	}
}
}

// 0.3.7
C3DTextLabelPool::C3DTextLabelPool()
{
	for (int i = 0; i < MAX_TEXT_LABELS; i++) {
		//m_TextLabels[i].pszText = nullptr;
		m_TextLabels[i] = nullptr;

		m_bSlotUsed[i] = false;
	}
}
// 0.3.7
C3DTextLabelPool::~C3DTextLabelPool()
{
	for (int i = 0; i < MAX_TEXT_LABELS; i++) {
		if (m_bSlotUsed[i]) {
			this->ClearLabel(i);
		}
	}
}
// 0.3.7
void C3DTextLabelPool::NewLabel(uint16_t wLabelId, TEXT_LABEL* pLabel) {

	if (wLabelId < MAX_TEXT_LABELS) {

		if (m_TextLabels[wLabelId])
		{
			delete m_TextLabels[wLabelId];
			m_TextLabels[wLabelId] = nullptr;
			m_bSlotUsed[wLabelId] = false;
		}

		//labelInfo.dwColor = (labelInfo.dwColor >> 8) | (labelInfo.dwColor << 24);
		//pTextLabel->pszText = new char[strlen(pLabel->pszText) + 1];
		//strcpy(pTextLabel->pszText, pLabel->pszText);
		TEXT_LABEL* pTextLabel = new TEXT_LABEL;
		pTextLabel->text = Encoding::cp2utf(pLabel->text);

		pTextLabel->dwColor = pLabel->dwColor;
		pTextLabel->vecPos.x = pLabel->vecPos.x;
		pTextLabel->vecPos.y = pLabel->vecPos.y;
		pTextLabel->vecPos.z = pLabel->vecPos.z;
		pTextLabel->fDistance = pLabel->fDistance;
		pTextLabel->bTestLOS = pLabel->bTestLOS;
		pTextLabel->playerId = pLabel->playerId;
		pTextLabel->vehicleId = pLabel->vehicleId;

		m_TextLabels[wLabelId] = pTextLabel;
		m_bSlotUsed[wLabelId] = true;
	}
}
// 0.3.7
void C3DTextLabelPool::ClearLabel(uint16_t wLabelId) {
	if (wLabelId < 0 || wLabelId >= MAX_TEXT_LABELS)
	{
		return;
	}
	m_bSlotUsed[wLabelId] = false;
	if (m_TextLabels[wLabelId])
	{
		delete m_TextLabels[wLabelId];
		m_TextLabels[wLabelId] = nullptr;
	}
}

void C3DTextLabelPool::Render(ImGuiRenderer* renderer)
{
	CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
	if(!pPlayerPed) return;

    CCamera& TheCamera = GTASAEngineApi::Camera();

	for (int i = 0; i < MAX_TEXT_LABELS; i++)
	{
		if (m_bSlotUsed[i]) {
            CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
            if (!pPlayerPool) break;

            TEXT_LABEL *pTextLabel = m_TextLabels[i];

            CVector vecTextPos = pTextLabel->vecPos;

            if (pTextLabel->playerId != INVALID_PLAYER_ID) {
                if (pTextLabel->playerId == pPlayerPool->GetLocalPlayerID()) continue;

                if (pPlayerPool && pPlayerPool->GetSlotState(pTextLabel->playerId)) {
                    CRemotePlayer *pPlayer = pPlayerPool->GetAt(pTextLabel->playerId);
                    if (pPlayer && pPlayer->GetDistanceFromLocalPlayer() < pTextLabel->fDistance) {
                        CPlayerPed *pPlayerPed = pPlayer->GetPlayerPed();
                        if (pPlayerPed && pPlayerPed->m_pPed->IsAdded()) {
                            CVector matBone;
                            pPlayerPed->GetBonePosition(8, &matBone);

                            vecTextPos.x = matBone.x + pTextLabel->vecPos.x;
                            vecTextPos.y = matBone.y + pTextLabel->vecPos.y;
                            vecTextPos.z = matBone.z + 0.23 + pTextLabel->vecPos.z;

                            this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
                                       pTextLabel->dwColor);

                        }
                    }
                }
            }
			if (pTextLabel->vehicleId != INVALID_VEHICLE_ID) {
				CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
				if (pVehiclePool && pVehiclePool->GetSlotState(pTextLabel->vehicleId)) {
					CVehicle *pVehicle = pVehiclePool->GetAt(pTextLabel->vehicleId);
					if (pVehicle && pVehicle->m_pVehicle->IsAdded() &&
						pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() < pTextLabel->fDistance) {
						RwMatrix matVehicle = pVehicle->m_pVehicle->GetMatrix().ToRwMatrix();

						vecTextPos.x = matVehicle.pos.x + pTextLabel->vecPos.x;
						vecTextPos.y = matVehicle.pos.y + pTextLabel->vecPos.y;
						vecTextPos.z = matVehicle.pos.z + pTextLabel->vecPos.z;

						this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
								   pTextLabel->dwColor);
					}
				}
			}

			if (pPlayerPed->m_pPed->GetDistanceFromPoint(pTextLabel->vecPos.x, pTextLabel->vecPos.y, pTextLabel->vecPos.z) <= pTextLabel->fDistance)
				this->Draw(renderer, pTextLabel, vecTextPos, pTextLabel->text,
						   pTextLabel->dwColor);
        }
	}
}

void C3DTextLabelPool::RenderNative(ImDrawList* draw, const ImVec2& displaySize)
{
	if (!draw || !pGame || !pNetGame) return;

	CPlayerPed* localPed = pGame->FindPlayerPed();
	if (!localPed || !localPed->m_pPed) return;

	CPlayerPool* playerPool = pNetGame->GetPlayerPool();
	CVehiclePool* vehiclePool = pNetGame->GetVehiclePool();
	if (!playerPool) return;

	for (int i = 0; i < MAX_TEXT_LABELS; ++i) {
		if (!m_bSlotUsed[i] || !m_TextLabels[i]) {
			continue;
		}

		TEXT_LABEL* label = m_TextLabels[i];
		CVector world = label->vecPos;
		bool shouldDraw = false;

		if (label->playerId != INVALID_PLAYER_ID) {
			if (label->playerId == playerPool->GetLocalPlayerID()) {
				continue;
			}

			if (!playerPool->GetSlotState(label->playerId)) {
				continue;
			}

			CRemotePlayer* remote = playerPool->GetAt(label->playerId);
			CPlayerPed* remotePed = remote ? remote->GetPlayerPed() : nullptr;
			if (!remote || !remotePed || !remotePed->m_pPed || !remotePed->m_pPed->IsAdded() ||
				remote->GetDistanceFromLocalPlayer() > label->fDistance) {
				continue;
			}

			CVector bone{};
			remotePed->GetBonePosition(8, &bone);
			world.x = bone.x + label->vecPos.x;
			world.y = bone.y + label->vecPos.y;
			world.z = bone.z + 0.23f + label->vecPos.z;
			shouldDraw = true;
		} else if (label->vehicleId != INVALID_VEHICLE_ID) {
			if (!vehiclePool || !vehiclePool->GetSlotState(label->vehicleId)) {
				continue;
			}

			CVehicle* vehicle = vehiclePool->GetAt(label->vehicleId);
			if (!vehicle || !vehicle->m_pVehicle || !vehicle->m_pVehicle->IsAdded() ||
				vehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() > label->fDistance) {
				continue;
			}

			RwMatrix matrix = vehicle->m_pVehicle->GetMatrix().ToRwMatrix();
			world.x = matrix.pos.x + label->vecPos.x;
			world.y = matrix.pos.y + label->vecPos.y;
			world.z = matrix.pos.z + label->vecPos.z;
			shouldDraw = true;
		} else if (localPed->m_pPed->GetDistanceFromPoint(
					   label->vecPos.x,
					   label->vecPos.y,
					   label->vecPos.z) <= label->fDistance) {
			shouldDraw = true;
		}

		if (shouldDraw) {
			DrawNative(draw, label, world, label->text, label->dwColor, displaySize);
		}
	}
}

void C3DTextLabelPool::Draw(ImGuiRenderer* renderer, TEXT_LABEL* label, CVector vecPos, const std::string& text, uint32_t dwColor)
{
	CVector vPos;
	vPos.x = vecPos.x;
	vPos.y = vecPos.y;
	vPos.z = vecPos.z;

    CCamera& TheCamera = GTASAEngineApi::Camera();

	int hitEntity = 0;
    if (label->bTestLOS) {
		CAMERA_AIM *pCam = GameGetInternalAim();
		if (!pCam)
		{
			return;
		}

        RwMatrix matPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->m_pPed->GetMatrix().ToRwMatrix();

		CVector vec;
		vec.x = pCam->pos1x;
		vec.y = pCam->pos1y;
		vec.z = pCam->pos1z;

		//bool isLineOfSightClear = ((bool (*)(CVector*, CVector*, int, int, int, int, int, int, int))(g_libGTASA + 0x423418 + 1))(&vec, &matPlayer.pos, 1, 0, 0, 1, 0, 0, 0);

        hitEntity = CWorld::GetIsLineOfSightClear(vecPos, TheCamera.GetPosition(), true, false, false, true, false, false, false);
		/*if(!isLineOfSightClear)
		{
			LOGI("labelpool draw ok no render fuck you bitch");
			return;
		}*/
    }

	if (!label->bTestLOS || hitEntity) {
		if (pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->m_pPed->GetDistanceFromPoint(vecPos.x, vecPos.y, vecPos.z) <= label->fDistance) {
			CVector vecOut;
			GTASAEngineApi::CalcScreenCoors(vPos, vecOut);
			if (vecOut.z < 1.0f) return;

			std::stringstream ss_data(text);
			std::string s_row;
			while (std::getline(ss_data, s_row, '\n')) {
				ImVec2 sz = renderer->calculateTextSize(s_row, UISettings::fontSize() / 2);
				renderer->drawText(ImVec2(vecOut.x - (sz.x / 2), vecOut.y),
								   __builtin_bswap32(dwColor | (0x000000FF)), s_row, true,
								   UISettings::fontSize() / 2);
				vecOut.y += UISettings::fontSize() / 2;
			}
		}
	}
}

void C3DTextLabelPool::DrawNative(ImDrawList* draw, TEXT_LABEL* label, CVector vecPos,
								  const std::string& text, uint32_t dwColor,
								  const ImVec2& displaySize)
{
	if (!draw || !label || text.empty() || !pNetGame || !pNetGame->GetPlayerPool()) {
		return;
	}

	CPlayerPed* localPed = pNetGame->GetPlayerPool()->GetLocalPlayer()
		? pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()
		: nullptr;
	if (!localPed || !localPed->m_pPed ||
		localPed->m_pPed->GetDistanceFromPoint(vecPos.x, vecPos.y, vecPos.z) > label->fDistance) {
		return;
	}

	CCamera& camera = GTASAEngineApi::Camera();
	if (label->bTestLOS &&
		!CWorld::GetIsLineOfSightClear(vecPos, camera.GetPosition(),
									   true, false, false, true, false, false, false)) {
		return;
	}

	ImVec2 screen{};
	if (!WorldToScreenTextLabel(vecPos, screen)) {
		return;
	}
	if (screen.x < -220.0f || screen.x > displaySize.x + 220.0f ||
		screen.y < -160.0f || screen.y > displaySize.y + 160.0f) {
		return;
	}

	ImFont* font = ImGui::GetFont();
	const float scale = std::max(0.75f, displaySize.y / 1080.0f);
	const float fontSize = 18.0f * scale;
	const float lineGap = 2.0f * scale;
	const ImU32 baseColor = SampTextColorToImU32(dwColor);

	const char* lineStart = text.c_str();
	const char* cursor = lineStart;
	float y = screen.y;
	while (*cursor) {
		if (*cursor == '\n') {
			DrawInlineLineCentered(draw, font, fontSize, ImVec2(screen.x, y), baseColor, lineStart, cursor);
			y += fontSize + lineGap;
			lineStart = cursor + 1;
		}
		++cursor;
	}

	if (lineStart < cursor) {
		DrawInlineLineCentered(draw, font, fontSize, ImVec2(screen.x, y), baseColor, lineStart, cursor);
	}
}
