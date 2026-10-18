#pragma once

class Spawn : public Layout
{
public:
	Spawn();

private:
	Button* m_buttonPrev = nullptr;
	Button* m_buttonSpawn = nullptr;
	Button* m_buttonNext = nullptr;
};
