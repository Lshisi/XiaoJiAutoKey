// Regeneration is optional; the checked-in JSON is consumed without Node.js.
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const modes = ['toggle', 'hold', 'independent', 'sequence', 'longPress'];
const cases = [];
const clone = x => JSON.parse(JSON.stringify(x));
const binding = (mode, key = 'K:65', id = 1, extra = {}) => ({ id, mode, key, delayMs: 90, ...extra });
const profile = (bindings, extra = {}) => ({ id: 10000, pauseKeys: ['K:80'], bindings, ...extra });
const op = (op, extra = {}) => ({ op, ...extra });
const input = (key, action = 'tap', target = 'repeat') => op(action, { key, target });
const wait = ms => op('wait', { ms });
const state = expect => op('state', { expect });
const quiet = key => ({ key, maxDown: 0, held: false });
const active = (mode, key) => mode === 'longPress' ? { key, held: true, maxDown: 1 } : { key, minDown: 2 };
const observe = (...outputs) => op('observe', { ms: 260, outputs });
const modifiers = key => [key.includes('+Ctrl') && 'K:162', key.includes('+Shift') && 'K:160', key.includes('+Alt') && 'K:164'].filter(Boolean);
const press = key => [...modifiers(key).map(k => input(k, 'down')), input(key, 'down')];
const release = key => [input(key.split('+')[0], 'up'), ...modifiers(key).reverse().map(k => input(k, 'up'))];
const select = (mode, key) => mode === 'hold' ? press(key) : mode === 'independent' ? [...press(key), ...release(key)] : [];
function add(id, group, description, p, steps, extra = {}) {
  cases.push({ id, group, description, modes: [...new Set(p.bindings.map(b => b.mode))],
    tags: [group, ...new Set(p.bindings.map(b => b.mode))], profile: p, steps, ...extra });
}

// Each basic mode gets the same lifecycle contract, across common input shapes.
const keys = [['keyboard', 'K:65'], ['mouse-left', 'M:1'], ['mouse-right', 'M:2'],
  ['mouse-x1', 'M:5'], ['mouse-x2', 'M:6'], ['ctrl-combo', 'K:65+Ctrl'],
  ['shift-combo', 'K:65+Shift'], ['alt-combo', 'K:65+Alt'], ['multi-combo', 'K:65+Ctrl+Shift'],
  ['left-ctrl', 'K:162'], ['right-ctrl', 'K:163'], ['left-shift', 'K:160'],
  ['right-shift', 'K:161'], ['left-alt', 'K:164'], ['right-alt', 'K:165']];
for (const mode of modes) for (const [shape, key] of keys) {
  const p = profile([binding(mode, key)]);
  const steps = [op('start'), ...select(mode, key), observe(active(mode, key)), input('K:80', 'down'),
    state({ paused: true }), observe(mode === 'hold' ? active(mode, key) : quiet(key)),
    input('K:80', 'up'), observe(active(mode, key)), op('suspend', { value: true }),
    observe(quiet(key)), op('suspend', { value: false }), observe(active(mode, key)),
    op('stop'), observe(quiet(key)), ...release(key), op('start')];
  if (mode === 'hold' || mode === 'independent') steps.push(observe(quiet(key)), ...select(mode, key));
  steps.push(observe(active(mode, key)), op('stop'), observe(quiet(key)));
  add(`S-${mode}-${shape}`, 'single', `${mode}: ${shape}, pause/suspend/stop/restart`, p, steps,
    { tags: ['single', mode, shape, ...(shape === 'keyboard' ? ['smoke'] : [])] });
}

