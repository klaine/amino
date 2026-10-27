/* On-device parameter cards. Loaded lazily on touch/turn by Schwung. */
(function () {
  /* BEGIN GENERATED CARD_META (tools/gen/gen.py) */
  var META = {"fxTypes":["Off","Chorus","Flanger","Phaser","Tremolo","Drive","Comp","EQ","Delay","Reverb","Shimmer","Trance Gate"],"fx":[null,[["Rate","exp",0.05,5,"Hz"],["Depth","percent"],["Delay","lin",2,30,"ms"],["Feedback","lin",-90,90,"%"],["Spread","lin",0,180,"\u00b0"],["Mode","opts",["Chorus","Ensemble"]]],[["Rate","exp",0.02,10,"Hz"],["Depth","percent"],["Feedback","lin",-95,95,"%"],["Delay","exp",0.1,10,"ms"],["Spread","lin",0,180,"\u00b0"],["Sync","opts",["Off","1/16","1/8","1/4","1/2","1 bar","2 bars","4 bars"]]],[["Rate","exp",0.02,10,"Hz"],["Depth","percent"],["Feedback","lin",-95,95,"%"],["Stages","opts",["4","6","8","12"]],["Center","exp",100,4000,"Hz"],["Spread","lin",0,180,"\u00b0"]],[["Rate","exp",0.1,20,"Hz"],["Depth","percent"],["Shape","opts",["Sine","Tri","Square","SawUp","SawDn","Pump"]],["Sync","opts",["Off","1/32","1/16T","1/16","1/8T","1/8","1/4T","1/4","1/2","1 bar"]],["Stereo","percent"],["Phase","lin",0,360,"\u00b0"]],[["Gain","lin",0,36,"dB"],["Tone","lin",-6,6,"dB"],["Curve","opts",["Soft","Tube","Hard","Fold","Crush"]],["Output","lin",-24,0,"dB"],["Low Cut","exp",20,500,"Hz"],["Crush","percent"]],[["Threshold","lin",-40,0,"dB"],["Ratio","exp",1,20,":1"],["Attack","exp",0.1,100,"ms"],["Release","exp",10,1000,"ms"],["Makeup","lin",0,24,"dB"],["Knee","lin",0,12,"dB"]],[["Low","lin",-15,15,"dB"],["Mid","lin",-15,15,"dB"],["Mid Freq","exp",200,8000,"Hz"],["High","lin",-15,15,"dB"],["Mid Q","exp",0.3,4,""],["Low Cut","exp",20,300,"Hz"]],[["Time","exp",1,2000,"ms"],["Sync","opts",["Off","1/32","1/16T","1/16","1/16d","1/8T","1/8","1/8d","1/4T","1/4","1/4d","1/2","1 bar"]],["Feedback","percent"],["Tone","lin",-100,100,"%"],["Ping-Pong","percent"],["Mod","percent"]],[["Size","percent"],["Decay","exp",0.2,20,"s"],["Damping","percent"],["Pre-Delay","lin",0,200,"ms"],["Mod","percent"],["Width","percent"]],[["Size","percent"],["Decay","exp",0.2,20,"s"],["Damping","percent"],["Shimmer","percent"],["Interval","opts",["-12 st","+5 st","+7 st","+12 st","+19 st","+24 st"]],["Mod","percent"]],[["Pattern","opts",["Classic 8ths","Offbeat 8ths","16th Pulse","32nd Stutter","Rolling Trance","Broken Rave","Long Gate"]],["Timing","opts",["1/8","1/16","1/32"]],["Attack","lin",0,50,"ms"],["Release","lin",0,50,"ms"],["Gate Length","percent"],["Floor","percent"]]],"sources":["Off","Wave Env","Filter Env","Amp Env","LFO1","LFO2","LFO1xMW","LFO2xMW","Velocity","Release Vel","Key","Poly AT","Chan AT","ModWheel","Breath","Foot","Expression","Pitch Bend","Timbre","Sustain","Random","Random Bi","Voice #","Layer Alt","Constant","Flavour","Aux Env"],"targets":["Off","Pitch","Osc1 Pitch","Osc2 Pitch","Osc1 Wave","Osc2 Wave","Osc1 Level","Osc2 Level","Dirt Level","Cutoff","Resonance","Drive","Pan","Amp Level","LFO1 Rate","LFO2 Rate","LFO1 Depth","LFO2 Depth","E1 Decay","E2 Decay","E3 Decay","E1 Attack","E3 Release","Glide","FX Send","XMod"]};
  /* END GENERATED CARD_META */

  function pageValue(values, key) {
    if (!values) return null;
    if (Object.prototype.hasOwnProperty.call(values, key)) return values[key];
    const suffix = ":" + key;
    for (const name in values) {
      if (name.endsWith(suffix)) return values[name];
    }
    return null;
  }

  function cardSize(ctx, o) {
    return {
      w: Math.max(1, Math.floor(Number(o.w || ctx.width || 112))),
      h: Math.max(1, Math.floor(Number(o.h || ctx.height || 48))),
    };
  }

  function width(ctx, text) {
    return typeof ctx.textWidth === "function" ? ctx.textWidth(text) : text.length * 5;
  }

  /* Trim to the pixel budget; the font is proportional (Schwung docs/MODULES.md). */
  function fit(ctx, text, maxW) {
    let s = String(text);
    if (width(ctx, s) <= maxW) return s;
    while (s.length > 1 && width(ctx, s + ".") > maxW) s = s.slice(0, -1);
    return s + ".";
  }

  function printRight(ctx, right, y, text) {
    ctx.print(Math.max(0, right - width(ctx, text)), y, text, 1);
  }

  function frame(ctx, x, y, w, h) {
    ctx.fillRect(x, y, w, 1, 1);
    ctx.fillRect(x, y + h - 1, w, 1, 1);
    ctx.fillRect(x, y, 1, h, 1);
    ctx.fillRect(x + w - 1, y, 1, h, 1);
  }

  /* Unipolar bar: filled from the left edge to `x` in 0..1. */
  function barUni(ctx, x0, y, w, h, x) {
    frame(ctx, x0, y, w, h);
    const fill = Math.round(Math.max(0, Math.min(1, x)) * (w - 4));
    if (fill > 0) ctx.fillRect(x0 + 2, y + 2, fill, h - 4, 1);
  }

  /* Bipolar bar: filled from the centre tick toward `v` in -1..1. */
  function barBi(ctx, x0, y, w, h, v) {
    frame(ctx, x0, y, w, h);
    const mid = x0 + Math.floor(w / 2);
    ctx.fillRect(mid, y - 2, 1, h + 4, 1);
    const half = Math.floor((w - 4) / 2);
    const len = Math.round(Math.max(-1, Math.min(1, v)) * half);
    if (len > 0) ctx.fillRect(mid + 1, y + 2, len, h - 4, 1);
    else if (len < 0) ctx.fillRect(mid + len, y + 2, -len, h - 4, 1);
  }

  function signedPercent(v) {
    return (v >= 0 ? "+" : "") + (v * 100).toFixed(1) + "%";
  }

  function trim(n, digits) {
    return n.toFixed(digits).replace(/\.0+$|(\.\d*?)0+$/, "$1");
  }

  /* Mirrors src/web/format.js formatUnit, so both UIs print one reading. */
  function formatUnit(v, unit) {
    if (unit === "Hz") {
      if (v >= 1000) return trim(v / 1000, v >= 10000 ? 1 : 2) + " kHz";
      return trim(v, v >= 100 ? 0 : v >= 10 ? 1 : 2) + " Hz";
    }
    if (unit === "s") return v < 1 ? trim(v * 1000, 0) + " ms" : trim(v, v >= 10 ? 1 : 2) + " s";
    if (unit === "ms") return v >= 1000 ? trim(v / 1000, 2) + " s" : trim(v, v >= 100 ? 0 : 1) + " ms";
    if (unit === ":1") return trim(v, v >= 10 ? 0 : 1) + ":1";
    if (unit === "°") return Math.round(v) + "°";
    if (unit === "dB") return (v > 0.05 ? "+" : "") + trim(v, 1) + " dB";
    if (unit === "%") return Math.round(v) + "%";
    const text = trim(v, Math.abs(v) >= 100 ? 0 : 2);
    return unit ? text + " " + unit : text;
  }

  /* One FX knob reading from its generated row [name, kind, ...] (fx.c curves). */
  function fxReading(row, x) {
    const c = Math.max(0, Math.min(1, x));
    switch (row[1]) {
      case "opts": {
        const n = row[2].length;
        return row[2][Math.max(0, Math.min(n - 1, Math.floor(c * (n - 1) + 0.5)))];
      }
      case "lin": return formatUnit(row[2] + (row[3] - row[2]) * c, row[4]);
      case "exp": return formatUnit(row[2] * Math.pow(row[3] / row[2], c), row[4]);
      default: return Math.round(c * 100) + "%";
    }
  }

  function unavailable(ctx, w, h, title) {
    ctx.fillRect(0, 0, w, h, 0);
    ctx.print(3, 1, title, 1);
    ctx.print(3, Math.max(12, Math.floor(h / 2)), "Value unavailable", 1);
  }

  function drawWaveCard(ctx, o, osc) {
    const { w, h } = cardSize(ctx, o);
    ctx.fillRect(0, 0, w, h, 0);
    const label = "OSC" + osc + " WAVE";
    ctx.print(3, 1, label, 1);
    if (o.raw == null) {
      ctx.print(3, Math.max(12, Math.floor(h / 2)), "Value unavailable", 1);
      return;
    }

    const values = o.values || {};
    const waveKey = osc === 1 ? "wave_view" : "wave_view2";
    const hex = pageValue(values, waveKey);
    if (typeof hex !== "string" || hex.length < 8 || (hex.length & 1)) {
      ctx.print(3, Math.max(12, Math.floor(h / 2)), "Preview unavailable", 1);
      return;
    }

    const count = Math.min(128, hex.length / 2);
    const left = 3, right = Math.max(left + 1, w - 3);
    const top = 11, bottom = Math.max(top + 1, h - 8);
    const mid = Math.floor((top + bottom) / 2);
    ctx.fillRect(left, mid, right - left, 1, 1);
    for (let i = 0; i < count; i++) {
      const sample = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
      if (!Number.isFinite(sample)) continue;
      const x = left + Math.round(i * (right - left - 1) / Math.max(1, count - 1));
      const y = Math.round(mid - ((sample - 128) / 128) * ((bottom - top) / 2));
      const y0 = Math.min(mid, y);
      const height = Math.max(1, Math.abs(y - mid));
      ctx.fillRect(x, y0, 1, height, 1);
    }

    const raw = Number(o.raw);
    const position = Number.isFinite(raw) ? Math.round(raw) : String(o.value || "");
    const table = pageValue(values, "osc" + osc + "_table");
    const footer = "POS " + position + (table == null ? "" : "  TBL " + table);
    ctx.print(3, h - 7, footer.slice(0, 20), 1);
  }

  /* FX P1-P6: the effect's own name for the knob and its value in real units. */
  function drawFxCard(ctx, o) {
    const { w, h } = cardSize(ctx, o);
    const m = /FX (\d) P(\d)/.exec(String(o.name || ""));
    const slot = m ? Number(m[1]) : 1;
    const index = m ? Number(m[2]) - 1 : 0;
    const raw = Number(o.raw);
    if (o.raw == null || !Number.isFinite(raw)) {
      unavailable(ctx, w, h, "FX" + slot);
      return;
    }
    ctx.fillRect(0, 0, w, h, 0);
    const type = Math.round(Number(pageValue(o.values, "fx" + slot + "_type")));
    const rows = META.fx && META.fx[type];
    const row = rows && rows[index];
    const typeName = META.fxTypes && META.fxTypes[type];
    ctx.print(3, 1, fit(ctx, ("FX" + slot + " " + (typeName || "")).toUpperCase(), w - 6), 1);
    if (!row) {
      ctx.print(3, 13, "P" + (index + 1), 1);
      printRight(ctx, w - 3, 13, Math.round(raw * 100) + "%");
      barUni(ctx, 3, 26, w - 6, 7, raw);
      return;
    }
    const reading = fxReading(row, raw);
    ctx.print(3, 13, fit(ctx, row[0], w - 10 - width(ctx, reading)), 1);
    printRight(ctx, w - 3, 13, reading);
    if (row[1] === "opts") {
      const n = row[2].length;
      const i = Math.max(0, Math.min(n - 1, Math.floor(raw * (n - 1) + 0.5)));
      ctx.print(3, h - 9, (i + 1) + " of " + n, 1);
      barUni(ctx, 3, 26, w - 6, 7, n > 1 ? i / (n - 1) : 0);
    } else {
      barUni(ctx, 3, 26, w - 6, 7, raw);
    }
  }

  function drawRouteCard(ctx, o, title, source, target, viaName) {
    const { w, h } = cardSize(ctx, o);
    const raw = Number(o.raw);
    if (o.raw == null || !Number.isFinite(raw)) {
      unavailable(ctx, w, h, title);
      return;
    }
    ctx.fillRect(0, 0, w, h, 0);
    ctx.print(3, 1, title, 1);
    if (viaName) printRight(ctx, w - 3, 1, fit(ctx, "via " + viaName, w / 2));
    ctx.print(3, 12, fit(ctx, source + " > " + target, w - 6), 1);
    barBi(ctx, 3, 25, w - 6, 7, raw);
    const text = signedPercent(raw);
    ctx.print(Math.max(0, Math.floor((w - width(ctx, text)) / 2)), h - 9, text, 1);
  }

  function optionName(list, value, fallback) {
    const i = Math.round(Number(value));
    return Array.isArray(list) && Number.isFinite(i) && list[i] !== undefined ? list[i] : fallback;
  }

  /* Mod n Amount: which route the knob scales, and how far either way. */
  function drawModCard(ctx, o) {
    const m = /Mod (\d)/.exec(String(o.name || ""));
    const slot = m ? Number(m[1]) : 1;
    const values = o.values || {};
    const source = optionName(META.sources, pageValue(values, "m" + slot + "_src"), "Src");
    const target = optionName(META.targets, pageValue(values, "m" + slot + "_dst"), "Dst");
    const via = Math.round(Number(pageValue(values, "m" + slot + "_via")));
    const viaName = via > 0 ? optionName(META.sources, via, "") : "";
    drawRouteCard(ctx, o, "MOD " + slot, source, target, viaName);
  }

  /* Aux Env Amount n: the dedicated Aux Env route beside its target knob. */
  function drawAuxCard(ctx, o) {
    const m = /Amount (\d)/.exec(String(o.name || ""));
    const route = m ? Number(m[1]) : 1;
    const target = optionName(META.targets, pageValue(o.values, "e4_dst" + route), "Dst");
    drawRouteCard(ctx, o, "AUX ROUTE " + route, "Aux Env", target, "");
  }

  globalThis.osc1_wave_card = function (ctx, o) { drawWaveCard(ctx, o, 1); };
  globalThis.osc2_wave_card = function (ctx, o) { drawWaveCard(ctx, o, 2); };
  globalThis.fx_param_card = drawFxCard;
  globalThis.mod_amt_card = drawModCard;
  globalThis.aux_amt_card = drawAuxCard;
})();
