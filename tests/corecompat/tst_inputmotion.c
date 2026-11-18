// Exercise the production motion event entry point's argument validation.
// Type 0 used to pass a one-sided int comparison and index the sensor state at [-1].
#include "../../moonlight-common-c/moonlight-common-c/src/InputStream.c"

static int checks, failures;
static void check(int ok, const char* text) { ++checks; printf("%s %s\n", ok ? "PASS" : "FAIL", text); failures += !ok; }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    memcpy(AppVersionQuad, (int[]){7, 1, 431, 0}, sizeof(AppVersionQuad));
    SunshineFeatureFlags = LI_FF_CONTROLLER_TOUCH_EVENTS;
    if (initializeInputStream() != 0) return 2;
    initialized = true;

    check(LiSendControllerMotionEvent(0, 0, 1.0f, 2.0f, 3.0f) == -3, "motion type 0 is rejected");
    check(LiSendControllerMotionEvent(0, MAX_MOTION_EVENTS + 1, 1.0f, 2.0f, 3.0f) == -3, "motion type past the last is rejected");
    check(LiSendControllerMotionEvent(0, 255, 1.0f, 2.0f, 3.0f) == -3, "motion type 255 is rejected");
    check(LiSendControllerMotionEvent(3, LI_MOTION_TYPE_ACCEL, 1.0f, 2.0f, 3.0f) == 0, "accelerometer event is queued");
    check(LiSendControllerMotionEvent(3, LI_MOTION_TYPE_GYRO, 4.0f, 5.0f, 6.0f) == 0, "gyroscope event is queued");
    check(currentGamepadSensorState[3][LI_MOTION_TYPE_GYRO - 1].x == 4.0f &&
          currentGamepadSensorState[3][LI_MOTION_TYPE_ACCEL - 1].z == 3.0f, "events land in their own slots");
    check(currentGamepadSensorState[2][MAX_MOTION_EVENTS - 1].x == 0.0f, "the neighbouring controller's state is untouched");

    AppVersionQuad[3] = -1; // Sunshine carries the high half of the button word and supports 16 controllers.
    check(LiSendMultiControllerEvent(0, 1, 0x1000, 0, 0, 0, 0, 0, 0) == 0, "controller 0 is queued");
    check(LiSendMultiControllerEvent(15, 0x8000, 0, 0, 0, 0, 0, 0, 0) == 0, "controller 15 is queued");

    check(LiSendMultiControllerEvent(255, 0x8000, 0x2000, 0x35, 0x45, 101, 102, 103, 104) == 0,
          "controller byte 255 is queued");
    PPACKET_HOLDER wrapped15 = currentQueuedControllerPacket[15];
    check(wrapped15 && wrapped15->channelId == CTRL_CHANNEL_GAMEPAD_BASE + 15 &&
          wrapped15->packet.multiController.controllerNumber == LE16(15) &&
          (uint16_t)wrapped15->packet.multiController.activeGamepadMask == (uint16_t)LE16(0x8000) &&
          wrapped15->packet.multiController.leftTrigger == 0x35 &&
          wrapped15->packet.multiController.leftStickX == LE16(101),
          "controller byte 255 normalizes to controller 15 in the queued packet");

    check(LiSendMultiControllerEvent(240, 1, 0x2000, 0x35, 0x45, 201, 202, 203, 204) == 0,
          "controller byte 240 is queued");
    PPACKET_HOLDER wrapped0 = currentQueuedControllerPacket[0];
    check(wrapped0 && wrapped0 != wrapped15 && wrapped0->channelId == CTRL_CHANNEL_GAMEPAD_BASE &&
          wrapped0->packet.multiController.controllerNumber == LE16(0) &&
          wrapped0->packet.multiController.leftStickX == LE16(201),
          "controller byte 240 normalizes to controller 0 in the queued packet");

    check(LiSendMultiControllerEvent(16, 1, 0x2000, 0x35, 0x45, 301, 302, 303, 304) == 0,
          "controller byte 16 is queued");
    check(currentQueuedControllerPacket[0] == wrapped0 &&
          wrapped0->channelId == CTRL_CHANNEL_GAMEPAD_BASE &&
          wrapped0->packet.multiController.controllerNumber == LE16(0) &&
          wrapped0->packet.multiController.leftStickX == LE16(301),
          "controller byte 16 normalizes to 0 and updates the controller 0 batch");

    const int clicks = LEFT_PAD_CLICK_FLAG | RIGHT_TRIGGER_CLICK_FLAG;
    check(LiSendMultiControllerEvent(2, 4, clicks | TOUCHPAD_FLAG, 255, 0, 0, 0, 0, 0) == 0,
          "physical SC2026 clicks are queued");
    PPACKET_HOLDER pressed = currentQueuedControllerPacket[2];
    check(pressed && pressed->packet.multiController.buttonFlags2 == LE16((short)((clicks | TOUCHPAD_FLAG) >> 16)),
          "pad and trigger clicks survive the high button word");
    check(LiSendMultiControllerEvent(2, 4, 0, 255, 0, 0, 0, 0, 0) == 0 &&
          currentQueuedControllerPacket[2] != pressed && pressed->packet.multiController.buttonFlags2 != 0,
          "physical click release cannot coalesce away a press with unchanged analog triggers");

    initialized = false;
    // No sender thread is started by this fixture; close both queues explicitly.
    LbqSignalQueueShutdown(&packetHolderFreeList);
    LbqSignalQueueShutdown(&packetQueue);
    destroyInputStream();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
