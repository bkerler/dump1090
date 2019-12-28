// Part of dump1090, a Mode S message decoder for SOAPYSDR devices.
//
// sdr_soapy.c: SOAPYSDR dongle support
//
// Copyright (c) 2014-2017 Oliver Jowett <oliver@mutability.co.uk>
// Copyright (c) 2017 FlightAware LLC
// Copyright (c) 2019 Bjoern Kerler <info@revskills.de>
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

// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dump1090.h"
#include "sdr_soapysdr.h"

typedef int16_t sc16_t[2];

static struct {
    SoapySDRDevice  *dev;
    SoapySDRStream  *stream;
    int ppm_error;
    unsigned decimation;
    unsigned lpf_bandwidth;
    char antenna_name[256];
    iq_convert_fn converter;
    struct converter_state *converter_state;
} SOAPYSDR;

//
// =============================== SOAPYSDR handling ==========================
//

void SOAPYSDRInitConfig()
{
    SOAPYSDR.dev = NULL;
    SOAPYSDR.decimation=1;
    SOAPYSDR.lpf_bandwidth=2500000;
    SOAPYSDR.ppm_error = 0;
    SOAPYSDR.converter = NULL;
    SOAPYSDR.converter_state = NULL;
}

static void show_SOAPYSDR_devices()
{
    size_t length;
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);
    fprintf(stderr, "SOAPYSDR: found %d device(s):\n", (int)length);
    for (size_t i = 0; i < length; i++)
        {
            printf("Device #%d: ", (int)i);
            for (size_t j = 0; j < results[i].size; j++)
            {
                printf("%s=%s, ", results[i].keys[j], results[i].vals[j]);
            }
            printf("\n");
        }
    SoapySDRKwargsList_clear(results, length);
}

static int find_device_index(char *s)
{
    size_t length;
    int r;
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);

    char *s2;
    int device = (int)strtol(s, &s2, 10);
    if (device<length)
    {
        printf("Scanning %d\n",device);
        for (size_t j = 0; j < results[device].size; j++)
        {
            char* key=results[device].keys[j];
            r=strcmp(key,"driver");
            if (r==0)
            {
                printf("Using device: %s\n",results[device].vals[j]);
                SoapySDRKwargsList_clear(results, length);
                return device;
            }
        }
    }
    else
    {
        printf("Couldn't find device index. Options are:\n");
        for (size_t i = 0; i < length; i++)
        {
            printf("Device #%d: ", (int)i);
            for (size_t j = 0; j < results[i].size; j++)
            {
                printf("%s=%s, ", results[i].keys[j], results[i].vals[j]);
            }
            printf("\n");
        }
        SoapySDRKwargsList_clear(results, length);
        return -1;
    }
    SoapySDRKwargsList_clear(results, length);
    return device;
}

void SOAPYSDRShowHelp()
{
    printf("      SOAPYSDR-specific options (use with --device-type SOAPYSDR)\n");
    printf("\n");
    printf("--device <index>         select device by index number\n");
    printf("--ppm <correction>       set oscillator frequency correction in PPM\n");
    printf("--antenna <antenna_name> set antenna\n");
    printf("--decimation <N> assume FPGA decimates by a factor of N\n");
    printf("--bandwidth <hz> set LPF bandwidth ('bypass' to bypass the LPF)\n");
    printf("\n");
}

bool SOAPYSDRHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j +1  < argc);

    if (!strcmp(argv[j], "--ppm") && more) {
        SOAPYSDR.ppm_error = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--antenna") && more) {
        strncpy(SOAPYSDR.antenna_name,argv[++j],sizeof(argv[++j])<256?sizeof(argv[++j]):256);
    } else if (!strcmp(argv[j], "--decimation") && more) {
        SOAPYSDR.decimation = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--bandwidth") && more) {
        ++j;
        if (!strcasecmp(argv[j], "bypass")) {
            SOAPYSDR.lpf_bandwidth = 0;
        } else {
            SOAPYSDR.lpf_bandwidth = atoi(argv[j]);
        }
    } else {
        return false;
    }

    *jptr = j;
    return true;
}

