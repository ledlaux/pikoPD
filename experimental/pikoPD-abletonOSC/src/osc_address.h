
#ifndef OSC_ADDRESS_H
#define OSC_ADDRESS_H

#include <stddef.h>
#include <string.h>

// =====================================================
// OSC ADDRESS PRESETS
// =====================================================

enum OscAddressPreset {
    OSC_PRESET_SONG,
    OSC_PRESET_TRACK,
    OSC_PRESET_CLIP,
    OSC_PRESET_SCENE,
    OSC_PRESET_DEVICE,
    OSC_PRESET_CLIP_SLOT,
    OSC_PRESET_CUSTOM
};

// Select preset here
#define OSC_ADDRESS_PRESET OSC_PRESET_SONG

// Change these elements to customize the address.
static const char* const OSC_CUSTOM_NAMESPACE[] = {
    "live",
    "song",
    "set"
};

// =====================================================
// ADDRESS BUILDER
// =====================================================

#define OSC_ADDRESS_MAX_LENGTH 128

inline bool build_osc_address(
    const char* parameter,
    char* output,
    size_t output_size)
{
    if (!parameter || !output || output_size < 2)
        return false;

    const char* const* path = nullptr;
    size_t count = 0;

    static const char* const song[] = {
        "live", "song", "set"
    };

    static const char* const track[] = {
        "live", "track", "set"
    };

    static const char* const clip[] = {
        "live", "clip", "set"
    };

    static const char* const scene[] = {
        "live", "scene", "set"
    };

    static const char* const device[] = {
        "live", "device", "set"
    };

    static const char* const clip_slot[] = {
        "live", "clip_slot"
    };

    switch (OSC_ADDRESS_PRESET) {
        case OSC_PRESET_SONG:
            path = song;
            count = 3;
            break;

        case OSC_PRESET_TRACK:
            path = track;
            count = 3;
            break;

        case OSC_PRESET_CLIP:
            path = clip;
            count = 3;
            break;

        case OSC_PRESET_SCENE:
            path = scene;
            count = 3;
            break;

        case OSC_PRESET_DEVICE:
            path = device;
            count = 3;
            break;

        case OSC_PRESET_CLIP_SLOT:
            path = clip_slot;
            count = 2;
            break;

        case OSC_PRESET_CUSTOM:
            path = OSC_CUSTOM_NAMESPACE;
            count = sizeof(OSC_CUSTOM_NAMESPACE) /
                    sizeof(OSC_CUSTOM_NAMESPACE[0]);
            break;

        default:
            return false;
    }

    size_t pos = 0;
    output[pos++] = '/';

    for (size_t i = 0; i < count; ++i) {
        size_t len = strlen(path[i]);

        if (pos + len + 1 >= output_size)
            return false;

        memcpy(output + pos, path[i], len);
        pos += len;
        output[pos++] = '/';
    }

    size_t len = strlen(parameter);

    if (!len || pos + len >= output_size)
        return false;

    memcpy(output + pos, parameter, len);
    pos += len;
    output[pos] = '\0';

    return true;
}

// =====================================================
// ACTION ADDRESS BUILDER
// =====================================================

inline bool build_osc_action_address(
    const char* action,
    char* output,
    size_t output_size)
{
    if (!action || !output || output_size < 2)
        return false;

    static const char* const live[] = { "live" };

    const char* const* path = live;
    size_t count = 1;

    size_t pos = 0;
    output[pos++] = '/';

    for (size_t i = 0; i < count; ++i) {
        size_t len = strlen(path[i]);

        if (pos + len + 1 >= output_size)
            return false;

        memcpy(output + pos, path[i], len);
        pos += len;
        output[pos++] = '/';
    }

    size_t len = strlen(action);

    if (!len || pos + len >= output_size)
        return false;

    memcpy(output + pos, action, len);
    pos += len;
    output[pos] = '\0';

    return true;
}

#endif