// Additional selection semantics that a generic lifecycle cannot prove.
for (const prevent of [false, true]) {
  add(`S-independent-prevent-${prevent}`, 'single', 'Independent selection, repeated-down debounce, switch and prevent-stop',
    profile([binding('independent', 'K:65', 1, { preventAccidentalStop: prevent }), binding('independent', 'K:66', 2)]),
    [op('start'), input('K:65', 'down'), input('K:65', 'down'), state({ independent: 'K:65' }), input('K:65', 'up'),
      input('K:65'), state({ independent: prevent ? 'K:65' : null }), input('K:66'),
      state({ independent: 'K:66' }), observe(quiet('K:65'), active('independent', 'K:66'))]);
}
for (const remember of [false, true]) {
  add(`S-sequence-memory-${remember}`, 'single', 'Sequence skips disabled segments and honors restart memory',
    profile([binding('sequence', 'K:65'), binding('sequence', 'K:66', 2, { enabled: false }), binding('sequence', 'K:67', 3)],
      { rememberSequencePosition: remember }),
    [op('start'), state({ sequence: 1 }), input('K:82', 'down'), input('K:82', 'down'),
      state({ sequence: 3 }), input('K:82', 'up'), observe(quiet('K:65'), quiet('K:66'), active('sequence', 'K:67')),
      op('stop'), op('start'), state({ sequence: remember ? 3 : 1 })]);
}
for (const mode of modes) {
  add(`S-${mode}-disabled`, 'single', 'Disabled bindings never emit', profile([binding(mode, 'K:65', 1, { enabled: false })]),
    [op('start'), input('K:65'), input('K:82'), observe(quiet('K:65'))]);
}
for (const mode of ['toggle', 'hold', 'independent', 'sequence']) for (const delayMs of [1, 1000]) {
  add(`S-${mode}-interval-${delayMs}`, 'single', 'Minimum/slow interval still stops promptly without waiting for the next pulse',
    profile([binding(mode, 'K:65', 1, { delayMs })]),
    [op('start'), ...select(mode, 'K:65'), op('observe', { ms: delayMs === 1 ? 200 : 1200,
      outputs: [{ key: 'K:65', minDown: delayMs === 1 ? 10 : 1 }] }), op('stop'), observe(quiet('K:65'))]);
}
add('S-empty-profile', 'single', 'An empty profile starts/stops without phantom output', profile([]),
  [op('start'), input('K:65'), input('K:82'), observe(quiet('K:65')), op('stop'), state({ running: false })]);
for (const [code, mask] of [[162, 'Ctrl'], [163, 'Ctrl'], [160, 'Shift'], [161, 'Shift'], [164, 'Alt'], [165, 'Alt']]) {
  for (const primary of ['K:65', 'M:2']) for (const first of ['primary', 'modifier']) {
    const combo = `${primary}+${mask}`;
    add(`S-hold-release-${primary === 'K:65' ? 'keyboard' : 'mouse'}-${code}-${first}`, 'single',
      'Releasing either required combo component terminates hold and permits the next stroke',
      profile([binding('hold', combo)]),
      [op('start'), op('repeat', { count: 2, steps: [input(`K:${code}`, 'down'), input(combo, 'down'),
        observe(active('hold', primary)), input(first === 'primary' ? primary : `K:${code}`, 'up'),
        state({ holds: 0 }), observe(quiet(primary)), input(first === 'primary' ? `K:${code}` : primary, 'up')] })]);
  }
}
for (const mode of modes) {
  const slowed = ['toggle', 'independent', 'sequence'].includes(mode);
  const duringMovement = mode === 'longPress' ? { key: 'K:65', held: true }
    : slowed ? { key: 'K:65', minDown: 8, maxDown: 15 } : { key: 'K:65', minDown: 25 };
  const steps = [op('start'), input('K:87', 'down'), ...select(mode, 'K:65')];
  if (mode === 'independent') steps.push(state({ independent: 'K:65' }));
  steps.push(op('observe', { ms: 500, outputs: [duringMovement] }), input('K:87', 'up'),
    op('observe', { ms: 500, outputs: [mode === 'longPress' ? { key: 'K:65', held: true } : { key: 'K:65', minDown: 25 }] }),
    op('stop'), observe(quiet('K:65')));
  add(mode === 'independent' ? 'S-wasd-protection' : `S-wasd-protection-${mode}`, 'single',
    'WASD caps toggle-like frequency at a 35ms minimum interval; holds remain active and selection is retained',
    profile([binding(mode, 'K:65', 1, { delayMs: 1 })], { wasdProtection: true }), steps);
}