bool set_gain(SoapySDRDevice *dev, float gain) {
    int r=0;
    if (gain!=0.0)
    {
        double value = gain / 10.0; // tenths of dB -> dB
        r = (int)SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, value);

        if (r != 0) {
            fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
        }
    }
    else
    {

        char *driver = SoapySDRDevice_getDriverKey(dev);
        fprintf(stdout, "Trying to set auto-gain for %s\n",driver);

        if (strcmp(driver, "RTLSDR") == 0) {
            // For now, set 40.0 dB, high
            // Note: 26.5 dB in https://github.com/librtlsdr/librtlsdr/blob/master/src/tuner_r82xx.c#L1067 - but it's not the same
            // TODO: remove or change after auto-gain? https://github.com/pothosware/SoapyRTLSDR/issues/21 rtlsdr_set_tuner_gain_mode(dev, 0);
            r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, 40.);
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
            } else {
                fprintf(stderr, "Tuner gain semi-automatically set to 40 dB\n");
            }

        } else if (strcmp(driver, "HackRF") == 0) {
            // HackRF has three gains LNA, VGA, and AMP, setting total distributes amongst, 116.0 dB seems to work well,
            // even though it logs HACKRF_ERROR_INVALID_PARAM? https://github.com/rxseger/rx_tools/issues/9
            // Total gain is distributed amongst all gains, 116 = 37,65,1; the LNA is OK (<40) but VGA is out of range (65 > 62)
            // TODO: generic means to set all gains, of any SDR? string parsing LNA=#,VGA=#,AMP=#?
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "LNA", 40.); // max 40
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "VGA", 20.); // max 65
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set VGA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "AMP", 0.); // on or off
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set AMP tuner gain.\n");
            }

        } else if (strcmp(driver, "LimeSDR-USB") == 0)
        {
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "LNA", 20.); // 0.0 - 30.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "PGA", 10.); // -12.0 - 19.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set PGA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "TIA", 2.); // 0.0 - 12.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set TIA tuner gain.\n");
            }
            /*r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_TX, 0, "PAD", 0.); // -52.0 - 0.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set PAD tuner gain.\n");
            }*/
        } else if (strcmp(driver, "FT601") == 0) //LimeSDR-Mini
        {
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "LNA", 20.); // 0.0 - 30.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "PGA", 10.); // -12.0 - 19.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set PGA tuner gain.\n");
            }
            r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "TIA", 12.); // 0.0 - 12.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set TIA tuner gain.\n");
            }
            /*r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_TX, 0, "PAD", 0.); // -52.0 - 0.0
            if (r != 0) {
                fprintf(stderr, "WARNING: Failed to set PAD tuner gain.\n");
            }*/
        }
    } 
    return r;
}

int set_freq_correction(SoapySDRDevice *dev, int ppm) {
    int r = 0;
    if (ppm == 0) 
    {
        return 0;
    }
    r = SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, 0, "CORR", (double)ppm, NULL);
    if (r != 0) 
    {
        fprintf(stderr, "WARNING: Failed to set ppm error.\n");
    } 
    else 
    {
        fprintf(stderr, "Tuner error set to %i ppm.\n", ppm);
    }
    return r;
}

int tune(SoapySDRDevice *dev, double freq) {
    SoapySDRKwargs args = {};
    int r = 0;

    r = (int)SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, 0, (double)freq, &args);
    if (r != 0) {
        fprintf(stderr, "Tuning to %f Hz failed!\n", freq);
        return -1;
        }

    return r; //(r < 0) ? 0 : 1;
}

