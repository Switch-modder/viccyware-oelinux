/*
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_hw_utils"
/* #define LOG_NDEBUG 0 */

#include <inttypes.h>
#include <errno.h>
#include <cutils/properties.h>
#include <cutils/config_utils.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <cutils/str_parms.h>
#include <cutils/log.h>
#include <cutils/misc.h>


#include "audio_hw.h"
#include "platform.h"
#include "platform_api.h"
#include "audio_extn.h"
#include "voice.h"
#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <tinycompress/tinycompress.h>
#ifdef AUDIO_EXTERNAL_HDMI_ENABLED
#ifdef HDMI_PASSTHROUGH_ENABLED
#include "audio_parsers.h"
#endif
#endif

#ifdef LINUX_ENABLED
#define AUDIO_OUTPUT_POLICY_VENDOR_CONFIG_FILE "/etc/audio_output_policy.conf"
#define AUDIO_IO_POLICY_VENDOR_CONFIG_FILE "/etc/audio_io_policy.conf"
#else
#define AUDIO_OUTPUT_POLICY_VENDOR_CONFIG_FILE "/vendor/etc/audio_output_policy.conf"
#define AUDIO_IO_POLICY_VENDOR_CONFIG_FILE "/vendor/etc/audio_io_policy.conf"
#endif

#define OUTPUTS_TAG "outputs"
#define INPUTS_TAG "inputs"

#define DYNAMIC_VALUE_TAG "dynamic"
#define FLAGS_TAG "flags"
#define PROFILES_TAG "profile"
#define FORMATS_TAG "formats"
#define SAMPLING_RATES_TAG "sampling_rates"
#define BIT_WIDTH_TAG "bit_width"
#define APP_TYPE_TAG "app_type"

#define STRING_TO_ENUM(string) { #string, string }
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define BASE_TABLE_SIZE 64
#define MAX_BASEINDEX_LEN 256

#ifndef SND_AUDIOCODEC_TRUEHD
#define SND_AUDIOCODEC_TRUEHD 0x00000023
#endif

#ifdef AUDIO_EXTERNAL_HDMI_ENABLED
#define PROFESSIONAL        (1<<0)      /* 0 = consumer, 1 = professional */
#define NON_LPCM            (1<<1)      /* 0 = audio, 1 = non-audio */
#define SR_44100            (0<<0)      /* 44.1kHz */
#define SR_NOTID            (1<<0)      /* non indicated */
#define SR_48000            (2<<0)      /* 48kHz */
#define SR_32000            (3<<0)      /* 32kHz */
#define SR_22050            (4<<0)      /* 22.05kHz */
#define SR_24000            (6<<0)      /* 24kHz */
#define SR_88200            (8<<0)      /* 88.2kHz */
#define SR_96000            (10<<0)     /* 96kHz */
#define SR_176400           (12<<0)     /* 176.4kHz */
#define SR_192000           (14<<0)     /* 192kHz */

#endif

/* ToDo: Check and update a proper value in msec */
#define COMPRESS_OFFLOAD_PLAYBACK_LATENCY 50

#ifndef MAX_CHANNELS_SUPPORTED
#define MAX_CHANNELS_SUPPORTED 8
#endif

struct string_to_enum {
    const char *name;
    uint32_t value;
};

const struct string_to_enum s_flag_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_DIRECT),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_DIRECT_PCM),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_PRIMARY),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_FAST),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_RAW),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_DEEP_BUFFER),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_NON_BLOCKING),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_HW_AV_SYNC),
#ifdef INCALL_MUSIC_ENABLED
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_INCALL_MUSIC),
#endif
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_COMPRESS_PASSTHROUGH),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_TIMESTAMP),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_BD),
    STRING_TO_ENUM(AUDIO_OUTPUT_FLAG_INTERACTIVE),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_NONE),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_FAST),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_HW_HOTWORD),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_RAW),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_SYNC),
    STRING_TO_ENUM(AUDIO_INPUT_FLAG_TIMESTAMP),
};

const struct string_to_enum s_format_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_8_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_16_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_24_BIT_PACKED),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_8_24_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_32_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_MP3),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC),
    STRING_TO_ENUM(AUDIO_FORMAT_VORBIS),
    STRING_TO_ENUM(AUDIO_FORMAT_AMR_NB),
    STRING_TO_ENUM(AUDIO_FORMAT_AMR_WB),
    STRING_TO_ENUM(AUDIO_FORMAT_AC3),
    STRING_TO_ENUM(AUDIO_FORMAT_E_AC3),
    STRING_TO_ENUM(AUDIO_FORMAT_DTS),
    STRING_TO_ENUM(AUDIO_FORMAT_DTS_HD),
    STRING_TO_ENUM(AUDIO_FORMAT_DOLBY_TRUEHD),
    STRING_TO_ENUM(AUDIO_FORMAT_IEC61937),
#ifdef AUDIO_EXTN_FORMATS_ENABLED
    STRING_TO_ENUM(AUDIO_FORMAT_E_AC3_JOC),
    STRING_TO_ENUM(AUDIO_FORMAT_WMA),
    STRING_TO_ENUM(AUDIO_FORMAT_WMA_PRO),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ADIF),
    STRING_TO_ENUM(AUDIO_FORMAT_AMR_WB_PLUS),
    STRING_TO_ENUM(AUDIO_FORMAT_EVRC),
    STRING_TO_ENUM(AUDIO_FORMAT_EVRCB),
    STRING_TO_ENUM(AUDIO_FORMAT_EVRCWB),
    STRING_TO_ENUM(AUDIO_FORMAT_QCELP),
    STRING_TO_ENUM(AUDIO_FORMAT_MP2),
    STRING_TO_ENUM(AUDIO_FORMAT_EVRCNW),
    STRING_TO_ENUM(AUDIO_FORMAT_FLAC),
    STRING_TO_ENUM(AUDIO_FORMAT_ALAC),
    STRING_TO_ENUM(AUDIO_FORMAT_APE),
    STRING_TO_ENUM(AUDIO_FORMAT_E_AC3_JOC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_HE_V1),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_HE_V2),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ADTS),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ADTS_LC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ADTS_HE_V1),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ADTS_HE_V2),
    STRING_TO_ENUM(AUDIO_FORMAT_DSD),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LATM),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LATM_LC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LATM_HE_V1),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LATM_HE_V2),
    STRING_TO_ENUM(AUDIO_FORMAT_APTX),
#endif
};

/* payload structure avt_device drift query */
struct audio_avt_device_drift_stats {
    uint32_t       minor_version;
    /* Indicates the device interface direction as either
     * source (Tx) or sink (Rx).
    */
    uint16_t        device_direction;
    /*params exposed to client */
    struct audio_avt_device_drift_param drift_param;
};

static char bTable[BASE_TABLE_SIZE] = {
            'A','B','C','D','E','F','G','H','I','J','K','L',
            'M','N','O','P','Q','R','S','T','U','V','W','X',
            'Y','Z','a','b','c','d','e','f','g','h','i','j',
            'k','l','m','n','o','p','q','r','s','t','u','v',
            'w','x','y','z','0','1','2','3','4','5','6','7',
            '8','9','+','/'
};

static uint32_t string_to_enum(const struct string_to_enum *table, size_t size,
                               const char *name)
{
    size_t i;
    for (i = 0; i < size; i++) {
        if (strcmp(table[i].name, name) == 0) {
            ALOGV("%s found %s", __func__, table[i].name);
            return table[i].value;
        }
    }
    return 0;
}

static audio_io_flags_t parse_flag_names(char *name)
{
    uint32_t flag = 0;
    audio_io_flags_t io_flags;
    char *last_r;
    char *flag_name = strtok_r(name, "|", &last_r);
    while (flag_name != NULL) {
        if (strlen(flag_name) != 0) {
            flag |= string_to_enum(s_flag_name_to_enum_table,
                               ARRAY_SIZE(s_flag_name_to_enum_table),
                               flag_name);
        }
        flag_name = strtok_r(NULL, "|", &last_r);
    }

    ALOGV("parse_flag_names: flag - %x", flag);
    io_flags.in_flags = (audio_input_flags_t)flag;
    io_flags.out_flags = (audio_output_flags_t)flag;
    return io_flags;
}

static void parse_format_names(char *name, struct streams_io_cfg *s_info)
{
    struct stream_format *sf_info = NULL;
    char *last_r;
    char *str = strtok_r(name, "|", &last_r);

    if (str != NULL && strcmp(str, DYNAMIC_VALUE_TAG) == 0)
        return;

    list_init(&s_info->format_list);
    while (str != NULL) {
        audio_format_t format = (audio_format_t)string_to_enum(s_format_name_to_enum_table,
                                              ARRAY_SIZE(s_format_name_to_enum_table), str);
        ALOGV("%s: format - %d", __func__, format);
        if (format != 0) {
            sf_info = (struct stream_format *)calloc(1, sizeof(struct stream_format));
            if (sf_info == NULL)
                break; /* return whatever was parsed */

            sf_info->format = format;
            list_add_tail(&s_info->format_list, &sf_info->list);
        }
        str = strtok_r(NULL, "|", &last_r);
    }
}

static void parse_sample_rate_names(char *name, struct streams_io_cfg *s_info)
{
    struct stream_sample_rate *ss_info = NULL;
    uint32_t sample_rate = 48000;
    char *last_r;
    char *str = strtok_r(name, "|", &last_r);

    if (str != NULL && 0 == strcmp(str, DYNAMIC_VALUE_TAG))
        return;

    list_init(&s_info->sample_rate_list);
    while (str != NULL) {
        sample_rate = (uint32_t)strtol(str, (char **)NULL, 10);
        ALOGV("%s: sample_rate - %d", __func__, sample_rate);
        if (0 != sample_rate) {
            ss_info = (struct stream_sample_rate *)calloc(1, sizeof(struct stream_sample_rate));
            if (!ss_info) {
                ALOGE("%s: memory allocation failure", __func__);
                return;
            }
            ss_info->sample_rate = sample_rate;
            list_add_tail(&s_info->sample_rate_list, &ss_info->list);
        }
        str = strtok_r(NULL, "|", &last_r);
    }
}

static int parse_bit_width_names(char *name)
{
    int bit_width = 16;
    char *last_r;
    char *str = strtok_r(name, "|", &last_r);

    if (str != NULL && strcmp(str, DYNAMIC_VALUE_TAG))
        bit_width = (int)strtol(str, (char **)NULL, 10);

    ALOGV("%s: bit_width - %d", __func__, bit_width);
    return bit_width;
}

