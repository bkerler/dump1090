// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// convert.c: support for various IQ -> magnitude conversions
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "dump1090.h"

struct converter_state {
    double dc_a;
    double dc_b;
    double z1_I;
    double z1_Q;
};

static double *uc8_lookup;
static bool init_uc8_lookup()
{
    if (uc8_lookup)
        return true;

    uc8_lookup = malloc(sizeof(double) * 256 * 256);
    if (!uc8_lookup) {
        fprintf(stderr, "can't allocate UC8 conversion lookup table\n");
        return false;
    }

    for (int I = 0; I <= 255; I++) {
        for (int Q = 0; Q <= 255; Q++) {
            const double fI = I > 127 ? (I - 127) / 128.0 : (128 - I) / -128.0;
            const double fQ = Q > 127 ? (Q - 127) / 128.0 : (128 - Q) / -128.0;
            const double magsq = fI * fI + fQ * fQ;
            uc8_lookup[le16toh((I*256)+Q)] = sqrt(magsq); // magsq > 1.0 ? 1.0 : magsq;
        }
    }

    return true;
}

static void convert_uc8_nodc(void *iq_data,
                             mag_data_t *mag_data,
                             unsigned nsamples,
                             struct converter_state *state,
                             double *out_mean_level,
                             double *out_mean_power)
{
    uint16_t *in = iq_data;
    double sum_level = 0.0;
    double sum_power = 0.0;

    MODES_NOTUSED(state);

    // unroll this a bit

#define DO_ONE_SAMPLE \
    do {                                            \
        const double mag = uc8_lookup[*in++];                    \
        *mag_data++ = mag;                          \
        sum_level += mag;                           \
        sum_power += mag * mag; \
    } while(0)

    // unroll this a bit
    for (unsigned i = 0; i < (nsamples>>3); ++i) {
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
    }

    for (unsigned i = 0; i < (nsamples&7); ++i) {
        DO_ONE_SAMPLE;
    }

#undef DO_ONE_SAMPLE

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_uc8_generic(void *iq_data,
                                mag_data_t *mag_data,
                                unsigned nsamples,
                                struct converter_state *state,
                                double *out_mean_level,
                                double *out_mean_power)
{
    const uint8_t *in = iq_data;
    double z1_I = state->z1_I;
    double z1_Q = state->z1_Q;
    const double dc_a = state->dc_a;
    const double dc_b = state->dc_b;

    double sum_level = 0.0, sum_power = 0.0;

    for (unsigned i = 0; i < nsamples; ++i) {
        const uint8_t I = *in++;
        const uint8_t Q = *in++;
        double fI = I > 127 ? (I - 127) / 128.0 : (128 - I) / -128.0;
        double fQ = Q > 127 ? (Q - 127) / 128.0 : (128 - Q) / -128.0;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        double magsq = fI * fI + fQ * fQ;
        //if (magsq > 1.0)
        //    magsq = 1.0;

        const double mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_sc16_generic(void *iq_data,
                                 mag_data_t *mag_data,
                                 unsigned nsamples,
                                 struct converter_state *state,
                                double *out_mean_level,
                                double *out_mean_power)
{
    const uint16_t *in = iq_data;
    double z1_I = state->z1_I;
    double z1_Q = state->z1_Q;
    const double dc_a = state->dc_a;
    const double dc_b = state->dc_b;

    double sum_level = 0.0, sum_power = 0.0;

    for (unsigned i = 0; i < nsamples; ++i) {
        const uint16_t I = (int16_t)le16toh(*in++);
        const uint16_t Q = (int16_t)le16toh(*in++);
        double fI = I / (double)SHRT_MAX;
        double fQ = Q / (double)SHRT_MAX;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        double magsq = fI * fI + fQ * fQ;
        //if (magsq > 1.0)
        //    magsq = 1.0;

        const double mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_sc16_nodc(void *iq_data,
                              mag_data_t *mag_data,
                              unsigned nsamples,
                              struct converter_state *state,
                              double *out_mean_level,
                              double *out_mean_power)
{
    MODES_NOTUSED(state);

    const uint16_t *in = iq_data;
    double sum_level = 0.0, sum_power = 0.0;

    for (unsigned i = 0; i < nsamples; ++i) {
        const int16_t I = (int16_t)le16toh(*in++);
        const int16_t Q = (int16_t)le16toh(*in++);
        const double fI = I / (double)SHRT_MAX;
        const double fQ = Q / (double)SHRT_MAX;

        double magsq = fI * fI + fQ * fQ;
        //if (magsq > 1.0)
        //    magsq = 1.0;

        const double mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

// SC16Q11_TABLE_BITS controls the size of the lookup table
// for SC16Q11 data. The size of the table is 2 * (1 << (2*BITS))
// bytes. Reducing the number of bits reduces precision but
// can run substantially faster by staying in cache.
// See convert_benchmark.c for some numbers.

// Leaving SC16QQ_TABLE_BITS undefined will disable the table lookup and always use
// the floating-point path, which may be faster on some systems

#if defined(SC16Q11_TABLE_BITS)

#define USE_BITS SC16Q11_TABLE_BITS
#define LOSE_BITS (11 - SC16Q11_TABLE_BITS)

static uint16_t *sc16q11_lookup;
static bool init_sc16q11_lookup()
{
    if (sc16q11_lookup)
        return true;

    sc16q11_lookup = malloc(sizeof(uint16_t) * (1 << (USE_BITS * 2)));
    if (!sc16q11_lookup) {
        fprintf(stderr, "can't allocate SC16Q11 conversion lookup table\n");
        return false;
    }

    for (int i = 0; i < 2048; i += (1 << LOSE_BITS)) {
        for (int q = 0; q < 2048; q += (1 << LOSE_BITS)) {
            const double fI = i / 2048.0, fQ = q / 2048.0;
            double magsq = fI * fI + fQ * fQ;
            if (magsq > 1.0)
                magsq = 1.0;
            const double mag = sqrt(magsq);

            int index = ((i >> LOSE_BITS) << USE_BITS) | (q >> LOSE_BITS);
            sc16q11_lookup[index] = (uint16_t)lround(mag * USHRT_MAX);
        }
    }

    return true;
}

static void convert_sc16q11_table(void *iq_data,
                                  float *mag_data,
                                  unsigned nsamples,
                                  struct converter_state *state,
                                  double *out_mean_level,
                                  double *out_mean_power)
{
    uint16_t *in = iq_data;
    double sum_level = 0.0;
    double sum_power = 0.0;

    MODES_NOTUSED(state);

    for (unsigned i = 0; i < nsamples; ++i) {
        const uint16_t I = abs((int16_t)le16toh(*in++)) & 2047;
        const uint16_t Q = abs((int16_t)le16toh(*in++)) & 2047;
        const double mag = sc16q11_lookup[((I >> LOSE_BITS) << USE_BITS) | (Q >> LOSE_BITS)] / (double)USHRT_MAX);
        *mag_data++ = mag;
        sum_level += mag;
        sum_power += mag * mag;
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

#else /* ! defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_nodc(void *iq_data,
                                 mag_data_t *mag_data,
                                 unsigned nsamples,
                                 struct converter_state *state,
                                 double *out_mean_level,
                                 double *out_mean_power)
{
    MODES_NOTUSED(state);

    const uint16_t *in = iq_data;
    double sum_level = 0.0, sum_power = 0.0;

    for (unsigned i = 0; i < nsamples; ++i) {
        const int16_t I = (int16_t)le16toh(*in++);
        const int16_t Q = (int16_t)le16toh(*in++);
        const double fI = I / 2048.0;
        const double fQ = Q / 2048.0;

        double magsq = fI * fI + fQ * fQ;
        //if (magsq > 1.0)
        //    magsq = 1.0;

        double mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

#endif /* defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_generic(void *iq_data,
                                    mag_data_t *mag_data,
                                    unsigned nsamples,
                                    struct converter_state *state,
                                    double *out_mean_level,
                                    double *out_mean_power)
{
    const uint16_t *in = iq_data;
    double z1_I = state->z1_I;
    double z1_Q = state->z1_Q;
    const double dc_a = state->dc_a;
    const double dc_b = state->dc_b;

    double sum_level = 0.0, sum_power = 0.0;

    for (unsigned i = 0; i < nsamples; ++i) {
        const int16_t I = (int16_t)le16toh(*in++);
        const int16_t Q = (int16_t)le16toh(*in++);
        double fI = I / 2048.0;
        double fQ = Q / 2048.0;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        double magsq = fI * fI + fQ * fQ;
        //if (magsq > 1.0)
        //    magsq = 1.0;

        const double mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static struct {
    input_format_t format;
    int can_filter_dc;
    iq_convert_fn fn;
    const char *description;
    bool (*init)();
} converters_table[] = {
    // In order of preference
    { INPUT_UC8,          0, convert_uc8_nodc,         "UC8, integer/table path", init_uc8_lookup },
    { INPUT_UC8,          1, convert_uc8_generic,      "UC8, float path", NULL },
    { INPUT_SC16,         0, convert_sc16_nodc,        "SC16, float path, no DC", NULL },
    { INPUT_SC16,         1, convert_sc16_generic,     "SC16, float path", NULL },
#if defined(SC16Q11_TABLE_BITS)
    { INPUT_SC16Q11,      0, convert_sc16q11_table,    "SC16Q11, integer/table path", init_sc16q11_lookup },
#else
    { INPUT_SC16Q11,      0, convert_sc16q11_nodc,     "SC16Q11, float path, no DC", NULL },
#endif
    { INPUT_SC16Q11,      1, convert_sc16q11_generic,  "SC16Q11, float path", NULL },
    { 0, 0, NULL, NULL, NULL }
};

iq_convert_fn init_converter(input_format_t format,
                             double sample_rate,
                             int filter_dc,
                             struct converter_state **out_state)
{
    int i;

    for (i = 0; converters_table[i].fn; ++i) {
        if (converters_table[i].format != format)
            continue;
        if (filter_dc && !converters_table[i].can_filter_dc)
            continue;
        break;
    }

    if (!converters_table[i].fn) {
        fprintf(stderr, "no suitable converter for format=%d dc=%d\n",
                format, filter_dc);
        return NULL;
    }

    if (converters_table[i].init) {
        if (!converters_table[i].init())
            return NULL;
    }

    *out_state = malloc(sizeof(struct converter_state));
    if (! *out_state) {
        fprintf(stderr, "can't allocate converter state\n");
        return NULL;
    }

    (*out_state)->z1_I = 0.0;
    (*out_state)->z1_Q = 0.0;

    if (filter_dc) {
        // init DC block @ 1Hz
        (*out_state)->dc_b = exp(-2.0 * M_PI * 1.0 / sample_rate);
        (*out_state)->dc_a = 1.0 - (*out_state)->dc_b;
    } else {
        // if the converter does filtering, make sure it has no effect
        (*out_state)->dc_b = 1.0;
        (*out_state)->dc_a = 0.0;
    }

    return converters_table[i].fn;
}

void cleanup_converter(struct converter_state *state)
{
    free(state);
}
