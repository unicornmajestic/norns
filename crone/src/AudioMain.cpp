//
// Created by emb on 11/19/18.
//

#include <cstring> // hack: faust APIUI should include, but doesn't
#include "faust/gui/APIUI.h"

#include "AudioMain.h"
#include "Commands.h"
#include "Evil.h"


using namespace crone;
using std::cout;
using std::endl;

AudioMain::AudioMain() {
    comp.init(48000);
    reverb.init(48000);
    setDefaultParams();
}

AudioMain::AudioMain(int sampleRate) {
    comp.init(sampleRate);
}


// state constructors
AudioMain::BusList::BusList() {
    for (auto *p: { &adc_out, &dac_in, &adc_monitor, &aux_in, &aux_out, &ins_in, &ins_out}) {
        p->clear();
    }
}

AudioMain::SmoothLevelList::SmoothLevelList() {
    for (auto *p : { &adc, &monitor, &monitor_aux, &aux}) {
        p->setTarget(0);
    }
}

AudioMain::StaticLevelList::StaticLevelList() {
    for (auto &f : monitor_mix) {
        f = 0.f;
    }
}

AudioMain::EnabledList::EnabledList() {
    comp = false;
    reverb = false;
}

/////////////////////////
/// main process function

void AudioMain::processBlock(const float **in_adc, const float **in_ext, float **out, size_t numFrames) {
    // apply pending param changes
    Commands::handlePending(this);

    // clear all our internal busses
    clearBusses(numFrames);

    // FIXME: current faust architecture needs this
    float* pin[2];
    float* pout[2];

    // clear the output
    for(int ch=0; ch<2; ++ch) {
        for (size_t fr=0; fr<numFrames; ++fr) {
            out[ch][fr] = 0.f;
        }
    }

    // apply input levels
    bus.adc_out.mixFrom(in_adc, numFrames, smoothLevels.adc);
    bus.ext_out.mixFrom(in_ext, numFrames, smoothLevels.ext);

    // mix to monitor bus
    bus.adc_monitor.stereoMixFrom(bus.adc_out, numFrames, staticLevels.monitor_mix);

    // mix to aux input
    bus.aux_in.mixFrom(bus.adc_monitor, numFrames, smoothLevels.monitor_aux);
    bus.aux_in.mixFrom(bus.ext_out, numFrames, smoothLevels.ext_aux);

    if (!enabled.reverb) { // bypass aux
        bus.aux_out.sumFrom(bus.aux_in, numFrames);
    } else { // process aux
        // FIXME: arg!
        pin[0] = bus.aux_in.buf[0];
        pin[1] = bus.aux_in.buf[1];
        pout[0] = bus.aux_out.buf[0];
        pout[1] = bus.aux_out.buf[1];
        reverb.processBlock(pin, pout, static_cast<int>(numFrames));
    }

    // mix to insert bus
    bus.ins_in.mixFrom(bus.adc_monitor, numFrames, smoothLevels.monitor);
    bus.ins_in.sumFrom(bus.ext_out, numFrames);
    bus.ins_in.mixFrom(bus.aux_out, numFrames, smoothLevels.aux);

    if(!enabled.comp) { // bypass_insert
        bus.ins_out.sumFrom(bus.ins_in, numFrames);
        bus.dac_in.sumFrom(bus.ins_out, numFrames);
    } else { // process insert
        // FIXME: arg!
        pin[0] = bus.ins_in.buf[0];
        pin[1] = bus.ins_in.buf[1];
        pout[0] = bus.ins_out.buf[0];
        pout[1] = bus.ins_out.buf[1];
        comp.processBlock(pin, pout, static_cast<int>(numFrames));
        // apply insert wet/dry
        bus.dac_in.xfade(bus.ins_in, bus.ins_out, numFrames, smoothLevels.ins_mix);
    }
    // apply final output level
    bus.dac_in.mixTo(out, numFrames, smoothLevels.dac);
}


void AudioMain::setDefaultParams() {

    APIUI *ui;
    ui = &comp.getUi();
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/ratio"), 4.f);
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/threshold"), -20.f);
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/attack"), 0.005f);
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/release"), 0.05f);
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/gain_pre"), 0.0);
    ui->setParamValue(ui->getParamIndex("/StereoCompressor/gain_post"), 4.0);

    ui = &reverb.getUi();
    ui->setParamValue(ui->getParamIndex("/ZitaReverb/pre_del"), 47);
    ui->setParamValue(ui->getParamIndex("/ZitaReverb/lf_fc"), 666);
    ui->setParamValue(ui->getParamIndex("/ZitaReverb/low_rt60"), 3.33);
    ui->setParamValue(ui->getParamIndex("/ZitaReverb/mid_rt60"), 4.44);
    ui->setParamValue(ui->getParamIndex("/ZitaReverb/hf_damp"), 5555);

    if(0) {
        std::vector<Evil::FaustModule> mods;
        Evil::FaustModule mcomp("Compressor", &comp.getUi());
        mods.push_back(mcomp);
        Evil::FaustModule mverb("Reverb", &reverb.getUi());
        mods.push_back(mverb);
        Evil::DO_EVIL(mods); /// ayyehehee
    }
}


void AudioMain::clearBusses(size_t numFrames) {
    bus.adc_out.clear(numFrames);
    bus.ext_out.clear(numFrames);
    bus.dac_in.clear(numFrames);
    bus.ins_in.clear(numFrames);
    bus.ins_out.clear(numFrames);
    bus.aux_in.clear(numFrames);
    bus.aux_out.clear(numFrames);
    bus.adc_monitor.clear(numFrames);
}

void AudioMain::handleCommand(crone::Commands::CommandPacket *p) {
    switch(p->id) {
        case Commands::Id::SET_LEVEL_ADC:
            smoothLevels.adc.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_DAC:
            smoothLevels.dac.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_EXT:
            smoothLevels.ext.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_EXT_AUX:
            smoothLevels.ext_aux.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_AUX_DAC:
            smoothLevels.aux.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_MONITOR:
            smoothLevels.monitor.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_MONITOR_AUX:
            smoothLevels.monitor_aux.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_INS_MIX:
            smoothLevels.ins_mix.setTarget(p->value);
            break;
        case Commands::Id::SET_LEVEL_MONITOR_MIX:
            if(p->voice < 0 || p->voice > 3) { return; }
            staticLevels.monitor_mix[p->voice] = p->value;
            break;
        case Commands::Id::SET_PARAM_REVERB:
            reverb.getUi().setParamValue(p->voice, p->value);
            break;
        case Commands::Id::SET_PARAM_COMPRESSOR:
            comp.getUi().setParamValue(p->voice, p->value);
            break;
        case Commands::Id::SET_ENABLED_REVERB:
            enabled.reverb = p->value > 0.f;
            break;
        case Commands::Id::SET_ENABLED_COMPRESSOR:
            enabled.comp = p->value > 0.f;
            break;
        default:
            ;;
    }
}
