#include "Limelight-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Exercise production SDP negotiation without starting a connection. The
// zero-initialized optional fields must leave the established record-framing
// advertisement intact without requesting compression or host pacing modes.
int AppVersionQuad[4] = {7, 1, 431, -1};
STREAM_CONFIGURATION StreamConfig;
CONNECTION_LISTENER_CALLBACKS ListenerCallbacks;
ConnListenerControllerSpeakerPcm ControllerSpeakerPcmCallback;
DECODER_RENDERER_CALLBACKS VideoCallbacks;
AUDIO_RENDERER_CALLBACKS AudioCallbacks;
int NegotiatedVideoFormat;
struct sockaddr_storage RemoteAddr;
uint16_t RtspPortNumber = 48010;
uint16_t VideoPortNumber = 47998;
uint32_t EncryptionFeaturesSupported;
uint32_t EncryptionFeaturesRequested;
uint32_t EncryptionFeaturesEnabled;
bool AudioEncryptionEnabled;
bool HighQualitySurroundSupported;
bool HighQualitySurroundEnabled;
int AudioPacketDuration;

bool PltSafeStrcpy(char* dest, size_t size, const char* src) {
    if (strlen(src) >= size) return false;
    memcpy(dest, src, strlen(src) + 1);
    return true;
}

void addrToUrlSafeString(struct sockaddr_storage* addr, char* string, size_t size) {
    (void)addr;
    if (!PltSafeStrcpy(string, size, "127.0.0.1")) abort();
}

bool isReferenceFrameInvalidationSupportedByDecoder(void) { return false; }

static int failures;

#define EXPECT(condition, description) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", description, __LINE__); \
        failures++; \
    } \
} while (0)

static char* generateSdp(int format) {
    int length = 0;
    memset(&StreamConfig, 0, sizeof(StreamConfig));
    StreamConfig.width = 2560;
    StreamConfig.height = 1440;
    StreamConfig.fps = 120;
    StreamConfig.bitrate = 800000;
    StreamConfig.packetSize = 1392;
    StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
    StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    NegotiatedVideoFormat = format;
    RemoteAddr.ss_family = AF_INET;
    AudioEncryptionEnabled = false;
    EncryptionFeaturesEnabled = 0;

    char* sdp = getSdpPayloadForStreamConfig(14, &length);
    EXPECT(sdp != NULL && length > 0, "SDP generation succeeds");
    return sdp;
}

int main(void) {
    const int pyrowaveFormats[] = {
        VIDEO_FORMAT_PYROWAVE, VIDEO_FORMAT_PYROWAVE_444,
        VIDEO_FORMAT_PYROWAVE_HDR10, VIDEO_FORMAT_PYROWAVE_HDR10_444
    };
    const int fallbackFormats[] = {VIDEO_FORMAT_H264, VIDEO_FORMAT_H265, VIDEO_FORMAT_AV1_MAIN8};
    unsigned int i;

    for (i = 0; i < sizeof(pyrowaveFormats) / sizeof(pyrowaveFormats[0]); i++) {
        char* normal = generateSdp(pyrowaveFormats[i]);
        if (normal == NULL) continue;
        EXPECT(strstr(normal, "a=x-ss-video[0].pyrowaveFeatures:1 \r\n") != NULL,
               "Default PyroWave advertises only record framing");
        EXPECT(strstr(normal, "a=x-ss-video[0].pyrowaveCompression") == NULL,
               "Zero compression preference does not request compression transport");
        EXPECT(strstr(normal, "a=x-ss-video[0].pyrowaveLinkMbps") == NULL,
               "Zero link estimate does not advertise a link speed");
        EXPECT(strstr(normal, "a=x-ss-video[0].pyrowavePaceMbps") == NULL,
               "Zero pace calibration does not request host pacing");
        free(normal);
    }

    for (i = 0; i < sizeof(fallbackFormats) / sizeof(fallbackFormats[0]); i++) {
        char* fallback = generateSdp(fallbackFormats[i]);
        if (fallback == NULL) continue;
        EXPECT(strstr(fallback, "pyrowaveFeatures") == NULL,
               "A non-PyroWave codec never advertises PyroWave features");
        EXPECT(strstr(fallback, "pyrowaveCompression") == NULL,
               "A non-PyroWave codec never advertises compression");
        free(fallback);
    }

    if (failures != 0) return 1;
    puts("PyroWave default SDP negotiation checks passed");
    return 0;
}