static int parse_app_type_names(void *platform, char *name)
{
    int app_type = platform_get_default_app_type(platform);
    char *last_r;
    char *str = strtok_r(name, "|", &last_r);

    if (str != NULL && strcmp(str, DYNAMIC_VALUE_TAG))
        app_type = (int)strtol(str, (char **)NULL, 10);

    ALOGV("%s: app_type - %d", __func__, app_type);
    return app_type;
}

static void update_streams_cfg_list(cnode *root, void *platform,
                                    struct listnode *streams_cfg_list)
{
    cnode *node = root->first_child;
    struct streams_io_cfg *s_info;

    ALOGV("%s", __func__);
    s_info = (struct streams_io_cfg *)calloc(1, sizeof(struct streams_io_cfg));

    if (!s_info) {
        ALOGE("failed to allocate mem for s_info list element");
        return;
    }

    while (node) {
        if (strcmp(node->name, FLAGS_TAG) == 0) {
            s_info->flags = parse_flag_names((char *)node->value);
        } else if (strcmp(node->name, PROFILES_TAG) == 0) {
            strlcpy(s_info->profile, (char *)node->value, sizeof(s_info->profile));
        } else if (strcmp(node->name, FORMATS_TAG) == 0) {
            parse_format_names((char *)node->value, s_info);
        } else if (strcmp(node->name, SAMPLING_RATES_TAG) == 0) {
            s_info->app_type_cfg.sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;
            parse_sample_rate_names((char *)node->value, s_info);
        } else if (strcmp(node->name, BIT_WIDTH_TAG) == 0) {
            s_info->app_type_cfg.bit_width = parse_bit_width_names((char *)node->value);
        } else if (strcmp(node->name, APP_TYPE_TAG) == 0) {
            s_info->app_type_cfg.app_type = parse_app_type_names(platform, (char *)node->value);
        }
        node = node->next;
    }
    list_add_tail(streams_cfg_list, &s_info->list);
}

static void load_cfg_list(cnode *root, void *platform,
                          struct listnode *streams_output_cfg_list,
                          struct listnode *streams_input_cfg_list)
{
    cnode *node = NULL;

    node = config_find(root, OUTPUTS_TAG);
    if (node != NULL) {
        node = node->first_child;
        while (node) {
            ALOGV("%s: loading output %s", __func__, node->name);
            update_streams_cfg_list(node, platform, streams_output_cfg_list);
            node = node->next;
        }
    } else {
        ALOGI("%s: could not load output, node is NULL", __func__);
    }

    node = config_find(root, INPUTS_TAG);
    if (node != NULL) {
        node = node->first_child;
        while (node) {
            ALOGV("%s: loading input %s", __func__, node->name);
            update_streams_cfg_list(node, platform, streams_input_cfg_list);
            node = node->next;
        }
    } else {
        ALOGI("%s: could not load input, node is NULL", __func__);
    }
}

static void send_app_type_cfg(void *platform, struct mixer *mixer,
                              struct listnode *streams_output_cfg_list,
                              struct listnode *streams_input_cfg_list)
{
    size_t app_type_cfg[MAX_LENGTH_MIXER_CONTROL_IN_INT] = {0};
    int length = 0, i, num_app_types = 0;
    struct listnode *node;
    bool update;
    struct mixer_ctl *ctl = NULL;
    const char *mixer_ctl_name = "App Type Config";
    struct streams_io_cfg *s_info = NULL;

    if (!mixer) {
        ALOGE("%s: mixer is null",__func__);
        return;
    }
    ctl = mixer_get_ctl_by_name(mixer, mixer_ctl_name);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",__func__, mixer_ctl_name);
        return;
    }
    app_type_cfg[length++] = num_app_types;

    if (list_empty(streams_output_cfg_list)) {
        app_type_cfg[length++] = platform_get_default_app_type_v2(platform, PCM_PLAYBACK);
        app_type_cfg[length++] = 48000;
        app_type_cfg[length++] = 16;
        num_app_types += 1;
    }
    if (list_empty(streams_input_cfg_list)) {
        app_type_cfg[length++] = platform_get_default_app_type_v2(platform, PCM_CAPTURE);
        app_type_cfg[length++] = 48000;
        app_type_cfg[length++] = 16;
        num_app_types += 1;
    }

    list_for_each(node, streams_output_cfg_list) {
        s_info = node_to_item(node, struct streams_io_cfg, list);
        update = true;
        for (i=0; i<length; i=i+3) {
            if (app_type_cfg[i+1] == 0)
                break;
            else if (app_type_cfg[i+1] == (size_t)s_info->app_type_cfg.app_type) {
                if (app_type_cfg[i+2] < (size_t)s_info->app_type_cfg.sample_rate)
                    app_type_cfg[i+2] = s_info->app_type_cfg.sample_rate;
                if (app_type_cfg[i+3] < (size_t)s_info->app_type_cfg.bit_width)
                    app_type_cfg[i+3] = s_info->app_type_cfg.bit_width;
                update = false;
                break;
            }
        }
        if (update && ((length + 3) <= MAX_LENGTH_MIXER_CONTROL_IN_INT)) {
            num_app_types += 1;
            app_type_cfg[length++] = s_info->app_type_cfg.app_type;
            app_type_cfg[length++] = s_info->app_type_cfg.sample_rate;
            app_type_cfg[length++] = s_info->app_type_cfg.bit_width;
        }
    }
    list_for_each(node, streams_input_cfg_list) {
        s_info = node_to_item(node, struct streams_io_cfg, list);
        update = true;
        for (i=0; i<length; i=i+3) {
            if (app_type_cfg[i+1] == 0)
                break;
            else if (app_type_cfg[i+1] == (size_t)s_info->app_type_cfg.app_type) {
                if (app_type_cfg[i+2] < (size_t)s_info->app_type_cfg.sample_rate)
                    app_type_cfg[i+2] = s_info->app_type_cfg.sample_rate;
                if (app_type_cfg[i+3] < (size_t)s_info->app_type_cfg.bit_width)
                    app_type_cfg[i+3] = s_info->app_type_cfg.bit_width;
                update = false;
                break;
            }
        }
        if (update && ((length + 3) <= MAX_LENGTH_MIXER_CONTROL_IN_INT)) {
            num_app_types += 1;
            app_type_cfg[length++] = s_info->app_type_cfg.app_type;
            app_type_cfg[length++] = s_info->app_type_cfg.sample_rate;
            app_type_cfg[length++] = s_info->app_type_cfg.bit_width;
        }
    }
    ALOGV("%s: num_app_types: %d", __func__, num_app_types);
    if (num_app_types) {
        app_type_cfg[0] = num_app_types;
        mixer_ctl_set_array(ctl, app_type_cfg, length);
    }
}

void audio_extn_utils_update_streams_cfg_lists(void *platform,
                                    struct mixer *mixer,
                                    struct listnode *streams_output_cfg_list,
                                    struct listnode *streams_input_cfg_list)
{
    cnode *root;
    char *data = NULL;

    ALOGV("%s", __func__);
    list_init(streams_output_cfg_list);
    list_init(streams_input_cfg_list);

    root = config_node("", "");
    if (root == NULL) {
        ALOGE("cfg_list, NULL config root");
        return;
    }

    data = (char *)load_file(AUDIO_IO_POLICY_VENDOR_CONFIG_FILE, NULL);
    if (data == NULL) {
        ALOGD("%s: failed to open io config file(%s), trying older config file",
              __func__, AUDIO_IO_POLICY_VENDOR_CONFIG_FILE);
        data = (char *)load_file(AUDIO_OUTPUT_POLICY_VENDOR_CONFIG_FILE, NULL);
        if (data == NULL) {
            send_app_type_cfg(platform, mixer,
                              streams_output_cfg_list,
                              streams_input_cfg_list);
            ALOGE("%s: could not load io policy config!", __func__);
            free(root);
            return;
        }
    }

    config_load(root, data);
    load_cfg_list(root, platform, streams_output_cfg_list,
                                  streams_input_cfg_list);

    send_app_type_cfg(platform, mixer, streams_output_cfg_list,
                                       streams_input_cfg_list);

    config_free(root);
    free(root);
    free(data);
}

static void audio_extn_utils_dump_streams_cfg_list(
                                    struct listnode *streams_cfg_list)
{
    struct listnode *node_i, *node_j;
    struct streams_io_cfg *s_info;
    struct stream_format *sf_info;
    struct stream_sample_rate *ss_info;

    list_for_each(node_i, streams_cfg_list) {
        s_info = node_to_item(node_i, struct streams_io_cfg, list);
        ALOGV("%s: flags-%d, sample_rate-%d, bit_width-%d, app_type-%d",
               __func__, s_info->flags.out_flags, s_info->app_type_cfg.sample_rate,
               s_info->app_type_cfg.bit_width, s_info->app_type_cfg.app_type);
        list_for_each(node_j, &s_info->format_list) {
            sf_info = node_to_item(node_j, struct stream_format, list);
            ALOGV("format-%x", sf_info->format);
        }
        list_for_each(node_j, &s_info->sample_rate_list) {
            ss_info = node_to_item(node_j, struct stream_sample_rate, list);
            ALOGV("sample rate-%d", ss_info->sample_rate);
        }
    }
}

void audio_extn_utils_dump_streams_cfg_lists(
                                    struct listnode *streams_output_cfg_list,
                                    struct listnode *streams_input_cfg_list)
{
    ALOGV("%s", __func__);
    audio_extn_utils_dump_streams_cfg_list(streams_output_cfg_list);
    audio_extn_utils_dump_streams_cfg_list(streams_input_cfg_list);
}

static void audio_extn_utils_release_streams_cfg_list(
                                    struct listnode *streams_cfg_list)
{
    struct listnode *node_i, *node_j;
    struct streams_io_cfg *s_info;

    ALOGV("%s", __func__);

    while (!list_empty(streams_cfg_list)) {
        node_i = list_head(streams_cfg_list);
        s_info = node_to_item(node_i, struct streams_io_cfg, list);
        while (!list_empty(&s_info->format_list)) {
            node_j = list_head(&s_info->format_list);
            list_remove(node_j);
            free(node_to_item(node_j, struct stream_format, list));
        }
        while (!list_empty(&s_info->sample_rate_list)) {
            node_j = list_head(&s_info->sample_rate_list);
            list_remove(node_j);
            free(node_to_item(node_j, struct stream_sample_rate, list));
        }
        list_remove(node_i);
        free(node_to_item(node_i, struct streams_io_cfg, list));
    }
}

