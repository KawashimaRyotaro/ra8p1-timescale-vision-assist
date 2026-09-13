#include <tk/tkernel.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal_data.h"
#include "video_source.h"
#include "npu_worker.h"
#include "yolox_tiny/src_mcu_npu/model.h"
#include "yolox_tiny/preprocessing.h"
#include "yolox_tiny/postprocessing.h"


volatile fsp_err_t g_npu_worker_open_result = FSP_ERR_NOT_OPEN;

volatile uint32_t g_npu_worker_run_count       = 0U;
volatile uint32_t g_npu_worker_error_count     = 0U;
volatile uint32_t g_npu_worker_last_mismatch   = 0U;
volatile uint32_t g_npu_worker_running         = 0U;

volatile uint32_t g_npu_worker_frame_count      = 0U;
volatile uint32_t g_npu_worker_last_frame_index = 0U;
volatile uint32_t g_npu_worker_last_slot        = 0U;
volatile uint32_t g_npu_worker_frame_sample     = 0U;


/*
 * RunModel() profiling.
 */
volatile uint32_t g_npu_inference_count    = 0U;
volatile uint32_t g_npu_inference_last_ms  = 0U;
volatile uint32_t g_npu_inference_total_ms = 0U;
volatile uint32_t g_npu_inference_min_ms   = UINT32_MAX;
volatile uint32_t g_npu_inference_max_ms   = 0U;


/*
 * RGB565 -> YOLOX input preprocessing profiling.
 */
volatile uint32_t g_npu_preprocess_count    = 0U;
volatile uint32_t g_npu_preprocess_last_ms  = 0U;
volatile uint32_t g_npu_preprocess_total_ms = 0U;
volatile uint32_t g_npu_preprocess_min_ms   = UINT32_MAX;
volatile uint32_t g_npu_preprocess_max_ms   = 0U;


/*
 * YOLOX decode + NMS postprocessing profiling.
 */
volatile uint32_t g_npu_postprocess_count    = 0U;
volatile uint32_t g_npu_postprocess_last_ms  = 0U;
volatile uint32_t g_npu_postprocess_total_ms = 0U;
volatile uint32_t g_npu_postprocess_min_ms   = UINT32_MAX;
volatile uint32_t g_npu_postprocess_max_ms   = 0U;


volatile int32_t g_npu_detection_count = 0;

volatile float g_npu_first_x1    = 0.0F;
volatile float g_npu_first_y1    = 0.0F;
volatile float g_npu_first_x2    = 0.0F;
volatile float g_npu_first_y2    = 0.0F;
volatile float g_npu_first_score = 0.0F;

volatile uint32_t g_npu_first_class = 0U;


/*
 * Legacy exact-frame results.
 *
 * Kept because other debug code may still use
 * npu_worker_get_detections().
 */
static Detection_t
    s_slot_detections
        [VIDEO_SOURCE_BUFFER_COUNT]
        [MAX_DETECTIONS];

