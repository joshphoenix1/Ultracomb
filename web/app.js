// ============================================================
// UltraComb SAL — Web UI controller
// ============================================================

let audioCtx = null;
let workletNode = null;
let sourceNode = null;
let isPlaying = false;

// ---- Parameters ----
const params = {
  spread:      { value: 0.5, min: 0,     max: 1,    default: 0.5 },
  poles:        { value: 4,   min: 1,     max: 6,    default: 4, step: 1 },
  density:      { value: 0.5, min: 0,     max: 1,    default: 0.5 },
  freqShift:    { value: 0,   min: -1000, max: 1000, default: 0 },
  feedback:     { value: 0.3, min: 0,     max: 0.95, default: 0.3 },
  invertPhase:  { value: 0,   min: 0,     max: 1,    default: 0 },
  dryWet:       { value: 0.5, min: 0,     max: 1,    default: 0.5 },
  limiterOn:    { value: 0,   min: 0,     max: 1,    default: 0 },
};

// ---- Audio init ----
async function initAudio() {
  if (audioCtx) return;
  audioCtx = new AudioContext({ sampleRate: 48000 });
  await audioCtx.audioWorklet.addModule('worklet.js');
  workletNode = new AudioWorkletNode(audioCtx, 'ultracomb-processor');
  workletNode.connect(audioCtx.destination);
  syncAllParams();
}

function syncAllParams() {
  if (!workletNode) return;
  for (const [k, v] of Object.entries(params)) {
    const p = workletNode.parameters.get(k);
    if (p) p.setValueAtTime(v.value, audioCtx.currentTime);
  }
}

function setParam(name, value) {
  params[name].value = value;
  if (workletNode) {
    const p = workletNode.parameters.get(name);
    if (p) p.setValueAtTime(value, audioCtx.currentTime);
  }
  updateReadout();
}

// ---- Demo tone ----
function playDemo() {
  if (!audioCtx) return;
  stopSource();

  const osc = audioCtx.createOscillator();
  osc.type = 'sawtooth';
  osc.frequency.value = 110;
  osc.connect(workletNode);
  osc.start();
  sourceNode = osc;
  isPlaying = true;
}

function stopSource() {
  if (sourceNode) {
    sourceNode.stop?.();
    sourceNode.disconnect();
    sourceNode = null;
  }
  isPlaying = false;
}

// ---- File playback ----
async function playFile(file) {
  if (!audioCtx) return;
  stopSource();

  const buf = await file.arrayBuffer();
  const audio = await audioCtx.decodeAudioData(buf);
  const src = audioCtx.createBufferSource();
  src.buffer = audio;
  src.loop = true;
  src.connect(workletNode);
  src.start();
  sourceNode = src;
  isPlaying = true;
}

// ---- Knob behaviour ----
const FULL_RANGE_PX = 180;
const ANGLE_MIN = -140;
const ANGLE_MAX = 140;

function initKnob(el, paramName) {
  const p = params[paramName];
  const indicator = el.querySelector('.indicator');
  let startY, startValue;

  function updateVisual() {
    const norm = (p.value - p.min) / (p.max - p.min);
    const angle = ANGLE_MIN + norm * (ANGLE_MAX - ANGLE_MIN);
    indicator.style.transform = `translateX(-50%) rotate(${angle}deg)`;
  }

  function onPointerDown(e) {
    e.preventDefault();
    initAudio();
    startY = e.clientY;
    startValue = p.value;
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
  }

  function onPointerMove(e) {
    const dy = startY - e.clientY;
    let norm = (startValue - p.min) / (p.max - p.min) + dy / FULL_RANGE_PX;
    norm = Math.max(0, Math.min(1, norm));
    let val = p.min + norm * (p.max - p.min);
    if (p.step) val = Math.round(val / p.step) * p.step;
    setParam(paramName, val);
    updateVisual();
  }

  function onPointerUp() {
    window.removeEventListener('pointermove', onPointerMove);
    window.removeEventListener('pointerup', onPointerUp);
  }

  el.addEventListener('pointerdown', onPointerDown);
  el.addEventListener('dblclick', () => {
    setParam(paramName, p.default);
    updateVisual();
  });

  updateVisual();
}

// ---- XY Pad ----
function initXYPad(el) {
  const cursor = el.querySelector('.cursor');
  const crossV = el.querySelector('.crosshair-v');
  const crossH = el.querySelector('.crosshair-h');

  function updateVisual() {
    const normX = (params.freqShift.value - params.freqShift.min)
                / (params.freqShift.max - params.freqShift.min);
    const normY = 1 - (params.feedback.value - params.feedback.min)
                     / (params.feedback.max - params.feedback.min);
    const px = normX * el.offsetWidth;
    const py = normY * el.offsetHeight;
    cursor.style.left = px + 'px';
    cursor.style.top = py + 'px';
    crossV.style.left = px + 'px';
    crossH.style.top = py + 'px';
  }

  function onPointerDown(e) {
    e.preventDefault();
    initAudio();
    update(e);
    el.setPointerCapture(e.pointerId);
    el.addEventListener('pointermove', update);
    el.addEventListener('pointerup', () => {
      el.removeEventListener('pointermove', update);
    }, { once: true });
  }

  function update(e) {
    const rect = el.getBoundingClientRect();
    const normX = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    const normY = Math.max(0, Math.min(1, 1 - (e.clientY - rect.top) / rect.height));
    setParam('freqShift', params.freqShift.min + normX * (params.freqShift.max - params.freqShift.min));
    setParam('feedback', params.feedback.min + normY * (params.feedback.max - params.feedback.min));
    updateVisual();
  }

  el.addEventListener('pointerdown', onPointerDown);
  updateVisual();
}