void audio_extn_utils_release_streams_cfg_lists(
                                    struct listnode *streams_output_cfg_list,
                                    struct listnode *streams_input_cfg_list)
{
    ALOGV("%s", __func__);
    audio_extn_utils_release_streams_cfg_list(streams_output_cfg_list);
    audio_extn_utils_release_streams_cfg_list(streams_input_cfg_list);
}

static bool set_app_type_cfg(struct streams_io_cfg *s_info,
                    struct stream_app_type_cfg *app_type_cfg,
                    uint32_t sample_rate, uint32_t bit_width)
 {
    struct listnode *node_i;
    struct stream_sample_rate *ss_info;
    list_for_each(node_i, &s_info->sample_rate_list) {
        ss_info = node_to_item(node_i, struct stream_sample_rate, list);
        if ((sample_rate <= ss_info->sample_rate) &&
            (bit_width == s_info->app_type_cfg.bit_width)) {

            app_type_cfg->app_type = s_info->app_type_cfg.app_type;
            app_type_cfg->sample_rate = ss_info->sample_rate;
            app_type_cfg->bit_width = s_info->app_type_cfg.bit_width;
            ALOGV("%s app_type_cfg->app_type %d, app_type_cfg->sample_rate %d, app_type_cfg->bit_width %d",
                   __func__, app_type_cfg->app_type, app_type_cfg->sample_rate, app_type_cfg->bit_width);
            return true;
        }
    }
    /*
     * Reiterate through the list assuming dafault sample rate.
     * Handles scenario where input sample rate is higher
     * than all sample rates in list for the input bit width.
     */
    sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;

    list_for_each(node_i, &s_info->sample_rate_list) {
        ss_info = node_to_item(node_i, struct stream_sample_rate, list);
        if ((sample_rate <= ss_info->sample_rate) &&
            (bit_width == s_info->app_type_cfg.bit_width)) {
            app_type_cfg->app_type = s_info->app_type_cfg.app_type;
            app_type_cfg->sample_rate = sample_rate;
            app_type_cfg->bit_width = s_info->app_type_cfg.bit_width;
            ALOGV("%s Assuming sample rate. app_type_cfg->app_type %d, app_type_cfg->sample_rate %d, app_type_cfg->bit_width %d",
                   __func__, app_type_cfg->app_type, app_type_cfg->sample_rate, app_type_cfg->bit_width);
            return true;
        }
    }
    return false;
}

void audio_extn_utils_update_stream_input_app_type_cfg(void *platform,
                                  struct listnode *streams_input_cfg_list,
                                  audio_devices_t devices __unused,
                                  audio_input_flags_t flags,
                                  audio_format_t format,
                                  uint32_t sample_rate,
                                  uint32_t bit_width,
                                  char* profile,
                                  struct stream_app_type_cfg *app_type_cfg)
{
    struct listnode *node_i, *node_j;
    struct streams_io_cfg *s_info;
    struct stream_format *sf_info;

    ALOGV("%s: flags: 0x%x, format: 0x%x sample_rate %d, profile %s",
           __func__, flags, format, sample_rate, profile);

    list_for_each(node_i, streams_input_cfg_list) {
        s_info = node_to_item(node_i, struct streams_io_cfg, list);
        /* Along with flags do profile matching if set at either end.*/
        if (s_info->flags.in_flags == flags &&
            ((profile[0] == '\0' && s_info->profile[0] == '\0') ||
             strncmp(s_info->profile, profile, sizeof(s_info->profile)) == 0)) {
            list_for_each(node_j, &s_info->format_list) {
                sf_info = node_to_item(node_j, struct stream_format, list);
                if (sf_info->format == format) {
                    if (set_app_type_cfg(s_info, app_type_cfg, sample_rate, bit_width))
                        return;
                }
            }
        }
    }
    ALOGW("%s: App type could not be selected. Falling back to default", __func__);
    app_type_cfg->app_type = platform_get_default_app_type_v2(platform, PCM_CAPTURE);
    app_type_cfg->sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;
    app_type_cfg->bit_width = 16;
}

void audio_extn_utils_update_stream_output_app_type_cfg(void *platform,
                                  struct listnode *streams_output_cfg_list,
                                  audio_devices_t devices,
                                  audio_output_flags_t flags,
                                  audio_format_t format,
                                  uint32_t sample_rate,
                                  uint32_t bit_width,
                                  audio_channel_mask_t channel_mask,
                                  char *profile,
                                  struct stream_app_type_cfg *app_type_cfg)
{
    struct listnode *node_i, *node_j;
    struct streams_io_cfg *s_info;
    struct stream_format *sf_info;
    char value[PROPERTY_VALUE_MAX] = {0};

    if ((bit_width >= 24) &&
        (devices & AUDIO_DEVICE_OUT_SPEAKER)) {
        int32_t bw = platform_get_snd_device_bit_width(SND_DEVICE_OUT_SPEAKER);
        if (-ENOSYS != bw)
            bit_width = (uint32_t)bw;
        sample_rate = DEFAULT_OUTPUT_SAMPLING_RATE;
        ALOGI("%s Allowing 24-bit playback on speaker ONLY at default sampling rate", __func__);
    }

    property_get("audio.playback.mch.downsample",value,"");
    if (!strncmp("true", value, sizeof("true"))) {
        if ((popcount(channel_mask) > 2) &&
                (sample_rate > CODEC_BACKEND_DEFAULT_SAMPLE_RATE) &&
                !(flags & AUDIO_OUTPUT_FLAG_COMPRESS_PASSTHROUGH))  {
                    sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;
                    ALOGD("%s: MCH session defaulting sample rate to %d",
                               __func__, sample_rate);
        }
    }

    /* Set sampling rate to 176.4 for DSD64
     * and 352.8Khz for DSD128.
     * Set Bit Width to 16. output will be 16 bit
     * post DoP in ASM.
     */
    if ((flags & AUDIO_OUTPUT_FLAG_COMPRESS_PASSTHROUGH) &&
        (format == AUDIO_FORMAT_DSD)) {
        bit_width = 16;
        if (sample_rate == INPUT_SAMPLING_RATE_DSD64)
            sample_rate = OUTPUT_SAMPLING_RATE_DSD64;
        else if (sample_rate == INPUT_SAMPLING_RATE_DSD128)
            sample_rate = OUTPUT_SAMPLING_RATE_DSD128;
    }

    if(devices & AUDIO_DEVICE_OUT_ALL_A2DP) {
        //TODO: Handle fractional sampling rate configuration for LL
        audio_extn_a2dp_get_apptype_params(&sample_rate, &bit_width);
        ALOGI("%s using %d sampling rate %d bit width for A2DP CoPP",
              __func__, sample_rate, bit_width);
    }

    ALOGV("%s: flags: %x, format: %x sample_rate %d, profile %s, app_type %d",
           __func__, flags, format, sample_rate, profile, app_type_cfg->app_type);
    list_for_each(node_i, streams_output_cfg_list) {
        s_info = node_to_item(node_i, struct streams_io_cfg, list);
        /* Along with flags do profile matching if set at either end.*/
        if (s_info->flags.out_flags == flags &&
            ((profile[0] == '\0' && s_info->profile[0] == '\0') ||
             strncmp(s_info->profile, profile, sizeof(s_info->profile)) == 0)) {
            list_for_each(node_j, &s_info->format_list) {
                sf_info = node_to_item(node_j, struct stream_format, list);
                if (sf_info->format == format) {
                    if (set_app_type_cfg(s_info, app_type_cfg, sample_rate, bit_width))
                        return;
                }
            }
        }
    }
    list_for_each(node_i, streams_output_cfg_list) {
        s_info = node_to_item(node_i, struct streams_io_cfg, list);
        if (s_info->flags.out_flags == AUDIO_OUTPUT_FLAG_PRIMARY) {
            ALOGV("Compatible output profile not found.");
            app_type_cfg->app_type = s_info->app_type_cfg.app_type;
            app_type_cfg->sample_rate = s_info->app_type_cfg.sample_rate;
            app_type_cfg->bit_width = s_info->app_type_cfg.bit_width;
            ALOGV("%s Default to primary output: App type: %d sample_rate %d",
                  __func__, s_info->app_type_cfg.app_type, app_type_cfg->sample_rate);
            return;
        }
    }
    ALOGW("%s: App type could not be selected. Falling back to default", __func__);
    app_type_cfg->app_type = platform_get_default_app_type(platform);
    app_type_cfg->sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;
    app_type_cfg->bit_width = 16;
}

static bool audio_is_this_native_usecase(struct audio_usecase *uc)
{
    bool native_usecase = false;
    struct stream_out *out = (struct stream_out*) uc->stream.out;

    if (PCM_PLAYBACK == uc->type && out != NULL &&
        NATIVE_AUDIO_MODE_INVALID != platform_get_native_support() &&
        is_offload_usecase(uc->id) &&
        (out->sample_rate == OUTPUT_SAMPLING_RATE_44100))
        native_usecase = true;

    return native_usecase;
}


static inline bool audio_is_vr_mode_on(struct audio_device *(__attribute__((unused)) adev))
{
    return adev->vr_audio_mode_enabled;
}

void audio_extn_utils_update_stream_app_type_cfg_for_usecase(
                                    struct audio_device *adev,
                                    struct audio_usecase *usecase)
{
    ALOGV("%s", __func__);

