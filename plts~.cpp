//
//  plts.cpp
//  pd-plaits
//
//  Created by javiernonis on 5/1/19.
//  Copyright © 2019 jnonis. All rights reserved.
//

#include "m_pd.h"
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

inline float constrain(float v, float vMin, float vMax) {
    return std::max<float>(vMin,std::min<float>(vMax, v));
}

static t_class *plts_tilde_class;

typedef struct _plts_tilde {
    t_object x_obj;
    t_sample f_dummy;

    t_int model;
    t_float pitch;
    t_float harmonics;
    t_float timbre;
    t_float morph;
    t_float lpg_colour;
    t_float decay;
    bool trigger;
    
    t_float mod_timbre;
    t_float mod_fm;
    t_float mod_morph;
    
    bool frequency_active;
    bool timbre_active;
    bool morph_active;
    bool trigger_active;
    bool level_active;
    
    t_int block_size;
    t_int block_count;
    t_int last_n;
    t_float last_tigger;
    
    t_int last_engine;
    t_int last_engine_perform;
    
    plaits::Voice voice;
    plaits::Patch patch;
    plaits::Modulations modulations;
    char shared_buffer[16384];
    
    t_inlet *x_in2;
    t_inlet *x_in3;
    t_inlet *x_in4;
    t_inlet *x_in5;
    t_inlet *x_in6;
    t_inlet *x_in7;
    t_inlet *x_in8;
    t_inlet *x_in9;
    
    t_outlet *x_out1;
    t_outlet *x_out2;
    t_outlet *x_out_model;
    t_outlet *x_out_model_mod;
} t_plts_tilde;

