// ============================================================
// UltraComb SAL — AudioWorklet processor
// Signal: Disperser → Freq Shifter + Comb Feedback → Phase Inv → Mix → Limiter
// ============================================================

import { FirstOrderAP, SecondOrderAP } from './dsp.js';

const MAX_POLES = 6;
const HILBERT_ORDER = 4;
const MAX_DELAY = 8192;
const COMB_DELAY_MS = 3.0;

// Broadband Hilbert IIR coefficients (~90° phase diff, 15 Hz–20 kHz @ 44.1k)
const HILT_A = [0.6923878, 0.9360654, 0.9882295, 0.9987488];
const HILT_B = [0.4021921, 0.8561711, 0.9722910, 0.9952885];

class UltraCombWorklet extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'spread',      defaultValue: 0.5,  minValue: 0,     maxValue: 1,    automationRate: 'k-rate' },
      { name: 'poles',        defaultValue: 4,    minValue: 1,     maxValue: 6,    automationRate: 'k-rate' },
      { name: 'density',      defaultValue: 0.5,  minValue: 0,     maxValue: 1,    automationRate: 'k-rate' },
      { name: 'freqShift',    defaultValue: 0,    minValue: -1000, maxValue: 1000, automationRate: 'k-rate' },
      { name: 'feedback',     defaultValue: 0.3,  minValue: 0,     maxValue: 0.95, automationRate: 'k-rate' },
      { name: 'invertPhase',  defaultValue: 0,    minValue: 0,     maxValue: 1,    automationRate: 'k-rate' },
      { name: 'dryWet',       defaultValue: 0.5,  minValue: 0,     maxValue: 1,    automationRate: 'k-rate' },
      { name: 'limiterOn',    defaultValue: 0,    minValue: 0,     maxValue: 1,    automationRate: 'k-rate' },
    ];
  }

  constructor() {
    super();
    this.channels = [this._makeChannel(), this._makeChannel()];
  }

  _makeChannel() {
    const ch = {
      disperser: Array.from({ length: MAX_POLES }, () => new SecondOrderAP()),
      hiltA: HILT_A.map(c => new FirstOrderAP(c)),
      hiltB: HILT_B.map(c => new FirstOrderAP(c)),
      oscPhase: 0,
      delayBuf: new Float32Array(MAX_DELAY),
      delayWrite: 0,
    };
    return ch;
  }

  process(inputs, outputs, params) {
    const input = inputs[0];
    const output = outputs[0];
    if (!input || !input.length) return true;

    const sr = sampleRate;
    const spread     = params.spread[0];
    const poles      = Math.round(params.poles[0]);
    const density    = params.density[0];
    const freqShift  = params.freqShift[0];
    const feedback   = params.feedback[0];
    const invertPh   = params.invertPhase[0] >= 0.5;
    const dryWet     = params.dryWet[0];
    const limiterOn  = params.limiterOn[0] >= 0.5;

    const centreFreq    = 1000;
    const spacingFactor = 1 + spread * 1.5;
    const Q             = 0.5 + density * 7.5;
    const combDelay     = Math.max(1, Math.min(MAX_DELAY - 1, Math.round(COMB_DELAY_MS * 0.001 * sr)));
    const twoPi         = 2 * Math.PI;
    const phaseInc      = twoPi * freqShift / sr;

    const numCh = Math.min(input.length, output.length, 2);

    for (let ch = 0; ch < numCh; ch++) {
      const inp = input[ch];
      const out = output[ch];
      const state = this.channels[ch];
      const len = inp.length;

      // Update disperser coefficients
      for (let i = 0; i < poles; i++) {
        let freq = centreFreq * Math.pow(spacingFactor, i - (poles - 1) * 0.5);
        freq = Math.max(20, Math.min(sr * 0.45, freq));
        state.disperser[i].setParams(freq, Q, sr);
      }

      for (let n = 0; n < len; n++) {
        const dry = inp[n];
        let wet = dry;

        // 1. Disperser
        for (let i = 0; i < poles; i++)
          wet = state.disperser[i].process(wet);

        // 2. Comb with freq shifter in feedback
        let readPos = state.delayWrite - combDelay;
        if (readPos < 0) readPos += MAX_DELAY;
        const delayed = state.delayBuf[readPos];

        let I = delayed, Qi = delayed;
        for (let i = 0; i < HILBERT_ORDER; i++) {
          I  = state.hiltA[i].process(I);
          Qi = state.hiltB[i].process(Qi);
        }

        const cosP = Math.cos(state.oscPhase);
        const sinP = Math.sin(state.oscPhase);
        const shifted = I * cosP - Qi * sinP;

        state.oscPhase += phaseInc;
        if (state.oscPhase > twoPi)     state.oscPhase -= twoPi;
        else if (state.oscPhase < 0)    state.oscPhase += twoPi;

        const combOut = wet + feedback * shifted;
        state.delayBuf[state.delayWrite] = combOut;
        state.delayWrite = (state.delayWrite + 1) % MAX_DELAY;

        wet = combOut;

        // 3. Phase inversion
        if (invertPh) wet = -wet;

        // 4. Dry/Wet
        let sample = dry * (1 - dryWet) + wet * dryWet;

        // 5. Limiter
        if (limiterOn) sample = Math.tanh(sample);

        out[n] = sample;
      }
    }

    return true;
  }
}

registerProcessor('ultracomb-processor', UltraCombWorklet);