bool SOAPYSDROpen(void) {
    size_t length;
    int r;
    
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);

    if (!length) {
        fprintf(stderr, "SOAPYSDR: no supported devices found.\n");
        return false;
    }

    int dev_index = 0;
    if (Modes.dev_name) {
        if ((dev_index = find_device_index(Modes.dev_name)) < 0) {
            fprintf(stderr, "SOAPYSDR: no device matching '%s' found.\n", Modes.dev_name);
            show_SOAPYSDR_devices();
            return false;
        }
    }

    SOAPYSDR.dev = SoapySDRDevice_make(&results[dev_index]);
    SoapySDRKwargsList_clear(results, length);

    if (SOAPYSDR.dev==NULL) {
        fprintf(stderr, "SOAPYSDR: error opening the SOAPYSDR device: %s\n",
            strerror(errno));
        return false;
    }

    // Set gain, frequency, sample rate, and reset the device
    if (Modes.gain == MODES_AUTO_GAIN) {
            fprintf(stderr, "SOAPYSDR: enabling tuner AGC\n");
            set_gain(SOAPYSDR.dev,0);
    } else {
            set_gain(SOAPYSDR.dev, Modes.gain);
    }

    if (SOAPYSDR.lpf_bandwidth>0)
    {
        double bw = SoapySDRDevice_getBandwidth(SOAPYSDR.dev, SOAPY_SDR_RX, 0);
        fprintf(stderr, "Bandwidth was: %.0f\n", bw);
        r = SoapySDRDevice_setBandwidth(SOAPYSDR.dev, SOAPY_SDR_RX, 0, SOAPYSDR.lpf_bandwidth);
        if (r != 0)
                fprintf(stderr, "SoapySDRDevice_setBandwidth: %s (%d)\n", SoapySDR_errToStr(r), r);
        double bandwidth = SoapySDRDevice_getBandwidth(SOAPYSDR.dev, SOAPY_SDR_RX, 0);
        fprintf(stderr, "Bandwidth set to: %.0f\n", bandwidth);
    }

    if (strlen(SOAPYSDR.antenna_name)>0)
    {
        fprintf(stderr, "Using antenna '%s' on channel %i\n", SOAPYSDR.antenna_name, 0);
        r = SoapySDRDevice_setAntenna(SOAPYSDR.dev, SOAPY_SDR_RX, 0, SOAPYSDR.antenna_name);
    }
    if (set_freq_correction(SOAPYSDR.dev, SOAPYSDR.ppm_error)==-1) return false;
    if (tune(SOAPYSDR.dev, Modes.freq)==-1) return false;
    r = SoapySDRDevice_setSampleRate(SOAPYSDR.dev, SOAPY_SDR_RX, 0, (unsigned)Modes.sample_rate);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set sample rate.\n");

    //SOAPYSDR_reset_buffer(SOAPYSDR.dev);
    char** names = SoapySDRDevice_listGains(SOAPYSDR.dev, SOAPY_SDR_RX, 0, &length);
    fprintf(stderr,"Rx gains: ");
    for(int i = 0; i < length; i++) {
    fprintf(stderr," - %s: %.2f dB\n", names[i], SoapySDRDevice_getGainElement(SOAPYSDR.dev, SOAPY_SDR_RX, 0, names[i]));
    }
    SoapySDRStrings_clear(&names,length);

    SOAPYSDR.converter = init_converter(INPUT_SC16,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &SOAPYSDR.converter_state);
    if (!SOAPYSDR.converter) {
        fprintf(stderr, "SOAPYSDR: can't initialize sample converter\n");
        SoapySDRDevice_unmake(SOAPYSDR.dev);
        return false;
    }
    return true;
}


static struct timespec SOAPYSDR_thread_cpu;