// All 15 unordered pairs, with ordinary concurrency, hold priority and excluded holds.
for (let i = 0; i < modes.length; i++) for (let j = i; j < modes.length; j++) {
  const a = modes[i], b = modes[j];
  for (const policy of ['parallel', 'priority', 'excluded']) {
    const p = profile([binding(a, 'K:65', 1, { suppressMixedMode: policy === 'excluded' }),
      binding(b, 'K:66', 2, { suppressMixedMode: policy === 'excluded' })], { mixedMode: policy !== 'parallel' });
    const selected = [!(a === 'independent' && b === 'independent'), !(a === 'sequence' && b === 'sequence')];
    const priority = policy === 'priority' && (a === 'hold' || b === 'hold');
    const expected = (m, k, index, hasPriority) => selected[index] && (!hasPriority || m === 'hold') ? active(m, k) : quiet(k);
    const steps = [op('start'), ...select(a, 'K:65'), ...select(b, 'K:66'),
      observe(expected(a, 'K:65', 0, priority), expected(b, 'K:66', 1, priority))];
    if (a === 'hold') steps.push(input('K:65', 'up'));
    if (b === 'hold') steps.push(input('K:66', 'up'));
    steps.push(observe(a === 'hold' ? quiet('K:65') : expected(a, 'K:65', 0, false),
      b === 'hold' ? quiet('K:66') : expected(b, 'K:66', 1, false)), op('stop'), observe(quiet('K:65'), quiet('K:66')));
    add(`P-${a}-${b}-${policy}`, 'composite', `${a} + ${b}: ${policy}`, p, steps);
  }
}

// Shared components: stopping one requester must not release another requester's prefix.
for (const a of ['toggle', 'longPress']) for (const b of ['toggle', 'longPress']) {
  const p = profile([binding(a, 'K:65+Ctrl'), binding(b, 'K:66+Ctrl', 2)]);
  const remaining = profile([binding(a, 'K:65+Ctrl', 1, { enabled: false }), binding(b, 'K:66+Ctrl', 2)]);
  add(`P-shared-prefix-${a}-${b}`, 'composite', 'Shared Ctrl stays held until the final requester ends', p,
    [op('start'), observe(active(a, 'K:65'), active(b, 'K:66'), { key: 'K:162', held: true }),
      op('bindings', { profile: remaining }), observe(quiet('K:65'), active(b, 'K:66'), { key: 'K:162', held: true, maxUp: 0 }),
      op('stop'), observe(quiet('K:162'), quiet('K:66'))]);
}
add('P-held-modifier-with-pulse', 'composite', 'Standalone held Ctrl supplies a stable layer to pulsed A',
  profile([binding('longPress', 'K:162'), binding('toggle', 'K:65', 2)]),
  [op('start'), observe({ key: 'K:162', held: true, maxUp: 0 }, active('toggle', 'K:65'))]);

// Same/different keys across the repeater and macro, with both start orders and trigger modes.
for (const mode of modes) for (const triggerMode of ['toggle', 'hold']) for (const shared of [false, true]) for (const first of ['repeat', 'macro']) {
  const key = shared ? 'K:65' : 'K:67';
  const m = { id: 1, triggerKey: 'K:120', triggerMode, steps: [
    { action: 'click', key: 'K:65', delayAfterMs: 500 }, { action: 'click', key: 'K:66', delayAfterMs: 100 }] };
  const startMacro = input('K:120', triggerMode === 'hold' ? 'down' : 'tap', 'macro');
  const startRepeat = [op('start'), ...select(mode, key)];
  const steps = first === 'repeat' ? [...startRepeat, wait(30), startMacro] : [startMacro, wait(30), ...startRepeat];
  const source = who => op('source', { key: 'K:65', source: who, heartbeat: 'K:66' });
  if (shared) {
    steps.push(source(first), input('K:80', 'down'), source(first === 'macro' ? 'macro' : mode === 'hold' ? 'repeat' : 'none'),
      input('K:80', 'up'), source(first));
  } else steps.push(op('observe', { ms: 800, outputs: [active(mode, key), { key: 'K:65', minDown: 1 }, { key: 'K:66', minDown: 1 }] }));
  steps.push(op('stop'));
  if (shared) steps.push(source('macro'));
  else steps.push(op('observe', { ms: 800, outputs: [quiet(key), { key: 'K:65', minDown: 1 }, { key: 'K:66', minDown: 1 }] }));
  steps.push(op('macroStop'), observe(quiet('K:65'), quiet('K:66'), quiet(key)));
  add(`X-${mode}-${triggerMode}-${shared ? 'same' : 'different'}-${first}-first`, 'composite',
    `Repeater ${mode} + macro ${triggerMode}, ${shared ? 'shared' : 'different'} primary, ${first} starts first`,
    profile([binding(mode, key)]), steps, { modes: [mode, `macro:${triggerMode}`], macros: [m], tags: ['composite', 'arbitration', mode, `macro:${triggerMode}`] });
}