static volatile int32_t
    s_slot_detection_count
        [VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint32_t
    s_slot_result_frame_index
        [VIDEO_SOURCE_BUFFER_COUNT] =
{
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX
};


/*
 * Newest COMPLETED YOLOX result.
 *
 * This buffer is independent of the raw Frame Cache slot.
 * Therefore Display may show the newest semantic result without
 * waiting for the current 30-FPS raw frame to finish YOLOX.
 *
 * s_latest_result_version is used as a seqlock:
 *
 *   even = stable snapshot
 *   odd  = writer is updating snapshot
 */
static Detection_t
    s_latest_detections[MAX_DETECTIONS];

static volatile int32_t
    s_latest_detection_count = 0;

static volatile uint32_t
    s_latest_result_frame_index = UINT32_MAX;

static volatile uint32_t
    s_latest_result_version = 0U;


/*
 * Get monotonically increasing system operating time.
 */
static uint8_t npu_worker_get_time_ms(
    uint64_t * time_ms)
{
    if (NULL == time_ms)
    {
        return 0U;
    }

    SYSTIM system_time;

    ER const err =
        tk_get_otm(
            &system_time
        );

    if (err < E_OK)
    {
        return 0U;
    }

    *time_ms =
        ((uint64_t) (uint32_t) system_time.hi << 32) |
        (uint64_t) system_time.lo;

    return 1U;
}


/*
 * Invalidate latest completed result.
 *
 * Used when a new dataset sequence starts from frame 0.
 */
static void npu_worker_invalidate_latest_result(void)
{
    s_latest_result_version++;

    __DMB();

    s_latest_detection_count =
        0;

    s_latest_result_frame_index =
        UINT32_MAX;

    __DMB();

    s_latest_result_version++;

    __DMB();
}


/*
 * Publish newest completed YOLOX result.
 */
static void npu_worker_publish_latest_result(
    const Detection_t * detections,
    int32_t detection_count,
    uint32_t frame_index)
{
    int32_t copy_count =
        detection_count;

    if (copy_count < 0)
    {
        copy_count = 0;
    }

    if (copy_count >
        (int32_t) MAX_DETECTIONS)
    {
        copy_count =
            (int32_t) MAX_DETECTIONS;
    }


    /*
     * Enter write section.
     */
    s_latest_result_version++;

    __DMB();


    if ((copy_count > 0) &&
        (NULL != detections))
    {
        memcpy(
            s_latest_detections,
            detections,
            (size_t) copy_count *
            sizeof(Detection_t)
        );
    }


    s_latest_detection_count =
        copy_count;

    s_latest_result_frame_index =
        frame_index;


    __DMB();

    /*
     * Leave write section.
     */
    s_latest_result_version++;

    __DMB();
}


static void npu_worker_task(
    INT stacd,
    void * exinf)
{
    (void) stacd;
    (void) exinf;


    /*
     * Open Ethos-U55.
     */
    g_npu_worker_open_result =
        RM_ETHOSU_Open(
            &g_rm_ethosu0_ctrl,
            &g_rm_ethosu0_cfg
        );

    if (FSP_SUCCESS !=
        g_npu_worker_open_result)
    {
        return;
    }


    /*
     * Tell video_source that NPU is active.
     *
     * With CP1.5 video_source, this consumer receives only the newest
     * waiting raw frame while Ethos-U55 is busy.
     */
    video_source_npu_consumer_enable();

    g_npu_worker_running =
        1U;


    while (1)
    {
        uint32_t slot;
        uint32_t frame_index;


        const uint8_t * frame =
            video_source_acquire_npu_buffer(
                &slot,
                &frame_index
            );

        if (NULL == frame)
        {
            tk_dly_tsk(1);
            continue;
        }


        /*
         * A new RAW sequence starts from frame 0.
         */
        if (0U == frame_index)
        {
            g_npu_inference_count    = 0U;
            g_npu_inference_last_ms  = 0U;
            g_npu_inference_total_ms = 0U;
            g_npu_inference_min_ms   = UINT32_MAX;
            g_npu_inference_max_ms   = 0U;

            g_npu_preprocess_count    = 0U;
            g_npu_preprocess_last_ms  = 0U;
            g_npu_preprocess_total_ms = 0U;
            g_npu_preprocess_min_ms   = UINT32_MAX;
            g_npu_preprocess_max_ms   = 0U;

            g_npu_postprocess_count    = 0U;
            g_npu_postprocess_last_ms  = 0U;
            g_npu_postprocess_total_ms = 0U;
            g_npu_postprocess_min_ms   = UINT32_MAX;
            g_npu_postprocess_max_ms   = 0U;

            npu_worker_invalidate_latest_result();
        }


        /*
         * Debug sample proving that NPU sees the raw RGB565 frame.
         *
         * Note: raw frame uses a 2048-byte line stride, so sample
         * accesses below intentionally follow the legacy behavior.
         */
        const uint16_t * pixels =
            (const uint16_t *) frame;

        uint32_t const pixel_count =
            VIDEO_SOURCE_WIDTH *
            VIDEO_SOURCE_HEIGHT;

        g_npu_worker_frame_sample =
            ((uint32_t) pixels[0]) ^
            ((uint32_t) pixels[pixel_count / 2U] << 8U) ^
            ((uint32_t) pixels[pixel_count - 1U] << 16U);


        g_npu_worker_last_slot =
            slot;

        g_npu_worker_last_frame_index =
            frame_index;

        g_npu_worker_frame_count++;


        int8_t * const p_input =
            GetModelInputPtr_serving_default_images_0();


        letterbox_params_t
            letterbox_params;


        /*
         * RGB565 -> 224x224 INT8 preprocessing.
         */
        uint64_t preprocess_start_ms = 0U;
        uint64_t preprocess_end_ms   = 0U;

        uint8_t const preprocess_timing_valid =
            npu_worker_get_time_ms(
                &preprocess_start_ms
            );


        preprocess_rgb565_strided(
            (const uint16_t *) frame,
            VIDEO_SOURCE_WIDTH,
            VIDEO_SOURCE_HEIGHT,
            VIDEO_SOURCE_STRIDE_BYTES,
            p_input,
            224U,
            224U,
            &letterbox_params
        );


        if ((0U !=
             preprocess_timing_valid) &&
            (0U !=
             npu_worker_get_time_ms(
                 &preprocess_end_ms)))
        {
            uint32_t const elapsed_ms =
                (uint32_t)
                (
                    preprocess_end_ms -
                    preprocess_start_ms
                );

            g_npu_preprocess_last_ms =
                elapsed_ms;

            g_npu_preprocess_total_ms +=
                elapsed_ms;

            g_npu_preprocess_count++;

            if (elapsed_ms <
                g_npu_preprocess_min_ms)
            {
                g_npu_preprocess_min_ms =
                    elapsed_ms;
            }

            if (elapsed_ms >
                g_npu_preprocess_max_ms)
            {
                g_npu_preprocess_max_ms =
                    elapsed_ms;
            }
        }


        /*
         * IMPORTANT:
         * Raw frame ownership ends here.
         *
         * Ethos-U55 inference below no longer holds the Camera/USB
         * Frame Cache slot.
         */
        video_source_release_npu_buffer(
            slot
        );


        /*
         * Run YOLOX-Tiny.
         */
        uint64_t inference_start_ms = 0U;
        uint64_t inference_end_ms   = 0U;

        uint8_t const inference_timing_valid =
            npu_worker_get_time_ms(
                &inference_start_ms
            );


        RunModel(false);


        if ((0U !=
             inference_timing_valid) &&
            (0U !=
             npu_worker_get_time_ms(
                 &inference_end_ms)))
        {
            uint32_t const elapsed_ms =
                (uint32_t)
                (
                    inference_end_ms -
                    inference_start_ms
                );

            g_npu_inference_last_ms =
                elapsed_ms;

            g_npu_inference_total_ms +=
                elapsed_ms;

            g_npu_inference_count++;

            if (elapsed_ms <
                g_npu_inference_min_ms)
            {
                g_npu_inference_min_ms =
                    elapsed_ms;
            }

            if (elapsed_ms >
                g_npu_inference_max_ms)
            {
                g_npu_inference_max_ms =
                    elapsed_ms;
            }
        }


        /*
         * Raw model output:
         * 1029 anchors x 85 channels.
         */
        int8_t * const p_output =
            GetModelOutputPtr_PartitionedCall_0_70478();


        /*
         * Mark legacy slot result invalid while writing.
         */
        s_slot_result_frame_index[slot] =
            UINT32_MAX;

        __DMB();


        uint64_t postprocess_start_ms = 0U;
        uint64_t postprocess_end_ms   = 0U;

        uint8_t const postprocess_timing_valid =
            npu_worker_get_time_ms(
                &postprocess_start_ms
            );


        int32_t const detection_count =
            postprocess(
                p_output,
                &letterbox_params,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT,
                SCORE_THRESH,
                NMS_IOU_THRESH,
                s_slot_detections[slot]
            );


        if ((0U !=
             postprocess_timing_valid) &&
            (0U !=
             npu_worker_get_time_ms(
                 &postprocess_end_ms)))
        {
            uint32_t const elapsed_ms =
                (uint32_t)
                (
                    postprocess_end_ms -
                    postprocess_start_ms
                );

            g_npu_postprocess_last_ms =
                elapsed_ms;

            g_npu_postprocess_total_ms +=
                elapsed_ms;

            g_npu_postprocess_count++;

            if (elapsed_ms <
                g_npu_postprocess_min_ms)
            {
                g_npu_postprocess_min_ms =
                    elapsed_ms;
            }

            if (elapsed_ms >
                g_npu_postprocess_max_ms)
            {
                g_npu_postprocess_max_ms =
                    elapsed_ms;
            }
        }


        /*
         * Publish legacy exact-frame result.
         */
        s_slot_detection_count[slot] =
            detection_count;

        __DMB();

        s_slot_result_frame_index[slot] =
            frame_index;

        __DMB();


        /*
         * Publish independent newest-completed semantic result.
         *
         * Display will use this result without waiting for the raw
         * frame currently being shown.
         */
        npu_worker_publish_latest_result(
            s_slot_detections[slot],
            detection_count,
            frame_index
        );


        g_npu_detection_count =
            detection_count;


        /*
         * Export first detection for debugger inspection.
         */
        if (detection_count > 0)
        {
            g_npu_first_x1 =
                s_slot_detections[slot][0].x1;

            g_npu_first_y1 =
                s_slot_detections[slot][0].y1;

            g_npu_first_x2 =
                s_slot_detections[slot][0].x2;

            g_npu_first_y2 =
                s_slot_detections[slot][0].y2;

            g_npu_first_score =
                s_slot_detections[slot][0].score;

            g_npu_first_class =
                s_slot_detections[slot][0].cls_id;
        }
        else
        {
            g_npu_first_x1    = 0.0F;
            g_npu_first_y1    = 0.0F;
            g_npu_first_x2    = 0.0F;
            g_npu_first_y2    = 0.0F;
            g_npu_first_score = 0.0F;
            g_npu_first_class = 0U;
        }


        g_npu_worker_last_mismatch =
            0U;

        g_npu_worker_run_count++;
    }
}


void npu_worker_start(void)
{
    T_CTSK ctsk =
    {
        .tskatr  = TA_HLNG,
        .task    = npu_worker_task,
        .itskpri = 12,
        .stksz   = 2048
    };


    ID const task_id =
        tk_cre_tsk(
            &ctsk
        );


    if (task_id > 0)
    {
        tk_sta_tsk(
            task_id,
            0
        );
    }
}


const Detection_t * npu_worker_get_detections(
    uint32_t slot,
    uint32_t frame_index,
    int32_t * p_detection_count)
{
    if ((slot >=
         VIDEO_SOURCE_BUFFER_COUNT) ||
        (NULL ==
         p_detection_count))
    {
        return NULL;
    }


    __DMB();


    /*
     * Result must belong to exactly the requested frame.
     */
    if (s_slot_result_frame_index[slot] !=
        frame_index)
    {
        return NULL;
    }


    __DMB();


    *p_detection_count =
        s_slot_detection_count[slot];


    return
        s_slot_detections[slot];
}


int32_t npu_worker_copy_latest_detections(
    Detection_t * p_out_detections,
    uint32_t capacity,
    uint32_t * p_result_frame_index)
{
    if ((NULL ==
         p_out_detections) ||
        (0U ==
         capacity) ||
        (NULL ==
         p_result_frame_index))
    {
        return -1;
    }


    /*
     * Retry if the NPU worker happens to publish a new snapshot while
     * Display is copying the previous one.
     */
    for (uint32_t retry = 0U;
         retry < 4U;
         retry++)
    {
        uint32_t const version_before =
            s_latest_result_version;


        /*
         * Odd version means writer is active.
         */
        if (0U !=
            (version_before & 1U))
        {
            continue;
        }


        __DMB();


        uint32_t const result_frame_index =
            s_latest_result_frame_index;


        /*
         * No completed inference yet.
         */
        if (UINT32_MAX ==
            result_frame_index)
        {
            return -1;
        }


        int32_t copy_count =
            s_latest_detection_count;


        if (copy_count < 0)
        {
            copy_count =
                0;
        }


        if ((uint32_t) copy_count >
            capacity)
        {
            copy_count =
                (int32_t) capacity;
        }


        if (copy_count > 0)
        {
            memcpy(
                p_out_detections,
                s_latest_detections,
                (size_t) copy_count *
                sizeof(Detection_t)
            );
        }


        __DMB();


        uint32_t const version_after =
            s_latest_result_version;


        if ((version_before ==
             version_after) &&
            (0U ==
             (version_after & 1U)))
        {
            *p_result_frame_index =
                result_frame_index;

            return
                copy_count;
        }
    }


    return -1;
}
