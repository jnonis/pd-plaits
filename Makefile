# Makefile to build class 'helloworld' for Pure Data.
# Needs Makefile.pdlibbuilder as helper makefile for platform-dependent build
# settings and rules.

# library name
lib.name = plts~

common.sources = stmlib/dsp/units.cc \
                 stmlib/utils/random.cc \
                 stmlib/dsp/atan.cc \
                 plaits/dsp/voice.cc \
                 plaits/dsp/engine/additive_engine.cc \
                 plaits/dsp/engine/bass_drum_engine.cc \
                 plaits/dsp/engine/chord_engine.cc \
                 plaits/dsp/engine/fm_engine.cc \
                 plaits/dsp/engine/grain_engine.cc \
                 plaits/dsp/engine/hi_hat_engine.cc \
                 plaits/dsp/engine/modal_engine.cc \
                 plaits/dsp/engine/noise_engine.cc \
                 plaits/dsp/engine/particle_engine.cc \
                 plaits/dsp/engine/snare_drum_engine.cc \
                 plaits/dsp/engine/speech_engine.cc \
                 plaits/dsp/engine/string_engine.cc \
                 plaits/dsp/engine/swarm_engine.cc \
                 plaits/dsp/engine/virtual_analog_engine.cc \
                 plaits/dsp/engine/waveshaping_engine.cc \
                 plaits/dsp/engine/wavetable_engine.cc \
                 plaits/dsp/speech/lpc_speech_synth.cc \
                 plaits/dsp/speech/lpc_speech_synth_controller.cc \
                 plaits/dsp/speech/lpc_speech_synth_phonemes.cc \
                 plaits/dsp/speech/lpc_speech_synth_words.cc \
                 plaits/dsp/speech/naive_speech_synth.cc \
                 plaits/dsp/speech/sam_speech_synth.cc \
                 plaits/dsp/physical_modelling/modal_voice.cc \
                 plaits/dsp/physical_modelling/resonator.cc \
                 plaits/dsp/physical_modelling/pm_string.cc \
                 plaits/dsp/physical_modelling/string_voice.cc \
                 plaits/resources.cc

# input source file (class name == source file basename)
class.sources = plts~.cpp

# all extra files to be included in binary distribution of the library
datafiles = plts~-help.pd README.md

cflags = -DTEST -Wno-unused-local-typedefs -I./

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
include Makefile.pdlibbuilder