// Every macro action, finite loops, disabled steps and both trigger modes.
const actionSequences = {
  click: [{ action: 'click', key: 'K:65' }],
  hold: [{ action: 'hold', key: 'K:65', durationMs: 120 }],
  'down-up': [{ action: 'down', key: 'K:65' }, { action: 'wait', durationMs: 100 }, { action: 'up', key: 'K:65' }],
  wait: [{ action: 'wait', durationMs: 100 }, { action: 'click', key: 'K:65' }],
  wheel: [{ action: 'wheel', wheelDelta: 120 }, { action: 'wheel', wheelDelta: -120 }],
  disabled: [{ action: 'click', key: 'K:66', enabled: false }, { action: 'click', key: 'K:65' }],
};
for (const disabled of ['macro', 'all-steps']) {
  add(`M-disabled-${disabled}`, 'single', 'Disabled macros or all-disabled steps cannot start or emit', profile([]),
    [input('K:120', 'tap', 'macro'), state({ macro: { id: 1, running: false, error: false } }), observe(quiet('K:65'))],
    { modes: ['macro:toggle'], tags: ['single', 'macro:toggle', 'disabled'],
      macros: [{ id: 1, enabled: disabled !== 'macro', steps: [{ action: 'click', key: 'K:65', enabled: disabled !== 'all-steps' }] }] });
}
for (const [action, steps] of Object.entries(actionSequences)) for (const loops of [1, 3]) {
  const counts = action === 'wheel' ? [{ wheel: 120, min: loops, max: loops }, { wheel: -120, min: loops, max: loops }]
    : [{ key: 'K:65', minDown: loops, maxDown: loops, minUp: loops, maxUp: loops }, quiet('K:66')];
  add(`M-action-${action}-loops-${loops}`, 'single', `Macro ${action}: exactly ${loops} finite loops`, profile([]),
    [op('mark'), input('K:120', 'tap', 'macro'), state({ macro: { id: 1, running: true } }),
      op('state', { withinMs: 6000, expect: { macro: { id: 1, running: false, error: false } } }), op('counts', { outputs: counts })],
    { modes: ['macro:toggle', `action:${action}`], tags: ['single', 'macro:toggle', `action:${action}`, ...(action === 'click' && loops === 1 ? ['smoke'] : [])],
      macros: [{ id: 1, loopCount: loops, triggerMode: 'toggle', steps }] });
}
for (const triggerMode of ['toggle', 'hold']) for (const interruption of ['pause-resume', 'pause-stop', 'suspend-resume', 'suspend-stop']) {
  for (const action of ['click', 'hold', 'down-up', 'wait', 'wheel']) {
    const steps = clone(actionSequences[action]);
    if (action === 'hold') steps[0].durationMs = 800;
    else if (action === 'wait') steps[0].durationMs = 800;
    else if (action === 'down-up') steps[1].durationMs = 800;
    else steps[0].delayAfterMs = 800;
    const actions = [input('K:120', triggerMode === 'hold' ? 'down' : 'tap', 'macro'), wait(100),
      state({ macro: { id: 1, running: true } })];
    const pause = interruption.startsWith('pause');
    actions.push(pause ? input('K:80', 'down', 'macro') : op('suspend', { value: true, target: 'macro' }),
      state({ macro: { id: 1, paused: true } }), observe(quiet('K:65')));
    if (interruption.endsWith('stop')) actions.push(op('macroStop'));
    actions.push(pause ? input('K:80', 'up', 'macro') : op('suspend', { value: false, target: 'macro' }));
    if (interruption.endsWith('resume')) {
      actions.push(state({ macro: { id: 1, paused: false, running: true } }), wait(100));
      if (action === 'hold' || action === 'down-up') actions.push(observe({ key: 'K:65', held: true }));
    } else actions.push(state({ macro: { id: 1, running: false } }));
    actions.push(op('macroStop'), observe(quiet('K:65')));
    if (action === 'click') actions.push(op('counts', { from: 'start', outputs: [{ key: 'K:65', minDown: 1, maxDown: 1 }] }));
    if (action === 'wheel') actions.push(op('counts', { from: 'start', outputs: [{ wheel: 120, min: 1, max: 1 }] }));
    add(`M-interrupt-${triggerMode}-${action}-${interruption}`, 'complex', `Macro ${triggerMode}/${action}: ${interruption}`,
      profile([]), actions, { modes: [`macro:${triggerMode}`, `action:${action}`], tags: ['complex', `macro:${triggerMode}`, 'interruption'],
        macros: [{ id: 1, triggerMode, pauseKeys: ['K:80'], steps }] });
  }
}
for (const [name, key, extra] of [['mouse', 'M:2', 'K:160'], ['ctrl-keyboard', 'K:120+Ctrl', 'K:162']]) {
  const begin = key.includes('+Ctrl') ? [input('K:162', 'down', 'macro'), input(key, 'down', 'macro')] : [input(key, 'down', 'macro')];
  add(`M-hold-trigger-${name}`, 'single', 'Hold-trigger macro releases with changed modifier snapshots and can restart', profile([]),
    [...begin, observe({ key: 'K:65', minDown: 2 }), input(extra, 'down', 'macro'), input(key.split('+')[0] + (name === 'mouse' ? '+Shift' : '+Ctrl'), 'up', 'macro'),
      state({ macro: { id: 1, running: false } }), observe(quiet('K:65')), input(extra, 'up', 'macro'),
      ...begin, observe({ key: 'K:65', minDown: 2 }), op('macroStop'), observe(quiet('K:65'))],
    { modes: ['macro:hold'], tags: ['single', 'macro:hold', 'trigger-release'],
      macros: [{ id: 1, triggerMode: 'hold', triggerKey: key, steps: [{ action: 'click', key: 'K:65', delayAfterMs: 90 }] }] });
}

