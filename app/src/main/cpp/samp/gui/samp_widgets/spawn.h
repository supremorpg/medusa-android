#pragma once

#include "../imguirenderer.h"
#include "../widget.h"
#include "../widgets/label.h"
#include "../widgets/layout.h"
#include "../widgets/button.h"

class Spawn : public Layout
{
public:
	Spawn();

private:
	Button* m_buttonPrev = nullptr;
	Button* m_buttonSpawn = nullptr;
	Button* m_buttonNext = nullptr;
};