void SOAPYSDRRun()
{
    sc16_t samples[MODES_MAG_BUF_SAMPLES];
    static unsigned timeouts = 0;
    static uint64_t nextTimestamp = 0;
    static bool dropping = false;
    static struct timespec thread_cpu;

    if (!SOAPYSDR.dev) {
        return;
    }

    SoapySDRKwargs args = {};
    if (SoapySDRDevice_setupStream(SOAPYSDR.dev, &SOAPYSDR.stream, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, &args) != 0) {
        fprintf(stderr, "WARNING: Failed to run streaming.\n");
        exit(1);
    }

    SoapySDRDevice_activateStream(SOAPYSDR.dev, SOAPYSDR.stream, 0, 0, 0);

    start_cpu_timing(&SOAPYSDR_thread_cpu);
    timeouts = 0;
    uint64_t entryTimestamp = mstime();
    pthread_mutex_lock(&Modes.data_mutex);

    while (!Modes.exit) {
        pthread_mutex_unlock(&Modes.data_mutex);
        long long timeNs = 0;
        int flags = 0;
        long timeoutNs = 1000000;
        void* buffs[]={samples};
        int nSamples=SoapySDRDevice_readStream(SOAPYSDR.dev, SOAPYSDR.stream, buffs, MODES_MAG_BUF_SAMPLES, &flags, &timeNs, timeoutNs);
        pthread_mutex_lock(&Modes.data_mutex);
        if (nSamples == -1) {
            fprintf(stderr, "SoapySDRDevice_RecvStream failed: %s\n", SoapySDRDevice_lastError());

            // could be timeout? or another error? I don't know how to tell so lets just
            // quit after receiving too many errors but assume they are timeouts otherwise
            if (++timeouts > 100) {
                break;
            } else {
                continue;
            }
        }

        unsigned next_free_buffer = (Modes.first_free_buffer + 1) % MODES_MAG_BUFFERS;
        struct mag_buf *outbuf = &Modes.mag_buffers[Modes.first_free_buffer];
        struct mag_buf *lastbuf = &Modes.mag_buffers[(Modes.first_free_buffer + MODES_MAG_BUFFERS - 1) % MODES_MAG_BUFFERS];
        unsigned free_bufs = (Modes.first_filled_buffer - next_free_buffer + MODES_MAG_BUFFERS) % MODES_MAG_BUFFERS;

        if (free_bufs == 0 || (dropping && free_bufs < MODES_MAG_BUFFERS/2)) {
            // FIFO is full. Drop this block.
            dropping = true;
            continue;
        }

        dropping = false;
        pthread_mutex_unlock(&Modes.data_mutex);

        // Copy trailing data from last block (or reset if not valid)
        if (outbuf->dropped == 0) {
            memcpy(outbuf->data, lastbuf->data + lastbuf->length, Modes.trailing_samples * sizeof(uint16_t));
        } else {
            memset(outbuf->data, 0, Modes.trailing_samples * sizeof(uint16_t));
        }

        // start handling metadata blocks
        outbuf->dropped = 0;
        outbuf->length = 0;
        outbuf->mean_level = outbuf->mean_power = 0;

        // Compute the sample timestamp for the start of the block
        outbuf->sampleTimestamp = nextTimestamp * 12e6 / Modes.sample_rate / SOAPYSDR.decimation;

        // Convert a block of data
        double mean_level, mean_power;
        SOAPYSDR.converter(samples, &outbuf->data[Modes.trailing_samples + outbuf->length], nSamples, SOAPYSDR.converter_state, &mean_level, &mean_power);
        outbuf->length += nSamples;
        outbuf->mean_level += mean_level;
        outbuf->mean_power += mean_power;
        nextTimestamp += nSamples * SOAPYSDR.decimation;
        timeouts = 0;

        // Get the approx system time for the start of this block
        unsigned block_duration = 1e3 * outbuf->length / Modes.sample_rate;
        outbuf->sysTimestamp = entryTimestamp - block_duration;

        // Push the new data to the demodulation thread
        pthread_mutex_lock(&Modes.data_mutex);

        // accumulate CPU while holding the mutex, and restart measurement
        end_cpu_timing(&thread_cpu, &Modes.reader_cpu_accumulator);
        start_cpu_timing(&thread_cpu);

        Modes.mag_buffers[next_free_buffer].dropped = 0;
        Modes.mag_buffers[next_free_buffer].length = 0;  // just in case
        Modes.first_free_buffer = next_free_buffer;

        pthread_cond_signal(&Modes.data_cond);
    }

    pthread_mutex_unlock(&Modes.data_mutex);
    if (SOAPYSDR.stream) {
        SoapySDRDevice_deactivateStream(SOAPYSDR.dev, SOAPYSDR.stream, 0, 0);
        SoapySDRDevice_closeStream(SOAPYSDR.dev, SOAPYSDR.stream);
        SOAPYSDR.stream = NULL;
    }
}

void SOAPYSDRClose()
{
    if (SOAPYSDR.dev) {
        SoapySDRDevice_unmake(SOAPYSDR.dev);
        SOAPYSDR.dev = NULL;
    }

    if (SOAPYSDR.converter) {
        cleanup_converter(SOAPYSDR.converter_state);
        SOAPYSDR.converter = NULL;
        SOAPYSDR.converter_state = NULL;
    }
}