// Overlapping pause reasons must not resume prematurely; stop and profile replacement are terminal.
for (const mode of ['toggle', 'independent', 'sequence', 'longPress']) {
  const p = profile([binding(mode), binding('hold', 'K:66', 2)], { mixedMode: true, pauseKeys: ['K:80', 'K:79'] });
  add(`C-${mode}-overlapping-gates`, 'complex', 'Pause keys, hold priority and suspension overlap without premature resume', p,
    [op('start'), ...select(mode, 'K:65'), observe(active(mode, 'K:65')),
      input('K:80', 'down'), input('K:79', 'down'), input('K:66', 'down'), op('suspend', { value: true }),
      input('K:80', 'up'), op('suspend', { value: false }), observe(quiet('K:65'), active('hold', 'K:66')),
      input('K:79', 'up'), observe(quiet('K:65')), input('K:66', 'up'), observe(active(mode, 'K:65')),
      input('K:80', 'down'), op('stop'), input('K:80', 'up'), observe(quiet('K:65'), quiet('K:66'))]);
  add(`C-${mode}-replace-while-paused`, 'complex', 'Replacing bindings while paused cannot restore the old output', p,
    [op('start'), ...select(mode, 'K:65'), observe(active(mode, 'K:65')), input('K:80', 'down'),
      op('bindings', { profile: profile([binding('toggle', 'K:67')]) }), input('K:80', 'up'),
      observe(quiet('K:65'), active('toggle', 'K:67')), op('stop'), observe(quiet('K:67'))]);
}
add('C-all-modes-and-macro', 'complex', 'All five modes plus a macro: concurrency, preemption, pause and final release',
  profile([binding('toggle'), binding('hold', 'K:66', 2), binding('independent', 'K:67', 3),
    binding('sequence', 'K:68', 4), binding('sequence', 'K:69', 5), binding('longPress', 'K:70', 6)], { mixedMode: true }),
  [op('start'), input('K:67'), input('K:120', 'tap', 'macro'),
    observe(active('toggle', 'K:65'), active('independent', 'K:67'), active('sequence', 'K:68'), active('longPress', 'K:70')),
    input('K:66', 'down'), observe(quiet('K:65'), quiet('K:67'), quiet('K:68'), quiet('K:70'), active('hold', 'K:66')),
    input('K:82'), input('K:66', 'up'), observe(quiet('K:68'), active('sequence', 'K:69'), active('longPress', 'K:70')),
    op('suspend', { value: true }), observe(quiet('K:65'), quiet('K:71'), quiet('K:70')),
    op('stop'), op('macroStop'), op('suspend', { value: false }), observe(quiet('K:65'), quiet('K:71'), quiet('K:70'))],
  { modes: [...modes, 'macro:toggle'], tags: ['complex', 'all-modes'],
    macros: [{ id: 1, steps: [{ action: 'click', key: 'K:71' }] }] });