    switch(usecase->type) {
    case PCM_PLAYBACK:
        audio_extn_utils_update_stream_output_app_type_cfg(adev->platform,
                                                &adev->streams_output_cfg_list,
                                                usecase->stream.out->devices,
                                                usecase->stream.out->flags,
                                                usecase->stream.out->format,
                                                usecase->stream.out->sample_rate,
                                                usecase->stream.out->bit_width,
                                                usecase->stream.out->channel_mask,
                                                usecase->stream.out->profile,
                                                &usecase->stream.out->app_type_cfg);
        ALOGV("%s Selected apptype: %d", __func__, usecase->stream.out->app_type_cfg.app_type);
        break;
    case PCM_CAPTURE:
        audio_extn_utils_update_stream_input_app_type_cfg(adev->platform,
                                                &adev->streams_input_cfg_list,
                                                usecase->stream.in->device,
                                                usecase->stream.in->flags,
                                                usecase->stream.in->format,
                                                usecase->stream.in->sample_rate,
                                                usecase->stream.in->bit_width,
                                                usecase->stream.in->profile,
                                                &usecase->stream.in->app_type_cfg);
        ALOGV("%s Selected apptype: %d", __func__, usecase->stream.in->app_type_cfg.app_type);
        break;
    case TRANSCODE_LOOPBACK :
        audio_extn_utils_update_stream_output_app_type_cfg(adev->platform,
                                                &adev->streams_output_cfg_list,
                                                usecase->stream.inout->out_config.devices,
                                                0,
                                                usecase->stream.inout->out_config.format,
                                                usecase->stream.inout->out_config.sample_rate,
                                                usecase->stream.inout->out_config.bit_width,
                                                usecase->stream.inout->out_config.channel_mask,
                                                usecase->stream.inout->profile,
                                                &usecase->stream.inout->out_app_type_cfg);
        ALOGV("%s Selected apptype: %d", __func__, usecase->stream.inout->out_app_type_cfg.app_type);
        break;
    default:
        ALOGE("%s: app type cfg not supported for usecase type (%d)",
            __func__, usecase->type);
    }
}

static int send_app_type_cfg_for_device(struct audio_device *adev,
                                        struct audio_usecase *usecase,
                                        int split_snd_device)
{
    char mixer_ctl_name[MAX_LENGTH_MIXER_CONTROL_IN_INT];
    size_t app_type_cfg[MAX_LENGTH_MIXER_CONTROL_IN_INT] = {0};
    int len = 0, rc;
    struct mixer_ctl *ctl;
    int pcm_device_id = 0, acdb_dev_id, app_type;
    int snd_device = split_snd_device, snd_device_be_idx = -1;
    int32_t sample_rate = DEFAULT_OUTPUT_SAMPLING_RATE;
    char value[PROPERTY_VALUE_MAX] = {0};
    struct streams_io_cfg *s_info = NULL;
    struct listnode *node = NULL;
    int direct_app_type = 0;

    ALOGV("%s: usecase->out_snd_device %s, usecase->in_snd_device %s, split_snd_device %s",
          __func__, platform_get_snd_device_name(usecase->out_snd_device),
          platform_get_snd_device_name(usecase->in_snd_device),
          platform_get_snd_device_name(split_snd_device));

    if (usecase->type != PCM_PLAYBACK && usecase->type != PCM_CAPTURE &&
        usecase->type != TRANSCODE_LOOPBACK) {
        ALOGE("%s: not a playback/capture path, no need to cfg app type", __func__);
        rc = 0;
        goto exit_send_app_type_cfg;
    }
    if ((usecase->id != USECASE_AUDIO_PLAYBACK_DEEP_BUFFER) &&
        (usecase->id != USECASE_AUDIO_PLAYBACK_LOW_LATENCY) &&
        (usecase->id != USECASE_AUDIO_PLAYBACK_MULTI_CH) &&
        (usecase->id != USECASE_AUDIO_PLAYBACK_ULL) &&
        (usecase->id != USECASE_AUDIO_TRANSCODE_LOOPBACK) &&
        (!is_interactive_usecase(usecase->id)) &&
        (!is_offload_usecase(usecase->id)) &&
        (usecase->type != PCM_CAPTURE)) {
        ALOGV("%s: a rx/tx/loopback path where app type cfg is not required %d", __func__, usecase->id);
        rc = 0;
        goto exit_send_app_type_cfg;
    }

    //if VR is active then only send the mixer control
    if (usecase->id == USECASE_AUDIO_PLAYBACK_ULL && !audio_is_vr_mode_on(adev)) {
            ALOGI("ULL doesnt need sending app type cfg, returning");
            rc = 0;
            goto exit_send_app_type_cfg;
    }

    if (usecase->type == PCM_PLAYBACK || usecase->type == TRANSCODE_LOOPBACK) {
        pcm_device_id = platform_get_pcm_device_id(usecase->id, PCM_PLAYBACK);
        snprintf(mixer_ctl_name, sizeof(mixer_ctl_name),
            "Audio Stream %d App Type Cfg", pcm_device_id);
    } else if (usecase->type == PCM_CAPTURE) {
        pcm_device_id = platform_get_pcm_device_id(usecase->id, PCM_CAPTURE);
        snprintf(mixer_ctl_name, sizeof(mixer_ctl_name),
            "Audio Stream Capture %d App Type Cfg", pcm_device_id);
    }

    ctl = mixer_get_ctl_by_name(adev->mixer, mixer_ctl_name);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s", __func__,
              mixer_ctl_name);
        rc = -EINVAL;
        goto exit_send_app_type_cfg;
    }
    snd_device = platform_get_spkr_prot_snd_device(snd_device);

    acdb_dev_id = platform_get_snd_device_acdb_id(snd_device);
    if (acdb_dev_id <= 0) {
        ALOGE("%s: Couldn't get the acdb dev id", __func__);
        rc = -EINVAL;
        goto exit_send_app_type_cfg;
    }

    snd_device_be_idx = platform_get_snd_device_backend_index(snd_device);
    if (snd_device_be_idx < 0) {
        ALOGE("%s: Couldn't get the backend index for snd device %s ret=%d",
              __func__, platform_get_snd_device_name(snd_device),
              snd_device_be_idx);
    }

    if ((usecase->type == PCM_PLAYBACK) && (usecase->stream.out != NULL)) {

        property_get("audio.playback.mch.downsample",value,"");
        if (!strncmp("true", value, sizeof("true"))) {
            if ((popcount(usecase->stream.out->channel_mask) > 2) &&
                   (usecase->stream.out->app_type_cfg.sample_rate > CODEC_BACKEND_DEFAULT_SAMPLE_RATE) &&
                   !(usecase->stream.out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_PASSTHROUGH))
               sample_rate = CODEC_BACKEND_DEFAULT_SAMPLE_RATE;
        }

        if (usecase->stream.out->devices & AUDIO_DEVICE_OUT_SPEAKER) {
            usecase->stream.out->app_type_cfg.sample_rate = DEFAULT_OUTPUT_SAMPLING_RATE;
        } else if ((snd_device == SND_DEVICE_OUT_HDMI ||
                    snd_device == SND_DEVICE_OUT_USB_HEADSET ||
                    snd_device == SND_DEVICE_OUT_DISPLAY_PORT) &&
                   (usecase->stream.out->sample_rate >= OUTPUT_SAMPLING_RATE_44100)) {
             /*
              * To best utlize DSP, check if the stream sample rate is supported/multiple of
              * configured device sample rate, if not update the COPP rate to be equal to the
              * device sample rate, else open COPP at stream sample rate
              */
              platform_check_and_update_copp_sample_rate(adev->platform, snd_device,
                                      usecase->stream.out->sample_rate,
                                      &usecase->stream.out->app_type_cfg.sample_rate);
        } else if (((snd_device != SND_DEVICE_OUT_HEADPHONES_44_1 &&
                     !audio_is_this_native_usecase(usecase)) &&
            usecase->stream.out->sample_rate == OUTPUT_SAMPLING_RATE_44100) ||
            (usecase->stream.out->sample_rate < OUTPUT_SAMPLING_RATE_44100)) {
            /* Reset to default if no native stream is active*/
            usecase->stream.out->app_type_cfg.sample_rate = DEFAULT_OUTPUT_SAMPLING_RATE;
        }
        sample_rate = usecase->stream.out->app_type_cfg.sample_rate;

        /* Interactive streams are supported with only direct app type id.
         * Get Direct profile app type and use it for interactive streams
         */
        list_for_each(node, &adev->streams_output_cfg_list) {
            s_info = node_to_item(node, struct streams_io_cfg, list);
            if (s_info->flags.out_flags == AUDIO_OUTPUT_FLAG_DIRECT)
                direct_app_type = s_info->app_type_cfg.app_type;
        }
        if (usecase->stream.out->flags == AUDIO_OUTPUT_FLAG_INTERACTIVE)
            app_type = direct_app_type;
        else
            app_type = usecase->stream.out->app_type_cfg.app_type;
        app_type_cfg[len++] = app_type;
        app_type_cfg[len++] = acdb_dev_id;
        if (((usecase->stream.out->format == AUDIO_FORMAT_E_AC3) ||
            (usecase->stream.out->format == AUDIO_FORMAT_E_AC3_JOC) ||
            (usecase->stream.out->format == AUDIO_FORMAT_DOLBY_TRUEHD))
            && audio_extn_passthru_is_passthrough_stream(usecase->stream.out)
            && !audio_extn_passthru_is_convert_supported(adev, usecase->stream.out)) {

            sample_rate = sample_rate * 4;
            if (sample_rate > HDMI_PASSTHROUGH_MAX_SAMPLE_RATE)
                sample_rate = HDMI_PASSTHROUGH_MAX_SAMPLE_RATE;
        }
        app_type_cfg[len++] = sample_rate;

        if (snd_device_be_idx > 0)
            app_type_cfg[len++] = snd_device_be_idx;

         ALOGI("%s PLAYBACK app_type %d, acdb_dev_id %d, sample_rate %d, snd_device_be_idx %d",
             __func__, app_type, acdb_dev_id, sample_rate, snd_device_be_idx);

    } else if ((usecase->type == PCM_CAPTURE) && (usecase->stream.in != NULL)) {
        app_type = usecase->stream.in->app_type_cfg.app_type;
        app_type_cfg[len++] = app_type;
        app_type_cfg[len++] = acdb_dev_id;
        sample_rate = usecase->stream.in->app_type_cfg.sample_rate;
        app_type_cfg[len++] = sample_rate;
        if (snd_device_be_idx > 0)
            app_type_cfg[len++] = snd_device_be_idx;
        ALOGI("%s CAPTURE app_type %d, acdb_dev_id %d, sample_rate %d, snd_device_be_idx %d",
           __func__, app_type, acdb_dev_id, sample_rate, snd_device_be_idx);
    } else {
        app_type = platform_get_default_app_type_v2(adev->platform, usecase->type);
        if(usecase->type == TRANSCODE_LOOPBACK) {
            sample_rate = usecase->stream.inout->out_config.sample_rate;
            app_type = usecase->stream.inout->out_app_type_cfg.app_type;
        }
        app_type_cfg[len++] = app_type;
        app_type_cfg[len++] = acdb_dev_id;
        app_type_cfg[len++] = sample_rate;
        if (snd_device_be_idx > 0)
            app_type_cfg[len++] = snd_device_be_idx;
        ALOGI("%s default app_type %d, acdb_dev_id %d, sample_rate %d, snd_device_be_idx %d",
              __func__, app_type, acdb_dev_id, sample_rate, snd_device_be_idx);
    }

    if(ctl)
        mixer_ctl_set_array(ctl, app_type_cfg, len);
    rc = 0;
