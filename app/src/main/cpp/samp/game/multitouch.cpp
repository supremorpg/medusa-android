
#include "../main.h"
#include "multitouch.h"
#include "GTASAEngineApi.h"
#include "GTASAEngineNativeHookBindings.h"
#include "nv_event.h"
#include "../vendor/armhook/patch.h"
#include <string.h>

/* MultiTouch */

extern void AND_TouchEvent_hook(int type, int num, int posX, int posY);

void touch_event(int type, int num, int x, int y)
{
	// LOGI("touch_event: type %d | num %d | x %d | y %d", type, num, x, y);

	AND_TouchEvent_hook(type, num, x, y);
}

static void DispatchPointerTouch(int eventType, int pointerIndex, int x, int y)
{
    touch_event(eventType, pointerIndex, x, y);
}

int lastNvEvent;
#include "..//nv_event.h"
int32_t(*NVEventGetNextEvent_hooked)(NVEvent* ev, int waitMSecs);
int32_t NVEventGetNextEvent_hook(NVEvent* ev, int waitMSecs)
{
    if(!ev)
        return 0;

    int32_t ret = NVEventGetNextEvent_hooked(ev, waitMSecs);

    lastNvEvent =  ev->m_type;

    NVEvent event;
    if(NVEventGetNextEvent(&event))
    {
        int type = event.m_data.m_multi.m_action & NV_MULTITOUCH_ACTION_MASK;
        int num = (event.m_data.m_multi.m_action & NV_MULTITOUCH_POINTER_MASK) >> NV_MULTITOUCH_POINTER_SHIFT;

        int x1 = event.m_data.m_multi.m_x1;
        int y1 = event.m_data.m_multi.m_y1;

        int x2 = event.m_data.m_multi.m_x2;
        int y2 = event.m_data.m_multi.m_y2;

        int x3 = event.m_data.m_multi.m_x3;
        int y3 = event.m_data.m_multi.m_y3;

        if (type == NV_MULTITOUCH_CANCEL)
        {
            type = NV_MULTITOUCH_UP;
        }

        if ((x1 || y1) || num == 0)
        {
            if (num == 0 && type != NV_MULTITOUCH_MOVE)
            {
                DispatchPointerTouch(type, 0, x1, y1);
            }
            else
            {
                DispatchPointerTouch(NV_MULTITOUCH_MOVE, 0, x1, y1);
            }
        }

        if ((x2 || y2) || num == 1)
        {
            if (num == 1 && type != NV_MULTITOUCH_MOVE)
            {
                DispatchPointerTouch(type, 1, x2, y2);
            }
            else
            {
                DispatchPointerTouch(NV_MULTITOUCH_MOVE, 1, x2, y2);
            }
        }
        if ((x3 || y3) || num == 2)
        {
            if (num == 2 && type != NV_MULTITOUCH_MOVE)
            {
                DispatchPointerTouch(type, 2, x3, y3);
            }
            else
            {
                DispatchPointerTouch(NV_MULTITOUCH_MOVE, 2, x3, y3);
            }
        }
    }

    return ret;
}

int test_pointsArray[1000];
int test_pointersLibArray[1000];

void MultiTouch::initialize()
{
	LOGI("Initializing multi touch..");

    // 3 touch begin
    memset(test_pointsArray, 0, 999 * sizeof(int));

    memset(test_pointersLibArray, 0, 999 * sizeof(int));
    GTASAEngineApi::BindMultiTouchPointerArrays(&test_pointsArray, test_pointersLibArray);

    // 3 touch end
    GTASAEngineApi::PatchMultiTouchPointerLimits();

	// NVEventGetNextEvent
    CHook::InlineHook("_Z19NVEventGetNextEventP7NVEventi", NVEventGetNextEvent_hook, &NVEventGetNextEvent_hooked);
}