// Explicit conflict oracles are independent of the production validation functions.
for (let i = 0; i < modes.length; i++) for (let j = i; j < modes.length; j++) for (const mixed of [false, true]) {
  const a = modes[i], b = modes[j];
  const together = !(a === b && ['independent', 'sequence'].includes(a)) &&
    !(mixed && ((a === 'hold' && b !== 'hold') || (b === 'hold' && a !== 'hold')));
  let conflict = 'none';
  if (a === b) conflict = 'duplicate';
  else if (['hold', 'independent'].includes(a) && ['hold', 'independent'].includes(b)) conflict = 'sharedTrigger';
  else if ((a === 'longPress' && ['hold', 'independent'].includes(b)) || (b === 'longPress' && ['hold', 'independent'].includes(a))) conflict = 'heldTrigger';
  else if (together) conflict = 'sharedPrimary';
  add(`V-same-primary-${a}-${b}-${mixed}`, 'validation', 'Same-primary conflict matrix, including symmetric ordering', profile([]),
    [op('validatePair', { a: binding(a), b: binding(b), mixed, conflict, together })], { modes: [a, b], tags: ['validation', 'conflict'] });
}
const conflictCases = [
  ['shared-prefix', binding('toggle', 'K:65+Ctrl'), binding('toggle', 'K:66+Ctrl'), false, 'none'],
  ['different-prefix', binding('toggle', 'K:65+Ctrl'), binding('toggle', 'K:66+Alt'), false, 'modifierState'],
  ['held-layer', binding('longPress', 'K:162'), binding('toggle', 'K:65+Ctrl'), false, 'none'],
  ['wrong-held-layer', binding('longPress', 'K:164'), binding('toggle', 'K:65+Ctrl'), false, 'modifierState'],
  ['pulsed-prefix', binding('toggle', 'K:162'), binding('toggle', 'K:65+Ctrl'), false, 'pulsedModifier'],
  ['modifier-trigger', binding('independent', 'K:162'), binding('hold', 'K:65+Ctrl'), false, 'modifierTrigger'],
  ['held-trigger', binding('longPress'), binding('hold'), true, 'heldTrigger'],
  ['disabled', binding('toggle', 'K:65', 1, { enabled: false }), binding('longPress'), false, 'none'],
  ['disabled-duplicate', binding('toggle', 'K:65', 1, { enabled: false }), binding('toggle'), false, 'duplicate'],
  ['assist-shared-prefix', binding('longPress', 'K:162', 1, { martialAssist: true }), binding('toggle', 'K:65+Ctrl'), true, 'pulsedModifier'],
];
for (const [id, a, b, mixed, conflict] of conflictCases)
  add(`V-${id}`, 'validation', `Conflict rule: ${id}`, profile([]), [op('validatePair', { a, b, mixed, conflict })],
    { modes: [a.mode, b.mode], tags: ['validation', 'conflict'] });