exit_send_app_type_cfg:
    return rc;
}

int audio_extn_utils_send_app_type_cfg(struct audio_device *adev,
                                       struct audio_usecase *usecase)
{
    int i, num_devices = 0;
    snd_device_t new_snd_devices[SND_DEVICE_OUT_END] = {0};
    int rc = 0;

    switch (usecase->type) {
    case PCM_PLAYBACK:
    case TRANSCODE_LOOPBACK:
        ALOGD("%s: usecase->out_snd_device %s",
              __func__, platform_get_snd_device_name(usecase->out_snd_device));
        /* check for out combo device */
        if (platform_split_snd_device(adev->platform,
                                      usecase->out_snd_device,
                                      &num_devices, new_snd_devices)) {
            new_snd_devices[0] = usecase->out_snd_device;
            num_devices = 1;
        }
        break;
    case PCM_CAPTURE:
        ALOGD("%s: usecase->in_snd_device %s",
              __func__, platform_get_snd_device_name(usecase->in_snd_device));
        /* check for in combo device */
        if (platform_split_snd_device(adev->platform,
                                      usecase->in_snd_device,
                                      &num_devices, new_snd_devices)) {
            new_snd_devices[0] = usecase->in_snd_device;
            num_devices = 1;
        }
        break;
    default:
        ALOGI("%s: not a playback/capture path, no need to cfg app type", __func__);
        rc = 0;
        break;
    }

    for (i = 0; i < num_devices; i++) {
        rc = send_app_type_cfg_for_device(adev, usecase, new_snd_devices[i]);
        if (rc)
            break;
    }

    return rc;
}

int read_line_from_file(const char *path, char *buf, size_t count)
{
    char * fgets_ret;
    FILE * fd;
    int rv;

    fd = fopen(path, "r");
    if (fd == NULL)
        return -1;

    fgets_ret = fgets(buf, (int)count, fd);
    if (NULL != fgets_ret) {
        rv = (int)strlen(buf);
    } else {
        rv = ferror(fd);
    }
    fclose(fd);

   return rv;
}

/*Translates ALSA formats to AOSP PCM formats*/
audio_format_t alsa_format_to_hal(uint32_t alsa_format)
{
    audio_format_t format;

    switch(alsa_format) {
    case SNDRV_PCM_FORMAT_S16_LE:
        format = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case SNDRV_PCM_FORMAT_S24_3LE:
        format = AUDIO_FORMAT_PCM_24_BIT_PACKED;
        break;
    case SNDRV_PCM_FORMAT_S24_LE:
        format = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    case SNDRV_PCM_FORMAT_S32_LE:
        format = AUDIO_FORMAT_PCM_32_BIT;
        break;
    default:
        ALOGW("Incorrect ALSA format");
        format = AUDIO_FORMAT_INVALID;
    }
    return format;
}

/*Translates hal format (AOSP) to alsa formats*/
uint32_t hal_format_to_alsa(audio_format_t hal_format)
{
    uint32_t alsa_format;

    switch (hal_format) {
    case AUDIO_FORMAT_PCM_32_BIT: {
        if (platform_supports_true_32bit())
            alsa_format = SNDRV_PCM_FORMAT_S32_LE;
        else
            alsa_format = SNDRV_PCM_FORMAT_S24_3LE;
        }
        break;
    case AUDIO_FORMAT_PCM_8_BIT:
        alsa_format = SNDRV_PCM_FORMAT_S8;
        break;
    case AUDIO_FORMAT_PCM_24_BIT_PACKED:
        alsa_format = SNDRV_PCM_FORMAT_S24_3LE;
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT: {
        if (platform_supports_true_32bit())
            alsa_format = SNDRV_PCM_FORMAT_S32_LE;
        else
            alsa_format = SNDRV_PCM_FORMAT_S24_3LE;
        }
        break;
    case AUDIO_FORMAT_PCM_FLOAT:
        alsa_format = SNDRV_PCM_FORMAT_S24_3LE;
        break;
    default:
    case AUDIO_FORMAT_PCM_16_BIT:
        alsa_format = SNDRV_PCM_FORMAT_S16_LE;
        break;
    }
    return alsa_format;
}

/*Translates PCM formats to AOSP formats*/
audio_format_t pcm_format_to_hal(uint32_t pcm_format)
{
    audio_format_t format = AUDIO_FORMAT_INVALID;

    switch(pcm_format) {
    case PCM_FORMAT_S16_LE:
        format = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case PCM_FORMAT_S24_3LE:
        format = AUDIO_FORMAT_PCM_24_BIT_PACKED;
        break;
    case PCM_FORMAT_S24_LE:
        format = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    case PCM_FORMAT_S32_LE:
        format = AUDIO_FORMAT_PCM_32_BIT;
        break;
    default:
        ALOGW("Incorrect PCM format");
        format = AUDIO_FORMAT_INVALID;
    }
    return format;
}

/*Translates hal format (AOSP) to alsa formats*/
uint32_t hal_format_to_pcm(audio_format_t hal_format)
{
    uint32_t pcm_format;

    switch (hal_format) {
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_FLOAT: {
        if (platform_supports_true_32bit())
            pcm_format = PCM_FORMAT_S32_LE;
        else
            pcm_format = PCM_FORMAT_S24_3LE;
        }
        break;
    case AUDIO_FORMAT_PCM_8_BIT:
        pcm_format = PCM_FORMAT_S8;
        break;
    case AUDIO_FORMAT_PCM_24_BIT_PACKED:
        pcm_format = PCM_FORMAT_S24_3LE;
        break;
    default:
    case AUDIO_FORMAT_PCM_16_BIT:
        pcm_format = PCM_FORMAT_S16_LE;
        break;
    }
    return pcm_format;
}

uint32_t get_alsa_fragment_size(uint32_t bytes_per_sample,
                                  uint32_t sample_rate,
                                  uint32_t noOfChannels)
{
    uint32_t fragment_size = 0;
    uint32_t pcm_offload_time = PCM_OFFLOAD_BUFFER_DURATION;

    fragment_size = (pcm_offload_time
                     * sample_rate
                     * bytes_per_sample
                     * noOfChannels)/1000;
    if (fragment_size < MIN_PCM_OFFLOAD_FRAGMENT_SIZE)
        fragment_size = MIN_PCM_OFFLOAD_FRAGMENT_SIZE;
    else if (fragment_size > MAX_PCM_OFFLOAD_FRAGMENT_SIZE)
        fragment_size = MAX_PCM_OFFLOAD_FRAGMENT_SIZE;
    /*To have same PCM samples for all channels, the buffer size requires to
     *be multiple of (number of channels * bytes per sample)
     *For writes to succeed, the buffer must be written at address which is multiple of 32
     */
    fragment_size = ALIGN(fragment_size, (bytes_per_sample * noOfChannels * 32));

    ALOGI("PCM offload Fragment size to %d bytes", fragment_size);
    return fragment_size;
}

/* Calculates the fragment size required to configure compress session.
 * Based on the alsa format selected, decide if conversion is needed in

 * HAL ( e.g. convert AUDIO_FORMAT_PCM_FLOAT input format to
 * AUDIO_FORMAT_PCM_24_BIT_PACKED before writing to the compress driver.
 */
void audio_extn_utils_update_direct_pcm_fragment_size(struct stream_out *out)
{
    audio_format_t dst_format = out->hal_op_format;
    audio_format_t src_format = out->hal_ip_format;
    uint32_t hal_op_bytes_per_sample = audio_bytes_per_sample(dst_format);
    uint32_t hal_ip_bytes_per_sample = audio_bytes_per_sample(src_format);

    out->compr_config.fragment_size =
             get_alsa_fragment_size(hal_op_bytes_per_sample,
                                      out->sample_rate,
                                      popcount(out->channel_mask));

    if ((src_format != dst_format) &&
         hal_op_bytes_per_sample != hal_ip_bytes_per_sample) {

        out->hal_fragment_size =
                  ((out->compr_config.fragment_size * hal_ip_bytes_per_sample) /
                   hal_op_bytes_per_sample);
        ALOGI("enable conversion hal_input_fragment_size is %d src_format %x dst_format %x",
               out->hal_fragment_size, src_format, dst_format);
    } else {
        out->hal_fragment_size = out->compr_config.fragment_size;
    }
}

/* converts pcm format 24_8 to 8_24 inplace */
size_t audio_extn_utils_convert_format_24_8_to_8_24(void *buf, size_t bytes)
{
    size_t i = 0;
    int *int_buf_stream = buf;

    if ((bytes % 4) != 0) {
        ALOGE("%s: wrong inout buffer! ... is not 32 bit aligned ", __func__);
        return -EINVAL;
    }

    for (; i < (bytes / 4); i++)
        int_buf_stream[i] >>= 8;

    return bytes;
}

