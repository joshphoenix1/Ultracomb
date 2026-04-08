// ============================================================
// DSP primitives for UltraComb SAL web demo
// ============================================================

/** First-order all-pass filter (for Hilbert transform)
 *  H(z) = (a + z^-1) / (1 + a*z^-1)  */
export class FirstOrderAP {
  constructor(coeff = 0) {
    this.coeff = coeff;
    this.state = 0;
  }
  reset() { this.state = 0; }
  process(x) {
    const y = this.coeff * x + this.state;
    this.state = x - this.coeff * y;
    return y;
  }
}

/** Second-order all-pass biquad (for disperser)
 *  H(z) = (a2 + a1*z^-1 + z^-2) / (1 + a1*z^-1 + a2*z^-2)  */
export class SecondOrderAP {
  constructor() {
    this.b0 = 1; this.b1 = 0;
    this.a1 = 0; this.a2 = 0;
    this.x1 = 0; this.x2 = 0;
    this.y1 = 0; this.y2 = 0;
  }
  setParams(freq, Q, sr) {
    const w0 = 2 * Math.PI * freq / sr;
    const alpha = Math.sin(w0) / (2 * Q);
    const a0inv = 1 / (1 + alpha);
    this.b0 = (1 - alpha) * a0inv;
    this.b1 = (-2 * Math.cos(w0)) * a0inv;
    this.a1 = this.b1;
    this.a2 = this.b0;
  }
  reset() { this.x1 = this.x2 = this.y1 = this.y2 = 0; }
  process(x) {
    const y = this.b0 * x + this.b1 * this.x1 + this.x2
            - this.a1 * this.y1 - this.a2 * this.y2;
    this.x2 = this.x1; this.x1 = x;
    this.y2 = this.y1; this.y1 = y;
    return y;
  }
}
