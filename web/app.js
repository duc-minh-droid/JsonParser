// Replays a `jsonparser --trace` document. State at step i is rebuilt by
// folding events[0..i]; traces are small, so this stays simple and exact.
(function () {
  "use strict";

  const FN = {
    document: "parse()",
    value: "parseValue()",
    object: "parseObject()",
    member: "parseObject() loop",
    array: "parseArray()",
    string: "parseStringLiteral()",
    number: "parseNumber()",
    literal: "expectLiteral()",
  };
  const TOK_CLASS = {
    LBRACE: "tok-punct", RBRACE: "tok-punct", LBRACKET: "tok-punct", RBRACKET: "tok-punct",
    COLON: "tok-punct", COMMA: "tok-punct", STRING: "tok-string", NUMBER: "tok-number",
    TRUE: "tok-literal", FALSE: "tok-literal", NULL: "tok-literal", EOF: "",
  };

  const $ = (id) => document.getElementById(id);
  const el = {
    samples: $("samples"), source: $("source"), errorBox: $("errorBox"), cursorInfo: $("cursorInfo"),
    stack: $("stack"), log: $("log"), tree: $("tree"), treeWrap: $("treeWrap"), edges: $("edges"),
    scrub: $("scrub"), stepLabel: $("stepLabel"), play: $("btnPlay"), speed: $("speed"), eventBar: $("eventBar"),
  };

  let trace = null;     // current trace document
  let b2c = [];         // byte offset -> UTF-16 index into trace.input
  let chars = [];       // per-character <span>s
  let step = 0;         // number of events applied
  let timer = null;
  let nodeEls = new Map();

  // ---------- loading ----------
  function buildByteMap(str) {
    const map = [];
    const enc = new TextEncoder();
    let i = 0;
    for (const ch of str) {
      const n = enc.encode(ch).length;
      for (let k = 0; k < n; k++) map.push(i);
      i += ch.length;
    }
    map.push(i);
    return map;
  }
  const toChar = (byte) => b2c[Math.min(byte, b2c.length - 1)];

  function lineColOf(charIdx) {
    let line = 1, col = 1;
    const s = trace.input;
    for (let i = 0; i < charIdx && i < s.length; i++) {
      if (s[i] === "\n") { line++; col = 1; } else if ((s.charCodeAt(i) & 0xfc00) !== 0xdc00) col++;
    }
    return line + ":" + col;
  }

  function load(t) {
    stop();
    trace = t;
    b2c = buildByteMap(t.input);
    step = 0;

    // source: one span per UTF-16 unit, wrapped in line blocks
    el.source.innerHTML = "";
    chars = [];
    let line = document.createElement("span");
    line.className = "ln";
    el.source.appendChild(line);
    const s = t.input.replace(/\s+$/, "");
    for (let i = 0; i < s.length; i++) {
      const c = s[i];
      if (c === "\n") {
        const sp = document.createElement("span");
        sp.className = "ch nl";
        sp.textContent = " ";
        line.appendChild(sp);
        chars.push(sp);
        line = document.createElement("span");
        line.className = "ln";
        el.source.appendChild(line);
        continue;
      }
      const sp = document.createElement("span");
      sp.className = "ch";
      sp.textContent = c;
      line.appendChild(sp);
      chars.push(sp);
    }
    const eof = document.createElement("span");
    eof.className = "ch eof";
    eof.textContent = " ";
    line.appendChild(eof);
    while (chars.length < t.input.length) chars.push(eof);
    chars.push(eof);

    el.tree.querySelectorAll(".tnode").forEach((n) => n.remove());
    nodeEls = new Map();
    el.scrub.max = t.events.length;
    el.scrub.value = 0;
    [...el.samples.children].forEach((b) => b.classList.toggle("active", b.dataset.name === t.name));
    render();
  }

  // ---------- state ----------
  function stateAt(n) {
    const st = { stack: [], nodes: new Map(), order: [], tokens: [], cursor: 0, curTok: null, error: null, last: null, fresh: null };
    for (let i = 0; i < n; i++) {
      const e = trace.events[i];
      st.last = e;
      st.fresh = null;
      st.curTok = null;
      switch (e.ev) {
        case "enter":
          st.stack.push({ rule: e.rule, pos: e.pos, node: null });
          st.cursor = e.pos;
          break;
        case "exit": {
          const f = st.stack.pop();
          if (f && f.node !== null && st.nodes.has(f.node)) st.nodes.get(f.node).open = false;
          st.cursor = e.pos;
          break;
        }
        case "token":
          st.tokens.push(e);
          st.curTok = e;
          st.cursor = e.end;
          break;
        case "node": {
          const nd = { ...e, children: [], open: e.cls === "JsonObject" || e.cls === "JsonArray" };
          st.nodes.set(e.id, nd);
          st.order.push(e.id);
          if (e.parent !== null && st.nodes.has(e.parent)) st.nodes.get(e.parent).children.push(e.id);
          // containers belong to the innermost object/array frame
          if (nd.open) {
            for (let k = st.stack.length - 1; k >= 0; k--) {
              if (st.stack[k].rule === "object" || st.stack[k].rule === "array") { st.stack[k].node = e.id; break; }
            }
          }
          st.fresh = e.id;
          break;
        }
        case "error":
          st.error = e;
          st.cursor = e.pos;
          break;
      }
    }
    return st;
  }

  // ---------- rendering ----------
  function esc(s) {
    return String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  }

  function describe(e) {
    if (!e) return '<span class="kind exit">start</span>press Play or use the arrow keys to step through the parser';
    const at = (p) => lineColOf(toChar(p));
    switch (e.ev) {
      case "enter": return `<span class="kind enter">push</span>${esc(FN[e.rule] || e.rule)} &nbsp;<span style="color:var(--muted)">grammar rule <b>${esc(e.rule)}</b> at ${at(e.pos)}</span>`;
      case "exit": return `<span class="kind exit">pop</span>${esc(FN[e.rule] || e.rule)} returns &nbsp;<span style="color:var(--muted)">cursor now at ${at(e.pos)}</span>`;
      case "token": return e.tok === "EOF"
        ? `<span class="kind token">token</span>EOF &nbsp;<span style="color:var(--muted)">nothing left after the top-level value</span>`
        : `<span class="kind token">token</span>${e.tok} <b>${esc(e.text)}</b> &nbsp;<span style="color:var(--muted)">bytes ${e.start}..${e.end}, line ${e.line} col ${e.col}</span>`;
      case "node": {
        const slot = e.parent === null ? "root" : e.key !== undefined ? `#${e.parent}["${esc(e.key)}"]` : `#${e.parent}[${e.index}]`;
        return `<span class="kind node">new</span>${e.cls}(${esc(e.preview)}) &nbsp;<span style="color:var(--muted)">node #${e.id} &rarr; ${slot}</span>`;
      }
      case "error": return `<span class="kind error">error</span>line ${e.line}, col ${e.col}: ${esc(e.message)}`;
    }
    return "";
  }

  function logLine(e) {
    switch (e.ev) {
      case "enter": return `<span class="k enter">push</span>${esc(e.rule)}`;
      case "exit": return `<span class="k">pop</span>${esc(e.rule)}`;
      case "token": return `<span class="k token">tok</span>${e.tok} ${esc(e.text)}`;
      case "node": return `<span class="k node">new</span>${e.cls} #${e.id} ${esc(e.preview)}`;
      case "error": return `<span class="k error">err</span>${e.line}:${e.col} ${esc(e.message)}`;
    }
    return "";
  }

  function renderSource(st) {
    for (const c of chars) c.className = c.classList.contains("eof") ? "ch eof" : c.classList.contains("nl") ? "ch nl" : "ch";
    const cur = toChar(st.cursor);
    for (let i = 0; i < cur && i < chars.length; i++) chars[i].classList.add("seen");
    for (const t of st.tokens) {
      const cls = TOK_CLASS[t.tok];
      if (!cls) continue;
      for (let i = toChar(t.start); i < toChar(t.end); i++) chars[i] && chars[i].classList.add(cls);
    }
    if (st.curTok && st.curTok.tok !== "EOF") {
      for (let i = toChar(st.curTok.start); i < toChar(st.curTok.end); i++) chars[i] && chars[i].classList.add("cur");
    }
    if (st.error) {
      const ch = chars[toChar(st.error.pos)];
      if (ch) ch.classList.add("errch");
    } else if (chars[cur]) {
      chars[cur].classList.add("caret");
    }
    const target = st.error ? chars[toChar(st.error.pos)] : chars[cur];
    if (target && target.scrollIntoView) target.scrollIntoView({ block: "nearest" });
    el.cursorInfo.textContent = `cursor ${lineColOf(cur)} (byte ${st.cursor})`;

    if (st.error) {
      const lines = trace.input.split("\n");
      const src = lines[st.error.line - 1] || "";
      const pad = String(st.error.line).length;
      const colChars = [...src].slice(0, st.error.col - 1).length;
      el.errorBox.innerHTML =
        `<b>${esc(trace.name || "input")}:${st.error.line}:${st.error.col}: error:</b>\n${esc(st.error.message)}` +
        `<span class="snip">${st.error.line} | ${esc(src)}\n${" ".repeat(pad)} | ${" ".repeat(colChars)}<b>^</b></span>`;
      el.errorBox.hidden = false;
    } else {
      el.errorBox.hidden = true;
    }
  }

  function renderStack(st) {
    el.stack.innerHTML = "";
    st.stack.forEach((f, i) => {
      const d = document.createElement("div");
      const top = i === st.stack.length - 1;
      d.className = `frame r-${f.rule}` + (top ? (st.error ? " failed" : " top") : "");
      d.innerHTML = `<span class="rule">${esc(f.rule)}</span><span class="fn">${esc(FN[f.rule] || "")}</span><span class="at">@${lineColOf(toChar(f.pos))}</span>`;
      if (i < prevStackLen && !top) d.style.animation = "none";
      el.stack.appendChild(d);
    });
    prevStackLen = st.stack.length;
    if (!st.stack.length) {
      el.stack.innerHTML = `<div class="tree-empty">${step === 0 ? "empty: parsing has not started" : st.error ? "" : "empty: parse() returned the root value"}</div>`;
    }
    if (st.error && st.stack.length) {
      const d = document.createElement("div");
      d.className = "tree-empty";
      d.style.color = "var(--error)";
      d.textContent = "JsonParseError is thrown from the frame below. The stack unwinds and unique_ptr frees every partial node.";
      el.stack.appendChild(d);
    }
    el.stack.scrollTop = -el.stack.scrollHeight;
    const from = Math.max(0, step - 14);
    el.log.innerHTML = trace.events.slice(from, step).reverse().map((e) => `<li>${logLine(e)}</li>`).join("");
  }
  let prevStackLen = 0;

  const NODE_W = 118, NODE_H = 40, COL_W = 178, ROW_H = 50, PAD = 6;

  function renderTree(st) {
    // layered left-to-right layout: leaves get consecutive rows, parents sit
    // at the middle of their children
    const pos = new Map();
    let row = 0;
    function place(id, depth) {
      const n = st.nodes.get(id);
      let y;
      if (!n.children.length) y = row++;
      else {
        const ys = n.children.map((c) => place(c, depth + 1));
        y = (ys[0] + ys[ys.length - 1]) / 2;
      }
      pos.set(id, { x: PAD + depth * COL_W, y: PAD + y * ROW_H });
      return y;
    }
    const roots = st.order.filter((id) => st.nodes.get(id).parent === null);
    roots.forEach((r) => place(r, 0));

    // remove nodes that no longer exist (stepping backwards)
    for (const [id, d] of nodeEls) {
      if (!st.nodes.has(id)) { d.remove(); nodeEls.delete(id); }
    }
    // error: innermost container still open is where it broke
    let broken = null;
    if (st.error) {
      for (let k = st.order.length - 1; k >= 0; k--) {
        const n = st.nodes.get(st.order[k]);
        if (n.open) { broken = n.id; break; }
      }
    }
    let maxX = 0, maxY = 0;
    for (const id of st.order) {
      const n = st.nodes.get(id);
      const p = pos.get(id);
      let d = nodeEls.get(id);
      if (!d) {
        d = document.createElement("div");
        nodeEls.set(id, d);
        el.tree.appendChild(d);
      }
      d.className = `tnode cls-${n.cls}` + (n.open ? " open" : "") + (st.fresh === id ? " fresh" : "") + (broken === id ? " broken" : "");
      const pv = n.cls === "JsonString" ? `"${n.preview}"` : n.open ? (n.cls === "JsonObject" ? "{ ...building" : "[ ...building") : childSummary(st, n);
      d.innerHTML = `<div class="cls">${n.cls} <span style="color:var(--muted);font-weight:400">#${id}</span></div><div class="pv">${esc(pv)}</div>`;
      d.style.left = p.x + "px";
      d.style.top = p.y + "px";
      maxX = Math.max(maxX, p.x + NODE_W);
      maxY = Math.max(maxY, p.y + NODE_H);
    }

    // edges with key/index labels
    let svg = "";
    for (const id of st.order) {
      const n = st.nodes.get(id);
      if (n.parent === null || !pos.has(n.parent)) continue;
      const a = pos.get(n.parent), b = pos.get(id);
      const x1 = a.x + NODE_W, y1 = a.y + NODE_H / 2, x2 = b.x, y2 = b.y + NODE_H / 2;
      const mx = (x1 + x2) / 2;
      svg += `<path d="M${x1},${y1} C${mx},${y1} ${mx},${y2} ${x2},${y2}"/>`;
      const label = n.key !== undefined ? `"${n.key}"` : `[${n.index}]`;
      svg += `<text x="${x2 - 4}" y="${y2 - 7}" text-anchor="end">${esc(label.length > 12 ? label.slice(0, 11) + "…" : label)}</text>`;
    }
    el.edges.innerHTML = svg;
    el.edges.setAttribute("width", maxX + 10);
    el.edges.setAttribute("height", maxY + 10);

    // scale down to fit the panel
    const W = el.treeWrap.clientWidth, H = el.treeWrap.clientHeight;
    const s = Math.min(1, W / (maxX + 10 || 1), H / (maxY + 10 || 1));
    el.tree.style.transform = `scale(${s})`;
    el.tree.style.width = maxX + 10 + "px";
    el.tree.style.height = maxY + 10 + "px";

    if (!st.order.length) {
      if (!el.tree.querySelector(".tree-empty")) {
        const d = document.createElement("div");
        d.className = "tree-empty";
        d.textContent = "no JsonValue created yet";
        el.tree.appendChild(d);
      }
    } else {
      el.tree.querySelectorAll(".tree-empty").forEach((x) => x.remove());
    }
  }

  function childSummary(st, n) {
    if (n.cls === "JsonObject") return n.children.length ? `{ ${n.children.length} member${n.children.length > 1 ? "s" : ""} }` : "{ }";
    if (n.cls === "JsonArray") return n.children.length ? `[ ${n.children.length} item${n.children.length > 1 ? "s" : ""} ]` : "[ ]";
    return n.preview;
  }

  function render() {
    const st = stateAt(step);
    el.scrub.value = step;
    el.stepLabel.textContent = `${step} / ${trace.events.length}`;
    el.eventBar.innerHTML = describe(st.last);
    renderSource(st);
    renderStack(st);
    renderTree(st);
  }

  // ---------- playback ----------
  function go(n) { step = Math.max(0, Math.min(trace.events.length, n)); render(); if (step >= trace.events.length) stop(); }
  function stop() { clearTimeout(timer); timer = null; el.play.innerHTML = "&#x25B6; Play"; }
  function tick() { go(step + 1); if (timer !== null) timer = setTimeout(tick, 1000 / +el.speed.value); }
  function play() {
    if (step >= trace.events.length) step = 0;
    el.play.innerHTML = "&#x275A;&#x275A; Pause";
    timer = setTimeout(tick, 0);
  }
  el.play.onclick = () => (timer === null ? play() : stop());
  $("btnPrev").onclick = () => { stop(); go(step - 1); };
  $("btnNext").onclick = () => { stop(); go(step + 1); };
  $("btnStart").onclick = () => { stop(); go(0); };
  $("btnEnd").onclick = () => { stop(); go(trace.events.length); };
  el.scrub.oninput = () => { stop(); go(+el.scrub.value); };
  document.addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT" && e.target.type !== "range") return;
    if (e.key === " ") { e.preventDefault(); el.play.click(); }
    else if (e.key === "ArrowRight") { e.preventDefault(); stop(); go(step + 1); }
    else if (e.key === "ArrowLeft") { e.preventDefault(); stop(); go(step - 1); }
    else if (e.key === "Home") { stop(); go(0); }
    else if (e.key === "End") { stop(); go(trace.events.length); }
  });
  window.addEventListener("resize", () => trace && render());

  $("loadTrace").onchange = async (ev) => {
    const f = ev.target.files[0];
    if (!f) return;
    try {
      const t = JSON.parse(await f.text());
      t.name = f.name.replace(/\.json$/, "");
      addSample(t);
      load(t);
    } catch (err) {
      alert("Not a jsonparser --trace file: " + err.message);
    }
  };

  function addSample(t) {
    const b = document.createElement("button");
    b.dataset.name = t.name;
    b.textContent = t.name.replace(/^\d+-/, "");
    b.className = t.ok ? "okk" : "err";
    b.title = t.ok ? "valid input" : `invalid: ${t.error.line}:${t.error.col} ${t.error.message}`;
    b.onclick = () => load(t);
    el.samples.appendChild(b);
  }

  // test/recording hook
  window.viz = { load: (name) => load(window.TRACES.find((t) => t.name === name)), go, play, stop, get step() { return step; }, get total() { return trace.events.length; } };

  const all = window.TRACES || [];
  all.forEach(addSample);
  if (all.length) load(all[0]);
})();
