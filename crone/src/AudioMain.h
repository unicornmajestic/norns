//
// Created by emb on 11/19/18.
//

#ifndef CRONE_AUDIOMAIN_H
#define CRONE_AUDIOMAIN_H

#include "Bus.h"
#include "Commands.h"
#include "effects/StereoCompressor.h"
#include "effects/ZitaReverb.h"

namespace  crone {

    class AudioMain {
        friend class Commands;
        enum {
            MAX_BUF_SIZE = 2048
        };
    public:
        AudioMain();
        AudioMain(int sampleRate);

        void processBlock(
                const float *in_adc[2],
                const float *in_ext[2],
                float *out[2],
                size_t numFrames);
      
      void setDefaultParams();
      
    protected:
        void handleCommand(Commands::CommandPacket *p);
	
    private:
        void clearBusses(size_t numFrames);
	
    private:
        // processors
        StereoCompressor comp;
        ZitaReverb reverb;

        // busses
        typedef Bus<2, MAX_BUF_SIZE> StereoBus;
        struct BusList {
            StereoBus adc_out;
            StereoBus ext_out;
            StereoBus dac_in;
            StereoBus ins_in;
            StereoBus ins_out;
            StereoBus aux_in;
            StereoBus aux_out;
            StereoBus adc_monitor;
            BusList();
        };
        BusList bus;

        // levels
        struct SmoothLevelList{
        public:
            LogRamp adc;
            LogRamp dac;
            LogRamp ext;
            LogRamp monitor;
            LogRamp ext_aux;
            LogRamp ins_mix;
            LogRamp monitor_aux;
            LogRamp aux;
            SmoothLevelList();
        };
        SmoothLevelList smoothLevels;

        struct StaticLevelList {
            float monitor_mix[4];
            StaticLevelList();
        };
        StaticLevelList staticLevels;

        // other state
        struct EnabledList {
            bool reverb;
            bool comp;
            EnabledList();
        };
        EnabledList enabled;
    };
}

#endif //CRONE_AUDIOMAIN_H