// Pure data methods, needed because we are using C++
extern "C"  {
    t_int *plts_tilde_perform(t_int *w);
    void plts_tilde_dsp(t_plts_tilde *x, t_signal **sp);
    void plts_tilde_free(t_plts_tilde *x);
    void *plts_tilde_new(t_floatarg f);
    void plts_tilde_setup(void);
    
    void plts_tilde_model(t_plts_tilde *x, t_floatarg f);
    
    void plts_tilde_pitch(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_harmonics(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_timbre(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_morph(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_lpg_colour(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_decay(t_plts_tilde *x, t_floatarg f);
    
    void plts_tilde_mod_timbre(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_mod_fm(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_mod_morph(t_plts_tilde *x, t_floatarg f);
    
    void plts_tilde_trigger(t_plts_tilde *x, t_floatarg f);
    
    void plts_tilde_frequency_active(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_timbre_active(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_morph_active(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_trigger_active(t_plts_tilde *x, t_floatarg f);
    void plts_tilde_level_active(t_plts_tilde *x, t_floatarg f);
}

static const char* modelLabels[16] = {
    "Pair of classic waveforms",
    "Waveshaping oscillator",
    "Two operator FM",
    "Granular formant oscillator",
    "Harmonic oscillator",
    "Wavetable oscillator",
    "Chords",
    "Vowel and speech synthesis",
    "Granular cloud",
    "Filtered noise",
    "Particle noise",
    "Inharmonic string modeling",
    "Modal resonator",
    "Analog bass drum",
    "Analog snare drum",
    "Analog hi-hat",
};

t_int *plts_tilde_perform(t_int *w) {
    t_plts_tilde *x = (t_plts_tilde *) (w[1]);

    t_sample *eng = (t_sample *) (w[3]);
    t_sample *timbre = (t_sample *) (w[4]);
    t_sample *freq = (t_sample *) (w[5]);
    t_sample *morph = (t_sample *) (w[6]);
    t_sample *harmo = (t_sample *) (w[7]);
    t_sample *trig = (t_sample *) (w[8]);
    t_sample *level = (t_sample *) (w[9]);
    t_sample *note = (t_sample *) (w[10]);
    
    t_sample *out = (t_sample *) (w[11]);
    t_sample *aux = (t_sample *) (w[12]);
    int n = (int) (w[13]);
    
    // Determine block size
    if (n != x->last_n) {
        // Plaits uses a block size of 24 max
        if (n > 24) {
            int block_size = 24;
            while (n > 24 && n % block_size > 0) {
                block_size--;
            }
            x->block_size = block_size;
            x->block_count = n / block_size;
        } else {
            x->block_size = n;
            x->block_count = 1;
        }
        x->last_n = n;
    }
    
    // Model
    x->patch.engine = x->model;
    
    // Send current engine
    int active_engine = x->voice.active_engine();
    if (x->last_engine_perform > 128 && x->last_engine != active_engine) {
        outlet_float(x->x_out_model_mod, active_engine);
        x->last_engine = active_engine;
        x->last_engine_perform = 0;
    } else {
        x->last_engine_perform++;
    }
    
    // Calculate pitch for lowCpu mode if needed
    float pitch = x->pitch;
    pitch += log2f(48000.f * (1.f / sys_getsr()));
    // Update patch
    x->patch.note = 60.f + pitch * 12.f;
    x->patch.harmonics = x->harmonics;
    x->patch.timbre = x->timbre;
    x->patch.morph = x->morph;
    x->patch.lpg_colour = x->lpg_colour;
    x->patch.decay = x->decay;
    x->patch.frequency_modulation_amount = x->mod_fm;
    x->patch.timbre_modulation_amount = x->mod_timbre;
    x->patch.morph_modulation_amount = x->mod_morph;
    
    x->modulations.frequency_patched = x->frequency_active ? 1 : 0;
    x->modulations.timbre_patched = x->timbre_active ? 1 : 0;
    x->modulations.morph_patched = x->morph_active ? 1 : 0;
    x->modulations.trigger_patched = x->trigger_active ? 1 : 0;
    x->modulations.level_patched = x->level_active ? 1 : 0;
    
    // Render frames
    for (int j = 0; j < x->block_count; j++) {
        // Update modulations
        x->modulations.engine = eng[x->block_size * j] / 5.f;
        x->modulations.note = note[x->block_size * j] * 12.f;
        x->modulations.frequency = freq[x->block_size * j] * 6.f;
        x->modulations.harmonics = harmo[x->block_size * j] / 5.f;
        x->modulations.timbre = timbre[x->block_size * j] / 8.f;
        x->modulations.morph = morph[x->block_size * j] / 8.f;
        x->modulations.level = level[x->block_size * j] / 8.f;
        // Message trigger
        if (x->trigger) {
            x->modulations.trigger = 1.0f;
            x->trigger = false;
        } else {
            // Triggers at around 0.7 V
            x->modulations.trigger = trig[x->block_size * j] / 3.f;
        }
        
        plaits::Voice::Frame output[x->block_size];
        x->voice.Render(x->patch, x->modulations, output, x->block_size);
        
        for (int i = 0; i < x->block_size; i++) {
            out[i + (x->block_size * j)] = output[i].out / 32768.0f;
            aux[i + (x->block_size * j)] = output[i].aux / 32768.0f;
        }
    }

    return (w + 14);
}

void plts_tilde_dsp(t_plts_tilde *x, t_signal **sp) {
    dsp_add(plts_tilde_perform,
            13,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[2]->s_vec,
            sp[3]->s_vec,
            sp[4]->s_vec,
            sp[5]->s_vec,
            sp[6]->s_vec,
            sp[7]->s_vec,
            sp[8]->s_vec,
            sp[9]->s_vec,
            sp[10]->s_vec,
            sp[0]->s_n);
}

void plts_tilde_free(t_plts_tilde *x) {
    x->voice.FreeEngines();
    inlet_free(x->x_in2);
    inlet_free(x->x_in3);
    inlet_free(x->x_in4);
    inlet_free(x->x_in5);
    inlet_free(x->x_in6);
    inlet_free(x->x_in7);
    inlet_free(x->x_in8);
    inlet_free(x->x_in9);
    outlet_free(x->x_out1);
    outlet_free(x->x_out2);
    outlet_free(x->x_out_model);
    outlet_free(x->x_out_model_mod);
}

void *plts_tilde_new(t_floatarg f) {
    t_plts_tilde *x = (t_plts_tilde *)pd_new(plts_tilde_class);
    
    stmlib::BufferAllocator allocator(x->shared_buffer, sizeof(x->shared_buffer));
    x->voice.Init(&allocator);
    
    x->patch.engine = 0;
    x->patch.lpg_colour = 0.5f;
    x->patch.decay = 0.5f;

    x->model = 0;
    x->pitch = 0;
    x->harmonics = 0;
    x->timbre = 0;
    x->morph = 0;
    x->lpg_colour = 0.5f;
    x->decay = 0.5f;
    x->trigger = false;
    
    x->mod_timbre = 0;
    x->mod_fm = 0;
    x->mod_morph = 0;
    
    x->frequency_active = false;
    x->timbre_active = false;
    x->morph_active = false;
    x->trigger_active = false;
    x->level_active = false;
    
    x->last_engine = 0;
    x->last_engine_perform = 0;
    
    x->x_in2 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in3 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in4 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in5 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in6 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in7 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in8 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_in9 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    
    x->x_out1 = outlet_new(&x->x_obj, &s_signal);
    x->x_out2 = outlet_new(&x->x_obj, &s_signal);
    x->x_out_model = outlet_new(&x->x_obj, &s_symbol);
    x->x_out_model_mod = outlet_new(&x->x_obj, &s_symbol);
    
    return (void *)x;
}

void plts_tilde_model(t_plts_tilde *x, t_floatarg f) {
    x->model = constrain(f, 0, 15);
    outlet_symbol(x->x_out_model, gensym(modelLabels[x->model]));
}

void plts_tilde_pitch(t_plts_tilde *x, t_floatarg f) {
    x->pitch = f;
}

void plts_tilde_harmonics(t_plts_tilde *x, t_floatarg f) {
    x->harmonics = f;
}

void plts_tilde_timbre(t_plts_tilde *x, t_floatarg f) {
    x->timbre = f;
}

void plts_tilde_morph(t_plts_tilde *x, t_floatarg f) {
    x->morph = f;
}

void plts_tilde_trigger(t_plts_tilde *x, t_floatarg f) {
    x->trigger = f >= 1;
}

void plts_tilde_lpg_colour(t_plts_tilde *x, t_floatarg f) {
    x->lpg_colour = f;
}

void plts_tilde_decay(t_plts_tilde *x, t_floatarg f) {
    x->decay = f;
}

void plts_tilde_mod_timbre(t_plts_tilde *x, t_floatarg f) {
    x->mod_timbre = f;
}

void plts_tilde_mod_fm(t_plts_tilde *x, t_floatarg f) {
    x->mod_fm = f;
}

void plts_tilde_mod_morph(t_plts_tilde *x, t_floatarg f) {
    x->mod_morph = f;
}

void plts_tilde_frequency_active(t_plts_tilde *x, t_floatarg f) {
    x->frequency_active = f >= 1;
}

void plts_tilde_timbre_active(t_plts_tilde *x, t_floatarg f) {
    x->timbre_active = f >= 1;
}

void plts_tilde_morph_active(t_plts_tilde *x, t_floatarg f) {
    x->morph_active = f >= 1;
}

void plts_tilde_trigger_active(t_plts_tilde *x, t_floatarg f) {
    x->trigger_active = f >= 1;
}

void plts_tilde_level_active(t_plts_tilde *x, t_floatarg f) {
    x->level_active = f >= 1;
}

void plts_tilde_setup(void) {
    plts_tilde_class = class_new(gensym("plts~"),
                                   (t_newmethod)plts_tilde_new,
                                   (t_method)plts_tilde_free,
                                   sizeof(t_plts_tilde),
                                   CLASS_DEFAULT,
                                   A_DEFFLOAT,
                                   0);
    
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_dsp,
                    gensym("dsp"),
                    A_NULL);
    CLASS_MAINSIGNALIN(plts_tilde_class, t_plts_tilde, f_dummy);

    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_model,
                    gensym("model"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_pitch,
                    gensym("pitch"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_harmonics,
                    gensym("harmonics"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_timbre,
                    gensym("timbre"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_morph,
                    gensym("morph"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_trigger,
                    gensym("trigger"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_lpg_colour,
                    gensym("lpg_colour"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_decay,
                    gensym("decay"),
                    A_DEFFLOAT,
                    A_NULL);
    
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_mod_timbre,
                    gensym("mod_timbre"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_mod_fm,
                    gensym("mod_fm"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_mod_morph,
                    gensym("mod_morph"),
                    A_DEFFLOAT,
                    A_NULL);
    
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_frequency_active,
                    gensym("frequency_active"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_timbre_active,
                    gensym("timbre_active"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_morph_active,
                    gensym("morph_active"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_trigger_active,
                    gensym("trigger_active"),
                    A_DEFFLOAT,
                    A_NULL);
    class_addmethod(plts_tilde_class,
                    (t_method)plts_tilde_level_active,
                    gensym("level_active"),
                    A_DEFFLOAT,
                    A_NULL);
}