int get_snd_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format & AUDIO_FORMAT_MAIN_MASK) {
    case AUDIO_FORMAT_MP3:
        id = SND_AUDIOCODEC_MP3;
        break;
    case AUDIO_FORMAT_AAC:
        id = SND_AUDIOCODEC_AAC;
        break;
    case AUDIO_FORMAT_AAC_ADTS:
        id = SND_AUDIOCODEC_AAC;
        break;
    case AUDIO_FORMAT_AAC_LATM:
        id = SND_AUDIOCODEC_AAC;
        break;
    case AUDIO_FORMAT_PCM_OFFLOAD:
    case AUDIO_FORMAT_PCM:
        id = SND_AUDIOCODEC_PCM;
        break;
    case AUDIO_FORMAT_FLAC:
        id = SND_AUDIOCODEC_FLAC;
        break;
    case AUDIO_FORMAT_ALAC:
        id = SND_AUDIOCODEC_ALAC;
        break;
    case AUDIO_FORMAT_APE:
        id = SND_AUDIOCODEC_APE;
        break;
    case AUDIO_FORMAT_VORBIS:
        id = SND_AUDIOCODEC_VORBIS;
        break;
    case AUDIO_FORMAT_WMA:
        id = SND_AUDIOCODEC_WMA;
        break;
    case AUDIO_FORMAT_WMA_PRO:
        id = SND_AUDIOCODEC_WMA_PRO;
        break;
    case AUDIO_FORMAT_MP2:
        id = SND_AUDIOCODEC_MP2;
        break;
    case AUDIO_FORMAT_AC3:
        id = SND_AUDIOCODEC_AC3;
        break;
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_E_AC3_JOC:
        id = SND_AUDIOCODEC_EAC3;
        break;
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
        id = SND_AUDIOCODEC_DTS;
        break;
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        id = SND_AUDIOCODEC_TRUEHD;
        break;
    case AUDIO_FORMAT_IEC61937:
        id = SND_AUDIOCODEC_IEC61937;
        break;
    case AUDIO_FORMAT_DSD:
        id = SND_AUDIOCODEC_DSD;
        break;
    case AUDIO_FORMAT_APTX:
        id = SND_AUDIOCODEC_APTX;
        break;
    default:
        ALOGE("%s: Unsupported audio format :%x", __func__, format);
    }

    return id;
}

void audio_extn_utils_send_audio_calibration(struct audio_device *adev,
                                             struct audio_usecase *usecase)
{
    int type = usecase->type;

    if (type == PCM_PLAYBACK && usecase->stream.out != NULL) {
        struct stream_out *out = usecase->stream.out;
        int snd_device = usecase->out_snd_device;
        snd_device = (snd_device == SND_DEVICE_OUT_SPEAKER) ?
                     platform_get_spkr_prot_snd_device(snd_device) : snd_device;
        platform_send_audio_calibration(adev->platform, usecase,
                                        out->app_type_cfg.app_type,
                                        usecase->stream.out->app_type_cfg.sample_rate);
    } else if (type == PCM_CAPTURE && usecase->stream.in != NULL) {
        platform_send_audio_calibration(adev->platform, usecase,
                         usecase->stream.in->app_type_cfg.app_type,
                         usecase->stream.in->app_type_cfg.sample_rate);
    } else if (type == PCM_HFP_CALL || type == PCM_CAPTURE) {
        /* when app type is default. the sample rate is not used to send cal */
        platform_send_audio_calibration(adev->platform, usecase,
                         platform_get_default_app_type_v2(adev->platform, usecase->type),
                         48000);
    } else if (type == TRANSCODE_LOOPBACK && usecase->stream.inout != NULL) {
        int snd_device = usecase->out_snd_device;
        snd_device = (snd_device == SND_DEVICE_OUT_SPEAKER) ?
                     platform_get_spkr_prot_snd_device(snd_device) : snd_device;
        platform_send_audio_calibration(adev->platform, usecase,
                         platform_get_default_app_type_v2(adev->platform, usecase->type),
                         usecase->stream.inout->out_config.sample_rate);
    } else {
        /* No need to send audio calibration for voice and voip call usecases */
        if ((type != VOICE_CALL) && (type != VOIP_CALL))
            ALOGW("%s: No audio calibration for usecase type = %d",  __func__, type);
    }
}

// Base64 Encode and Decode
// Not all features supported. This must be used only with following conditions.
// Decode Modes: Support with and without padding
//         CRLF not handling. So no CRLF in string to decode.
// Encode Modes: Supports only padding
int b64decode(char *inp, int ilen, uint8_t* outp)
{
    int i, j, k, ii, num;
    int rem, pcnt;
    uint32_t res=0;
    uint8_t getIndex[MAX_BASEINDEX_LEN];
    uint8_t tmp, cflag;

    if(inp == NULL || outp == NULL || ilen <= 0) {
        ALOGE("[%s] received NULL pointer or zero length",__func__);
        return -1;
    }

    memset(getIndex, MAX_BASEINDEX_LEN-1, sizeof(getIndex));
    for(i=0;i<BASE_TABLE_SIZE;i++) {
        getIndex[(uint8_t)bTable[i]] = (uint8_t)i;
    }
    getIndex[(uint8_t)'=']=0;

    j=0;k=0;
    num = ilen/4;
    rem = ilen%4;
    if(rem==0)
        num = num-1;
    cflag=0;
    for(i=0; i<num; i++) {
        res=0;
        for(ii=0;ii<4;ii++) {
            res = res << 6;
            tmp = getIndex[(uint8_t)inp[j++]];
            res = res | tmp;
            cflag = cflag | tmp;
        }
        outp[k++] = (res >> 16)&0xFF;
        outp[k++] = (res >> 8)&0xFF;
        outp[k++] = res & 0xFF;
    }

    // Handle last bytes special
    pcnt=0;
    if(rem == 0) {
        //With padding or full data
        res = 0;
        for(ii=0;ii<4;ii++) {
            if(inp[j] == '=')
                pcnt++;
            res = res << 6;
            tmp = getIndex[(uint8_t)inp[j++]];
            res = res | tmp;
            cflag = cflag | tmp;
        }
        outp[k++] = res >> 16;
        if(pcnt == 2)
            goto done;
        outp[k++] = (res>>8)&0xFF;
        if(pcnt == 1)
            goto done;
        outp[k++] = res&0xFF;
    } else {
        //without padding
        res = 0;
        for(i=0;i<rem;i++) {
            res = res << 6;
            tmp = getIndex[(uint8_t)inp[j++]];
            res = res | tmp;
            cflag = cflag | tmp;
        }
        for(i=rem;i<4;i++) {
            res = res << 6;
            pcnt++;
        }
        outp[k++] = res >> 16;
        if(pcnt == 2)
            goto done;
        outp[k++] = (res>>8)&0xFF;
        if(pcnt == 1)
            goto done;
        outp[k++] = res&0xFF;
    }
done:
    if(cflag == 0xFF) {
        ALOGE("[%s] base64 decode failed. Invalid character found %s",
            __func__, inp);
        return 0;
    }
    return k;
}

int b64encode(uint8_t *inp, int ilen, char* outp)
{
    int i,j,k, num;
    int rem=0;
    uint32_t res=0;

    if(inp == NULL || outp == NULL || ilen<=0) {
        ALOGE("[%s] received NULL pointer or zero input length",__func__);
        return -1;
    }

    num = ilen/3;
    rem = ilen%3;
    j=0;k=0;
    for(i=0; i<num; i++) {
        //prepare index
        res = inp[j++]<<16;
        res = res | inp[j++]<<8;
        res = res | inp[j++];
        //get output map from index
        outp[k++] = (char) bTable[(res>>18)&0x3F];
        outp[k++] = (char) bTable[(res>>12)&0x3F];
        outp[k++] = (char) bTable[(res>>6)&0x3F];
        outp[k++] = (char) bTable[res&0x3F];
    }

    switch(rem) {
        case 1:
            res = inp[j++]<<16;
            outp[k++] = (char) bTable[res>>18];
            outp[k++] = (char) bTable[(res>>12)&0x3F];
            //outp[k++] = '=';
            //outp[k++] = '=';
            break;
        case 2:
            res = inp[j++]<<16;
            res = res | inp[j++]<<8;
            outp[k++] = (char) bTable[res>>18];
            outp[k++] = (char) bTable[(res>>12)&0x3F];
            outp[k++] = (char) bTable[(res>>6)&0x3F];
            //outp[k++] = '=';
            break;
        default:
            break;
    }
    outp[k] = '\0';
    return k;
}


int audio_extn_utils_get_codec_version(const char *snd_card_name,
                            int card_num,
                            char *codec_version)
{
    char procfs_path[50];
    FILE *fp;

    if (strstr(snd_card_name, "tasha")) {
        snprintf(procfs_path, sizeof(procfs_path),
                 "/proc/asound/card%d/codecs/tasha/version", card_num);
        if ((fp = fopen(procfs_path, "r")) != NULL) {
            fgets(codec_version, CODEC_VERSION_MAX_LENGTH, fp);
            fclose(fp);
        } else {
            ALOGE("%s: ERROR. cannot open %s", __func__, procfs_path);
            return -ENOENT;
        }
        ALOGD("%s: codec version %s", __func__, codec_version);
    }

    return 0;
}


#ifdef AUDIO_EXTERNAL_HDMI_ENABLED

void get_default_compressed_channel_status(
                                  unsigned char *channel_status)
{
     memset(channel_status,0,24);

     /* block start bit in preamble bit 3 */
     channel_status[0] |= PROFESSIONAL;
     //compre out
     channel_status[0] |= NON_LPCM;
     // sample rate; fixed 48K for default/transcode
     channel_status[3] |= SR_48000;
}

#ifdef HDMI_PASSTHROUGH_ENABLED
int32_t get_compressed_channel_status(void *audio_stream_data,
                                                   uint32_t audio_frame_size,
                                                   unsigned char *channel_status,
                                                   enum audio_parser_code_type codec_type)
                                                   // codec_type - AUDIO_PARSER_CODEC_AC3
                                                   //            - AUDIO_PARSER_CODEC_DTS
{
     unsigned char *stream;
     int ret = 0;
     stream = (unsigned char *)audio_stream_data;

     if (audio_stream_data == NULL || audio_frame_size == 0) {
         ALOGW("no buffer to get channel status, return default for compress");
         get_default_compressed_channel_status(channel_status);
         return ret;
     }

     memset(channel_status,0,24);
     if(init_audio_parser(stream, audio_frame_size, codec_type) == -1)
     {
         ALOGE("init audio parser failed");
         return -1;
     }
     ret = get_channel_status(channel_status, codec_type);
     return ret;

}

#endif

void get_lpcm_channel_status(uint32_t sampleRate,
                                                  unsigned char *channel_status)
{
     int32_t status = 0;
     memset(channel_status,0,24);
     /* block start bit in preamble bit 3 */
     channel_status[0] |= PROFESSIONAL;
     //LPCM OUT
     channel_status[0] &= ~NON_LPCM;

     switch (sampleRate) {
         case 8000:
         case 11025:
         case 12000:
         case 16000:
         case 22050:
             channel_status[3] |= SR_NOTID;
             break;
         case 24000:
             channel_status[3] |= SR_24000;
             break;
         case 32000:
             channel_status[3] |= SR_32000;
             break;
         case 44100:
             channel_status[3] |= SR_44100;
             break;
         case 48000:
             channel_status[3] |= SR_48000;
             break;
         case 88200:
             channel_status[3] |= SR_88200;
             break;
         case 96000:
             channel_status[3] |= SR_96000;
             break;
         case 176400:
             channel_status[3] |= SR_176400;
             break;
         case 192000:
            channel_status[3] |= SR_192000;
             break;
         default:
             ALOGV("Invalid sample_rate %u\n", sampleRate);
             status = -1;
             break;
     }
}

