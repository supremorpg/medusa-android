#pragma once

#include <cstddef>
#include <cstdint>

class CWidgetGta;

namespace Xyron::Input {

enum class SourceButton : uint8_t {
    Action = 0,
    Crouch,
    Fire,
    Sprint,
    SecondaryAttack,
    Jump,
    LookRight,
    Handbrake,
    LookLeft,
    Submission,
    Walk,
    AnalogUp,
    AnalogDown,
    AnalogLeft,
    AnalogRight,
    Yes,
    No,
    CtrlBack,
    Count
};

struct SourceHudState {
    bool firePressed = false;
    bool secondaryPressed = false;
    bool acceleratePressed = false;
    bool brakePressed = false;
    bool handbrakePressed = false;
    bool sprintPressed = false;
    bool jumpPressed = false;
    bool hornPressed = false;
    int16_t analogLeftRight = 0;
    int16_t analogUpDown = 0;
};

struct NativeInputFrameOptions {
    bool updatePads = true;
    bool clearTouchInterface = true;
    bool updateHid = true;
};

void ProcessNativeFrame(const NativeInputFrameOptions& options = {});
CWidgetGta** NativeTouchWidgets();
void BindNativeTouchWidgets(CWidgetGta*** target);
SourceHudState SnapshotHudState();
bool IsSourceInputOverrideEnabled();
bool IsSourceFirePressed();
bool IsSourceSecondaryPressed();
bool IsSourceAcceleratePressed();
bool IsSourceBrakePressed();
bool IsSourceHandbrakePressed();
bool IsSourceSprintPressed();
bool IsSourceSprintActionPressed();
bool IsSourceJumpPressed();
bool IsSourceHornPressed();
bool IsSourceCtrlBackPressed();
bool IsPassengerDriveByInputPressed();
int16_t SourceAnalogLeftRight();
int16_t SourceAnalogUpDown();
bool IsSourceButtonDown(SourceButton button);
bool IsCompatibilityButtonDown(SourceButton button);
void SetCompatibilityButton(SourceButton button, bool pressed);
void ClearCompatibilityButton(SourceButton button);
void ClearCompatibilityButtons(const SourceButton* buttons, std::size_t count);
void ClearAllCompatibilityButtons();
uint16_t CompatibilityAnalogLeftRight();
uint16_t CompatibilityAnalogUpDown();
void SetCompatibilityAnalogLeftRight(uint16_t value);
void SetCompatibilityAnalogUpDown(uint16_t value);
void SetCompatibilityAnalog(uint16_t leftRight, uint16_t upDown);
void ClearCompatibilityAnalog();
void ResetCompatibilityPadState();
bool IsJumpDown();
bool PressSourceButton(SourceButton button);
void SetHudActionState(int action, bool pressed);
void SetHudAnalogState(int leftRight, int upDown);
void AddCameraLookDelta(float deltaX, float deltaY, int screenWidth, int screenHeight);
void ApplySourceCameraLook();
bool IsSourceCameraLookActive();
bool IsSourceCameraLookRequested();
void RestoreCameraMatrixForGameplay();
void ResetSourceCameraLook();
void ResetHudControls();
void PressVehicleAction();
void PressVehicleLockAction();
void RequestPassengerVehicleAction();
bool HasPendingPassengerVehicleAction();
void ClearPassengerVehicleActionRequest();
bool CanProcessPassengerVehicleAction(uint32_t nowTick);
void MarkPassengerVehicleActionHandled(uint32_t nowTick);
bool ConsumeDriverEnterVehicleTaskStarted(bool taskActive);
bool ConsumeExitVehicleTaskStarted(bool taskActive);
void PressCameraAction();
bool ConsumeJumpJustPressed();
uint8_t ConsumeAdditionalKey();
uint16_t PackSampKeys(bool inVehicle, uint16_t* lrAnalog, uint16_t* udAnalog, bool clearMainButtons);

}
