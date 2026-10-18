#include "../../main.h"
#include "../gui.h"
#include "../../game/game.h"
#include "../../game/pad.h"
#include "../../net/netgame.h"
#include "../../net/localplayer.h"
#include "../../net/netgame.h"

extern UI* pUI;
extern CNetGame* pNetGame;
extern CGame *pGame;

bool bNeedEnterVehicle = false;
bool OpenButton = false;
int Tab = 0;
ButtonPanel::ButtonPanel()
	: Layout(Orientation::HORIZONTAL)
{
	m_bH = new CButton("H", UISettings::fontSize() / 2);
	m_bH->setVisible(false);
	this->addChild(m_bH);
	m_bAlt = nullptr;
	m_bY = nullptr;
	m_bN = nullptr;
	OpenButton = false;
}