void audio_utils_set_hdmi_channel_status(struct stream_out *out, char * buffer, size_t bytes)
{
    unsigned char channel_status[24]={0};
    struct snd_aes_iec958 iec958;
    const char *mixer_ctl_name = "IEC958 Playback PCM Stream";
    struct mixer_ctl *ctl;
    ALOGV("%s: buffer %s bytes %zd", __func__, buffer, bytes);
#ifdef HDMI_PASSTHROUGH_ENABLED
    if (audio_extn_utils_is_dolby_format(out->format) &&
        /*TODO:Extend code to support DTS passthrough*/
        /*set compressed channel status bits*/
        audio_extn_passthru_is_passthrough_stream(out)){
        get_compressed_channel_status(buffer, bytes, channel_status, AUDIO_PARSER_CODEC_AC3);
    } else
#endif
    {
        /*set channel status bit for LPCM*/
        get_lpcm_channel_status(out->sample_rate, channel_status);
    }

    memcpy(iec958.status, channel_status,sizeof(iec958.status));
    ctl = mixer_get_ctl_by_name(out->dev->mixer, mixer_ctl_name);
    if (!ctl) {
            ALOGE("%s: Could not get ctl for mixer cmd - %s",
                  __func__, mixer_ctl_name);
            return;
    }
    if (mixer_ctl_set_array(ctl, &iec958, sizeof(iec958)) < 0) {
        ALOGE("%s: Could not set channel status for ext HDMI ",
              __func__);
        return;
    }

}
#endif

int audio_extn_utils_get_avt_device_drift(
                struct audio_usecase *usecase,
                struct audio_avt_device_drift_param *drift_param)
{
    int ret = 0, count = 0;
    char avt_device_drift_mixer_ctl_name[MIXER_PATH_MAX_LENGTH] = {0};
    const char *backend = NULL;
    struct mixer_ctl *ctl = NULL;
    struct audio_avt_device_drift_stats drift_stats;
    struct audio_device *adev = NULL;

    if (usecase != NULL && usecase->type == PCM_PLAYBACK) {
        backend = platform_get_snd_device_backend_interface(usecase->out_snd_device);
        if (!backend) {
            ALOGE("%s: Unsupported device %d", __func__,
                   usecase->stream.out->devices);
            ret = -EINVAL;
            goto done;
        }
        strlcpy(avt_device_drift_mixer_ctl_name,
                backend,
                MIXER_PATH_MAX_LENGTH);

        count = strlen(backend);
        if (MIXER_PATH_MAX_LENGTH - count > 0) {
            strlcat(&avt_device_drift_mixer_ctl_name[count],
                    " DRIFT",
                    MIXER_PATH_MAX_LENGTH - count);
        } else {
            ret = -EINVAL;
            goto done;
        }
    } else {
        ALOGE("%s: Invalid usecase",__func__);
        ret = -EINVAL;
        goto done;
    }

    adev = usecase->stream.out->dev;
    ctl = mixer_get_ctl_by_name(adev->mixer, avt_device_drift_mixer_ctl_name);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",
                __func__, avt_device_drift_mixer_ctl_name);

        ret = -EINVAL;
        goto done;
    }

    ALOGV("%s: Getting AV Timer vs Device Drift mixer ctrl name %s", __func__,
            avt_device_drift_mixer_ctl_name);

    mixer_ctl_update(ctl);
    count = mixer_ctl_get_num_values(ctl);
    if (count != sizeof(struct audio_avt_device_drift_stats)) {
        ALOGE("%s: mixer_ctl_get_num_values() invalid drift_stats data size",
                __func__);

        ret = -EINVAL;
        goto done;
    }

    ret = mixer_ctl_get_array(ctl, (void *)&drift_stats, count);
    if (ret != 0) {
        ALOGE("%s: mixer_ctl_get_array() failed to get drift_stats Params",
                __func__);

        ret = -EINVAL;
        goto done;
    }
    memcpy(drift_param, &drift_stats.drift_param,
            sizeof(struct audio_avt_device_drift_param));
done:
    return ret;
}

#ifdef SNDRV_COMPRESS_PATH_DELAY
int audio_extn_utils_compress_get_dsp_latency(struct stream_out *out)
{
    int ret = -EINVAL;
    struct snd_compr_metadata metadata;
    int delay_ms = COMPRESS_OFFLOAD_PLAYBACK_LATENCY;

    if (property_get_bool("audio.playback.dsp.pathdelay", false)) {
        ALOGD("%s:: Quering DSP delay %d",__func__, __LINE__);
        if (!(is_offload_usecase(out->usecase))) {
            ALOGE("%s:: not supported for non offload session", __func__);
            goto exit;
        }

        if (!out->compr) {
            ALOGD("%s:: Invalid compress handle,returning default dsp latency",
                    __func__);
            goto exit;
        }

        metadata.key = SNDRV_COMPRESS_PATH_DELAY;
        ret = compress_get_metadata(out->compr, &metadata);
        if(ret) {
            ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
            goto exit;
        }
        delay_ms = metadata.value[0] / 1000; /*convert to ms*/
    } else {
        ALOGD("%s:: Using Fix DSP delay",__func__);
    }

exit:
    ALOGD("%s:: delay in ms is %d",__func__, delay_ms);
    return delay_ms;
}
#else
int audio_extn_utils_compress_get_dsp_latency(struct stream_out *out __unused)
{
    return COMPRESS_OFFLOAD_PLAYBACK_LATENCY;
}
#endif

#ifdef SNDRV_COMPRESS_RENDER_MODE
int audio_extn_utils_compress_set_render_mode(struct stream_out *out)
{
    struct snd_compr_metadata metadata;
    int ret = -EINVAL;

    if (!(is_offload_usecase(out->usecase))) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

    if (!out->compr) {
        ALOGD("%s:: Invalid compress handle",
                __func__);
        goto exit;
    }

    ALOGD("%s:: render mode %d", __func__, out->render_mode);

    metadata.key = SNDRV_COMPRESS_RENDER_MODE;
    if (out->render_mode == RENDER_MODE_AUDIO_MASTER) {
        metadata.value[0] = SNDRV_COMPRESS_RENDER_MODE_AUDIO_MASTER;
    } else if (out->render_mode == RENDER_MODE_AUDIO_STC_MASTER) {
        metadata.value[0] = SNDRV_COMPRESS_RENDER_MODE_STC_MASTER;
    } else {
        ret = 0;
        goto exit;
    }
    ret = compress_set_metadata(out->compr, &metadata);
    if(ret) {
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
    }
exit:
    return ret;
}
#else
int audio_extn_utils_compress_set_render_mode(struct stream_out *out __unused)
{
    ALOGD("%s:: configuring render mode not supported", __func__);
    return 0;
}
#endif

#ifdef SNDRV_COMPRESS_CLK_REC_MODE
int audio_extn_utils_compress_set_clk_rec_mode(
            struct audio_usecase *usecase)
{
    struct snd_compr_metadata metadata;
    struct stream_out *out = NULL;
    int ret = -EINVAL;

    if (usecase == NULL || usecase->type != PCM_PLAYBACK) {
        ALOGE("%s:: Invalid use case", __func__);
        goto exit;
    }

    out = usecase->stream.out;
    if (!out) {
        ALOGE("%s:: invalid stream", __func__);
        goto exit;
    }

    if (!is_offload_usecase(out->usecase)) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

    if (out->render_mode != RENDER_MODE_AUDIO_STC_MASTER) {
        ALOGD("%s:: clk recovery is only supported in STC render mode",
                __func__);
        ret = 0;
        goto exit;
    }

    if (!out->compr) {
        ALOGD("%s:: Invalid compress handle",
                __func__);
        goto exit;
    }
    metadata.key = SNDRV_COMPRESS_CLK_REC_MODE;
    switch(usecase->out_snd_device) {
        case SND_DEVICE_OUT_HDMI:
        case SND_DEVICE_OUT_SPEAKER_AND_HDMI:
        case SND_DEVICE_OUT_DISPLAY_PORT:
        case SND_DEVICE_OUT_SPEAKER_AND_DISPLAY_PORT:
            metadata.value[0] = SNDRV_COMPRESS_CLK_REC_MODE_NONE;
            break;
        default:
            metadata.value[0] = SNDRV_COMPRESS_CLK_REC_MODE_AUTO;
            break;
    }

    ALOGD("%s:: clk recovery mode %d",__func__, metadata.value[0]);

    ret = compress_set_metadata(out->compr, &metadata);
    if(ret) {
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
    }

exit:
    return ret;
}
#else
int audio_extn_utils_compress_set_clk_rec_mode(
            struct audio_usecase *usecase __unused)
{
    ALOGD("%s:: configuring render mode not supported", __func__);
    return 0;
}
#endif

#ifdef SNDRV_COMPRESS_RENDER_WINDOW
int audio_extn_utils_compress_set_render_window(
            struct stream_out *out,
            struct audio_out_render_window_param *render_window)
{
    struct snd_compr_metadata metadata;
    int ret = -EINVAL;

    if(render_window == NULL) {
        ALOGE("%s:: Invalid render_window", __func__);
        goto exit;
    }

    ALOGD("%s:: render window start 0x%"PRIx64" end 0x%"PRIx64"",
          __func__,render_window->render_ws, render_window->render_we);

    if (!is_offload_usecase(out->usecase)) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

    if ((out->render_mode != RENDER_MODE_AUDIO_MASTER) &&
        (out->render_mode != RENDER_MODE_AUDIO_STC_MASTER)) {
        ALOGD("%s:: only supported in timestamp mode, current "
              "render mode mode %d", __func__, out->render_mode);
        goto exit;
    }

    if (!out->compr) {
        ALOGW("%s:: offload session not yet opened,"
               "render window will be configure later", __func__);
        /* store render window to reconfigure in start_output_stream() */
       goto exit;
    }

