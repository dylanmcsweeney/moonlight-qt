#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "h264_stream.h"

static int fail_check(const char* expression, const char* file, int line)
{
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    return 1;
}

// These checks intentionally remain enabled when NDEBUG is defined.
#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            return fail_check(#expression, __FILE__, __LINE__); \
        } \
    } while (0)

static int check_read_se(const uint8_t encoded[8], int32_t expected)
{
    uint8_t guarded[10];
    bs_t bitstream;
    int32_t actual;

    guarded[0] = 0xA5;
    memcpy(&guarded[1], encoded, 8);
    guarded[9] = 0x5A;
    bs_init(&bitstream, &guarded[1], 8);

    actual = bs_read_se(&bitstream);

    CHECK(actual == expected);
    CHECK(bs_pos(&bitstream) == 7);
    CHECK(!bs_overrun(&bitstream));
    CHECK(guarded[0] == 0xA5);
    CHECK(guarded[9] == 0x5A);
    return 0;
}

static int test_read_se_boundaries(void)
{
    // ue(0x7fffffff): 31 zero prefix bits, a one, then 31 zero suffix bits.
    // Signed Exp-Golomb maps this odd code number to +0x40000000.
    static const uint8_t int32_max_code_num[8] = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
    };

    // ue(0xfffffffd): 31 zero prefix bits, a one, then suffix 0x7ffffffe.
    // This is the largest positive signed Exp-Golomb value, INT32_MAX.
    static const uint8_t largest_positive_code_num[8] = {
        0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFC
    };

    // ue(0xfffffffe): same prefix with suffix 0x7fffffff, mapping to -INT32_MAX.
    static const uint8_t largest_negative_code_num[8] = {
        0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE
    };

    CHECK(check_read_se(int32_max_code_num, INT32_C(1073741824)) == 0);
    CHECK(check_read_se(largest_positive_code_num, INT32_MAX) == 0);
    CHECK(check_read_se(largest_negative_code_num, -INT32_MAX) == 0);
    return 0;
}

static int test_write_u_more_than_32_bits(void)
{
    uint8_t guarded[7] = { 0xA5, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x5A };
    static const uint8_t expected[5] = { 0x00, 0x89, 0xAB, 0xCD, 0xEF };
    bs_t bitstream;

    bs_init(&bitstream, &guarded[1], 5);
    bs_write_u(&bitstream, 40, UINT32_C(0x89ABCDEF));

    CHECK(memcmp(&guarded[1], expected, sizeof(expected)) == 0);
    CHECK(guarded[0] == 0xA5);
    CHECK(guarded[6] == 0x5A);
    CHECK(bs_pos(&bitstream) == 5);
    CHECK(!bs_overrun(&bitstream));
    return 0;
}

static int test_sps_writer_clamps_poc_cycle(void)
{
    uint8_t guarded[258];
    sps_t source;
    sps_t parsed;
    bs_t writer;
    bs_t reader;
    int written_size;

    memset(guarded, 0xCC, sizeof(guarded));
    guarded[0] = 0xA5;
    guarded[257] = 0x5A;
    memset(&source, 0, sizeof(source));

    source.profile_idc = 66;
    source.level_idc = 30;
    source.pic_order_cnt_type = 1;
    source.num_ref_frames_in_pic_order_cnt_cycle = 257;
    source.num_ref_frames = 1;
    source.pic_width_in_mbs_minus1 = 39;
    source.pic_height_in_map_units_minus1 = 29;
    source.frame_mbs_only_flag = 1;
    source.direct_8x8_inference_flag = 1;

    bs_init(&writer, &guarded[1], 256);
    write_seq_parameter_set_rbsp(&source, &writer);
    write_rbsp_trailing_bits(&writer);

    CHECK(source.num_ref_frames_in_pic_order_cnt_cycle == 256);
    CHECK(!bs_overrun(&writer));
    CHECK(guarded[0] == 0xA5);
    CHECK(guarded[257] == 0x5A);
    written_size = bs_pos(&writer);
    CHECK(written_size > 0);

    bs_init(&reader, &guarded[1], (size_t)written_size);
    read_seq_parameter_set_rbsp(&parsed, &reader);
    CHECK(parsed.num_ref_frames_in_pic_order_cnt_cycle == 256);
    CHECK(parsed.offset_for_ref_frame[255] == 0);
    CHECK(parsed.num_ref_frames == 1);
    CHECK(!bs_overrun(&reader));
    return 0;
}

static int test_hrd_writer_clamps_cpb_count(void)
{
    uint8_t guarded[66];
    hrd_t source;
    hrd_t parsed;
    bs_t writer;
    bs_t reader;
    int written_size;

    memset(guarded, 0xCC, sizeof(guarded));
    guarded[0] = 0xA5;
    guarded[65] = 0x5A;
    memset(&source, 0, sizeof(source));
    source.cpb_cnt_minus1 = 32;

    bs_init(&writer, &guarded[1], 64);
    write_hrd_parameters(&source, &writer);
    write_rbsp_trailing_bits(&writer);

    CHECK(source.cpb_cnt_minus1 == 31);
    CHECK(!bs_overrun(&writer));
    CHECK(guarded[0] == 0xA5);
    CHECK(guarded[65] == 0x5A);
    written_size = bs_pos(&writer);
    CHECK(written_size > 0);

    bs_init(&reader, &guarded[1], (size_t)written_size);
    read_hrd_parameters(&parsed, &reader);
    CHECK(parsed.cpb_cnt_minus1 == 31);
    CHECK(parsed.bit_rate_value_minus1[31] == 0);
    CHECK(parsed.cpb_size_value_minus1[31] == 0);
    CHECK(parsed.cbr_flag[31] == 0);
    CHECK(!bs_overrun(&reader));
    return 0;
}

static int test_malformed_nal_handling(void)
{
    const uint8_t no_start_code[] = { 0x11, 0x22, 0x33, 0x44 };
    const uint8_t truncated_sps[] = { 0x67 };
    uint8_t guarded_output[] = { 0xA5, 0xCC, 0x5A };
    int nal_start = -1;
    int nal_end = -1;
    h264_stream_t* stream;
    int result;

    result = find_nal_unit((uint8_t*)no_start_code, (int)sizeof(no_start_code), &nal_start, &nal_end);
    CHECK(result == 0);
    CHECK(nal_start == 0);
    CHECK(nal_end == 0);

    stream = h264_new();
    result = read_nal_unit(stream, (uint8_t*)truncated_sps, (int)sizeof(truncated_sps));
    h264_free(stream);
    CHECK(result < 0);

    stream = h264_new();
    stream->nal->nal_unit_type = NAL_UNIT_TYPE_SPS;
    result = write_nal_unit(stream, &guarded_output[1], 1);
    h264_free(stream);
    CHECK(result < 0);
    CHECK(guarded_output[0] == 0xA5);
    CHECK(guarded_output[2] == 0x5A);
    return 0;
}

int main(void)
{
    if (test_read_se_boundaries() != 0 ||
        test_write_u_more_than_32_bits() != 0 ||
        test_sps_writer_clamps_poc_cycle() != 0 ||
        test_hrd_writer_clamps_cpb_count() != 0 ||
        test_malformed_nal_handling() != 0) {
        return 1;
    }

    printf("bitstream boundary tests passed\n");
    return 0;
}