// ---- Vertical slider ----
function initVSlider(el, paramName) {
  const p = params[paramName];
  const fill = el.querySelector('.fill');
  const thumb = el.querySelector('.thumb');

  function updateVisual() {
    const norm = (p.value - p.min) / (p.max - p.min);
    const pct = norm * 100;
    fill.style.height = pct + '%';
    thumb.style.bottom = `calc(${pct}% - 7px)`;
  }

  function onPointerDown(e) {
    e.preventDefault();
    initAudio();
    update(e);
    el.setPointerCapture(e.pointerId);
    el.addEventListener('pointermove', update);
    el.addEventListener('pointerup', () => {
      el.removeEventListener('pointermove', update);
    }, { once: true });
  }

  function update(e) {
    const rect = el.getBoundingClientRect();
    const norm = Math.max(0, Math.min(1, 1 - (e.clientY - rect.top) / rect.height));
    setParam(paramName, p.min + norm * (p.max - p.min));
    updateVisual();
  }

  el.addEventListener('pointerdown', onPointerDown);
  el.addEventListener('dblclick', () => {
    setParam(paramName, p.default);
    updateVisual();
  });

  updateVisual();
}

// ---- Toggle buttons ----
function initToggle(el, paramName) {
  el.addEventListener('click', async () => {
    await initAudio();
    const newVal = params[paramName].value >= 0.5 ? 0 : 1;
    setParam(paramName, newVal);
    el.classList.toggle('active', newVal >= 0.5);
  });
}

// ---- Readout ----
function updateReadout() {
  const el = document.getElementById('readout');
  if (!el) return;
  const p = params;
  el.textContent = [
    `Spread: ${p.spread.value.toFixed(2)}`,
    `Poles: ${Math.round(p.poles.value)}`,
    `Density: ${p.density.value.toFixed(2)}`,
    `Freq Shift: ${p.freqShift.value.toFixed(1)} Hz`,
    `Feedback: ${p.feedback.value.toFixed(2)}`,
    `Phase Inv: ${p.invertPhase.value >= 0.5 ? 'ON' : 'OFF'}`,
    `Dry/Wet: ${p.dryWet.value.toFixed(2)}`,
    `Limiter: ${p.limiterOn.value >= 0.5 ? 'ON' : 'OFF'}`,
  ].join('  |  ');
}

// ---- Level meters ----
function initMeters() {
  const analyser = audioCtx?.createAnalyser?.();
  if (!analyser || !workletNode) return;
  // Simple peak detection via analyser on master output
  analyser.fftSize = 256;
  workletNode.connect(analyser);

  const buf = new Float32Array(analyser.fftSize);
  const barL = document.getElementById('meter-l-bar');
  const barR = document.getElementById('meter-r-bar');

  function tick() {
    analyser.getFloatTimeDomainData(buf);
    let peak = 0;
    for (let i = 0; i < buf.length; i++) peak = Math.max(peak, Math.abs(buf[i]));
    const pct = Math.min(100, peak * 100);
    if (barL) barL.style.height = pct + '%';
    if (barR) barR.style.height = pct + '%';
    requestAnimationFrame(tick);
  }
  tick();
}

// ---- Boot ----
document.addEventListener('DOMContentLoaded', () => {
  // Knobs
  initKnob(document.getElementById('knob-spread'), 'spread');
  initKnob(document.getElementById('knob-poles'), 'poles');
  initKnob(document.getElementById('knob-density'), 'density');

  // XY Pad
  initXYPad(document.getElementById('xy-pad'));

  // Toggles
  initToggle(document.getElementById('toggle-phase'), 'invertPhase');
  initToggle(document.getElementById('toggle-limiter'), 'limiterOn');

  // Vertical slider
  initVSlider(document.getElementById('slider-drywet'), 'dryWet');

  // Controls
  document.getElementById('btn-demo')?.addEventListener('click', async () => {
    await initAudio();
    playDemo();
    initMeters();
  });

  document.getElementById('btn-stop')?.addEventListener('click', stopSource);

  document.getElementById('btn-file')?.addEventListener('click', () => {
    const inp = document.createElement('input');
    inp.type = 'file';
    inp.accept = 'audio/*';
    inp.onchange = async () => {
      if (inp.files[0]) {
        await initAudio();
        await playFile(inp.files[0]);
        initMeters();
      }
    };
    inp.click();
  });

  updateReadout();
});