    metadata.key = SNDRV_COMPRESS_RENDER_WINDOW;
    /*render window start value */
    metadata.value[0] = 0xFFFFFFFF & render_window->render_ws; /* lsb */
    metadata.value[1] = \
            (0xFFFFFFFF00000000 & render_window->render_ws) >> 32; /* msb*/
    /*render window end value */
    metadata.value[2] = 0xFFFFFFFF & render_window->render_we; /* lsb */
    metadata.value[3] = \
            (0xFFFFFFFF00000000 & render_window->render_we) >> 32; /* msb*/

    ret = compress_set_metadata(out->compr, &metadata);
    if(ret) {
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
    }

exit:
    return ret;
}
#else
int audio_extn_utils_compress_set_render_window(
            struct stream_out *out __unused,
            struct audio_out_render_window_param *render_window __unused)
{
    ALOGD("%s:: configuring render window not supported", __func__);
    return 0;
}
#endif

#ifdef SNDRV_COMPRESS_START_DELAY
int audio_extn_utils_compress_set_start_delay(
            struct stream_out *out,
            struct audio_out_start_delay_param *delay_param)
{
    struct snd_compr_metadata metadata;
    int ret = -EINVAL;

    if(delay_param == NULL) {
        ALOGE("%s:: Invalid delay_param", __func__);
        goto exit;
    }

    ALOGD("%s:: render start delay 0x%"PRIx64" ", __func__,
          delay_param->start_delay);

    if (!is_offload_usecase(out->usecase)) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

   if ((out->render_mode != RENDER_MODE_AUDIO_MASTER) &&
       (out->render_mode != RENDER_MODE_AUDIO_STC_MASTER)) {
        ALOGD("%s:: only supported in timestamp mode, current "
              "render mode mode %d", __func__, out->render_mode);
        goto exit;
    }

    if (!out->compr) {
        ALOGW("%s:: offload session not yet opened,"
               "start delay will be configure later", __func__);
       goto exit;
    }

    metadata.key = SNDRV_COMPRESS_START_DELAY;
    metadata.value[0] = 0xFFFFFFFF & delay_param->start_delay; /* lsb */
    metadata.value[1] = \
            (0xFFFFFFFF00000000 & delay_param->start_delay) >> 32; /* msb*/

    ret = compress_set_metadata(out->compr, &metadata);
    if(ret) {
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
    }

exit:
    return ret;
}
#else
int audio_extn_utils_compress_set_start_delay(
            struct stream_out *out __unused,
            struct audio_out_start_delay_param *delay_param __unused)
{
    ALOGD("%s:: configuring render window not supported", __func__);
    return 0;
}
#endif

#ifdef SNDRV_COMPRESS_ENABLE_ADJUST_SESSION_CLOCK
int audio_extn_utils_compress_enable_drift_correction(
        struct stream_out *out,
        struct audio_out_enable_drift_correction *drift)
{
    struct snd_compr_metadata metadata;
    int ret = -EINVAL;

    if(drift == NULL) {
        ALOGE("%s:: Invalid param", __func__);
        goto exit;
    }

    ALOGD("%s:: drift enable %d", __func__,drift->enable);

    if (!is_offload_usecase(out->usecase)) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

    if (!out->compr) {
        ALOGW("%s:: offload session not yet opened,"
                "start delay will be configure later", __func__);
        goto exit;
    }

    metadata.key = SNDRV_COMPRESS_ENABLE_ADJUST_SESSION_CLOCK;
    metadata.value[0] = drift->enable;
    out->drift_correction_enabled = drift->enable;

    ret = compress_set_metadata(out->compr, &metadata);
    if(ret) {
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
        out->drift_correction_enabled = false;
    }

exit:
    return ret;
}
#else
int audio_extn_utils_compress_enable_drift_correction(
        struct stream_out *out __unused,
        struct audio_out_enable_drift_correction *drift __unused)
{
    ALOGD("%s:: configuring drift enablement not supported", __func__);
    return 0;
}
#endif

#ifdef SNDRV_COMPRESS_ADJUST_SESSION_CLOCK
int audio_extn_utils_compress_correct_drift(
        struct stream_out *out,
        struct audio_out_correct_drift *drift_param)
{
    struct snd_compr_metadata metadata;
    int ret = -EINVAL;

    if (drift_param == NULL) {
        ALOGE("%s:: Invalid drift_param", __func__);
        goto exit;
    }

    ALOGD("%s:: adjust time 0x%"PRIx64" ", __func__,
            drift_param->adjust_time);

    if (!is_offload_usecase(out->usecase)) {
        ALOGE("%s:: not supported for non offload session", __func__);
        goto exit;
    }

    if (!out->compr) {
        ALOGW("%s:: offload session not yet opened", __func__);
        goto exit;
    }

    if (!out->drift_correction_enabled) {
        ALOGE("%s:: drift correction not enabled", __func__);
        goto exit;
    }

    metadata.key = SNDRV_COMPRESS_ADJUST_SESSION_CLOCK;
    metadata.value[0] = 0xFFFFFFFF & drift_param->adjust_time; /* lsb */
    metadata.value[1] = \
             (0xFFFFFFFF00000000 & drift_param->adjust_time) >> 32; /* msb*/

    ret = compress_set_metadata(out->compr, &metadata);
    if(ret)
        ALOGE("%s::error %s", __func__, compress_get_error(out->compr));
exit:
    return ret;
}
#else
int audio_extn_utils_compress_correct_drift(
        struct stream_out *out __unused,
        struct audio_out_correct_drift *drift_param __unused)
{
    ALOGD("%s:: setting adjust clock not supported", __func__);
    return 0;
}
#endif

int audio_extn_utils_set_channel_map(
            struct stream_out *out,
            struct audio_out_channel_map_param *channel_map_param)
{
    int ret = -EINVAL, i = 0;
    int channels = audio_channel_count_from_out_mask(out->channel_mask);

    if (channel_map_param == NULL) {
        ALOGE("%s:: Invalid channel_map", __func__);
        goto exit;
    }

    if (channel_map_param->channels != channels) {
        ALOGE("%s:: Channels(%d) does not match stream channels(%d)",
                                __func__, channel_map_param->channels, channels);
        goto exit;
    }

    for ( i = 0; i < channels; i++) {
        ALOGV("%s:: channel_map[%d]- %d", __func__, i, channel_map_param->channel_map[i]);
        out->channel_map_param.channel_map[i] = channel_map_param->channel_map[i];
    }
    ret = 0;
exit:
    return ret;
}

int audio_extn_utils_set_pan_scale_params(
            struct stream_out *out,
            struct mix_matrix_params *mm_params)
{
    int ret = -EINVAL, i = 0, j = 0;

    if (mm_params == NULL && out != NULL) {
        ALOGE("%s:: Invalid mix matrix params", __func__);
        goto exit;
    }

    if (mm_params->num_output_channels > MAX_CHANNELS_SUPPORTED ||
        mm_params->num_output_channels <= 0 ||
        mm_params->num_input_channels > MAX_CHANNELS_SUPPORTED ||
        mm_params->num_input_channels <= 0)
        goto exit;

    out->pan_scale_params.num_output_channels = mm_params->num_output_channels;
    out->pan_scale_params.num_input_channels = mm_params->num_input_channels;
    out->pan_scale_params.has_output_channel_map =
                                        mm_params->has_output_channel_map;
    for (i = 0; i < mm_params->num_output_channels; i++)
        out->pan_scale_params.output_channel_map[i] =
                                         mm_params->output_channel_map[i];

    out->pan_scale_params.has_input_channel_map =
                                         mm_params->has_input_channel_map;
    for (i = 0; i < mm_params->num_input_channels; i++)
        out->pan_scale_params.input_channel_map[i] =
                                         mm_params->input_channel_map[i];

    out->pan_scale_params.has_mixer_coeffs = mm_params->has_mixer_coeffs;
    for (i = 0; i < mm_params->num_output_channels; i++)
        for (j = 0; j < mm_params->num_input_channels; j++) {
            //Convert the channel coefficient gains in Q14 format
            out->pan_scale_params.mixer_coeffs[i][j] =
                               mm_params->mixer_coeffs[i][j] * (2 << 13);
        }

    ret = platform_set_stream_pan_scale_params(out->dev->platform,
                                               out->pcm_device_id,
                                               out->pan_scale_params);

exit:
    return ret;
}

int audio_extn_utils_set_downmix_params(
            struct stream_out *out,
            struct mix_matrix_params *mm_params)
{
    int ret = -EINVAL, i = 0, j = 0;
    struct audio_usecase *usecase = NULL;

    if (mm_params == NULL && out != NULL) {
        ALOGE("%s:: Invalid mix matrix params", __func__);
        goto exit;
    }

    if (mm_params->num_output_channels > MAX_CHANNELS_SUPPORTED ||
        mm_params->num_output_channels <= 0 ||
        mm_params->num_input_channels > MAX_CHANNELS_SUPPORTED ||
        mm_params->num_input_channels <= 0)
        goto exit;

    usecase = get_usecase_from_list(out->dev, out->usecase);
    out->downmix_params.num_output_channels = mm_params->num_output_channels;
    out->downmix_params.num_input_channels = mm_params->num_input_channels;

    out->downmix_params.has_output_channel_map =
                                        mm_params->has_output_channel_map;
    for (i = 0; i < mm_params->num_output_channels; i++) {
        out->downmix_params.output_channel_map[i] =
                                          mm_params->output_channel_map[i];
    }

    out->downmix_params.has_input_channel_map =
                                        mm_params->has_input_channel_map;
    for (i = 0; i < mm_params->num_input_channels; i++)
        out->downmix_params.input_channel_map[i] =
                                          mm_params->input_channel_map[i];

    out->downmix_params.has_mixer_coeffs = mm_params->has_mixer_coeffs;
    for (i = 0; i < mm_params->num_output_channels; i++)
        for (j = 0; j < mm_params->num_input_channels; j++) {
            //Convert the channel coefficient gains in Q14 format
            out->downmix_params.mixer_coeffs[i][j] =
                               mm_params->mixer_coeffs[i][j] * (2 << 13);
    }

    ret = platform_set_stream_downmix_params(out->dev->platform,
                                             out->pcm_device_id,
                                             usecase->out_snd_device,
                                             out->downmix_params);

exit:
    return ret;
}

bool audio_extn_utils_is_dolby_format(audio_format_t format)
{
    if (format == AUDIO_FORMAT_AC3 ||
            format == AUDIO_FORMAT_E_AC3 ||
            format == AUDIO_FORMAT_E_AC3_JOC)
        return true;
    else
        return false;
}