const invalidMacros = [
  ['unreleased', [{ action: 'down', key: 'K:65' }]],
  ['orphan-up', [{ action: 'up', key: 'K:65' }]],
  ['duplicate-down', [{ action: 'down', key: 'K:65' }, { action: 'down', key: 'K:65' }, { action: 'up', key: 'K:65' }]],
  ['click-held', [{ action: 'down', key: 'K:65' }, { action: 'click', key: 'K:65' }, { action: 'up', key: 'K:65' }]],
  ['zero-duration', [{ action: 'hold', key: 'K:65', durationMs: 0 }]],
  ['negative-delay', [{ action: 'click', key: 'K:65', delayAfterMs: -1 }]],
  ['bad-wheel', [{ action: 'wheel', wheelDelta: 121 }]],
  ['zero-wheel', [{ action: 'wheel', wheelDelta: 0 }]],
  ['too-large-wheel', [{ action: 'wheel', wheelDelta: 12120 }]],
  ['missing-key', [{ action: 'click' }]],
  ['reserved', [{ action: 'click', key: 'K:9+Alt' }]],
  ['too-many-steps', Array.from({ length: 513 }, () => ({ action: 'wait', durationMs: 1 }))],
];
for (const [id, steps] of invalidMacros)
  add(`V-macro-${id}`, 'validation', `Macro rejects ${id} before any output`, profile([]),
    [op('validateMacro', { macro: { steps }, valid: false }), input('K:120', 'tap', 'macro'),
      state({ macro: { id: 1, running: false, error: true } }), observe(quiet('K:65'))],
    { modes: ['macro:toggle'], tags: ['validation', 'macro:toggle'], macros: [{ id: 1, steps }] });

// Engine-only martial-assist timing. No game behavior is inferred from this.
const assist = profile([binding('longPress', 'K:70', 1, { martialAssist: true }), binding('hold', 'K:71', 2)], { mixedMode: true });
add('A-cancel-before-threshold', 'complex', 'Assist cancels an incomplete hold and restarts its 3.1s timer', assist,
  [op('start'), wait(2100), observe({ key: 'K:70', held: true }), input('K:80', 'down'), observe(quiet('K:70')),
    input('K:80', 'up'), wait(1000), observe({ key: 'K:70', held: true }), wait(2200), observe(quiet('K:70')),
    op('mark'), op('stop'), wait(300), op('counts', { outputs: [{ key: 'K:70', minDown: 1, maxDown: 1, minUp: 1, maxUp: 1 }] }),
    observe(quiet('K:70'))],
  { requiresStableForeground: true, modes: ['longPress', 'martialAssist'], tags: ['complex', 'martialAssist', 'engine-only'] });
add('A-complete-and-close', 'complex', 'Assist releases after 3.1s and emits a single close tap on pause', assist,
  [op('start'), wait(3500), observe(quiet('K:70')), op('mark'), input('K:80', 'down'), wait(300),
    op('counts', { outputs: [{ key: 'K:70', minDown: 1, maxDown: 1, minUp: 1, maxUp: 1 }] }),
    op('stop'), input('K:80', 'up'), observe(quiet('K:70'))],
  { requiresStableForeground: true, modes: ['longPress', 'martialAssist'], tags: ['complex', 'martialAssist', 'engine-only'] });
add('A-manual-takeover', 'complex', 'Manual takeover prevents later automatic close or resume', assist,
  [op('start'), wait(3500), input('K:70'), input('K:80', 'down'), input('K:80', 'up'),
    observe(quiet('K:70')), op('stop'), observe(quiet('K:70'))],
  { requiresStableForeground: true, modes: ['longPress', 'martialAssist'], tags: ['complex', 'martialAssist', 'engine-only'] });

add('R-all-mode-state-walk', 'stress', 'Seeded model-based state walk; generated actions retained in the report',
  profile([binding('toggle'), binding('hold', 'K:66', 2), binding('independent', 'K:67', 3),
    binding('sequence', 'K:68', 4), binding('sequence', 'K:69', 5), binding('longPress', 'K:70', 6)], { mixedMode: true }),
  [op('start'), op('randomWalk', { count: 150 }), op('stop'), observe(quiet('K:65'), quiet('K:66'), quiet('K:70'))],
  { timeoutMs: 120000, tags: ['stress', 'all-modes', 'seeded'] });

const document = { schemaVersion: 1, description: 'Data-driven engine contracts. Regenerate with generate-catalog.mjs.', cases };
writeFileSync(fileURLToPath(new URL('./catalog.json', import.meta.url)), JSON.stringify(document, null, 2) + '\n');
console.log(`Generated ${cases.length} scenarios.`);
