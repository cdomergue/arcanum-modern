/* Arcanum v119 object streams and TIG archives. No offsets guessed by scanning.
 * Format references: first_party/tig/src/file.c, src/game/{obj,sa,obj_private}.c
 * in the pinned Arcanum CE source. Only changed values/arrays are serialized.
 */
(function () {
  'use strict';
  const schema = globalThis.ArcanumSchema;
  const decoder = new TextDecoder('windows-1252');
  // Normalize runtimes that decode Windows-1252 punctuation as C1 controls.
  const c1 = '\u20ac\u0081\u201a\u0192\u201e\u2026\u2020\u2021\u02c6\u2030\u0160\u2039\u0152\u008d\u017d\u008f\u0090\u2018\u2019\u201c\u201d\u2022\u2013\u2014\u02dc\u2122\u0161\u203a\u0153\u009d\u017e\u0178';
  function decodeGameText(data) {
    return decoder.decode(data).replace(/[\x80-\x9f]/g, ch => c1[ch.charCodeAt(0) - 128]);
  }
  const typeNames = ['WALL', 'PORTAL', 'CONTAINER', 'SCENERY', 'PROJECTILE',
    'WEAPON', 'AMMO', 'ARMOR', 'GOLD', 'FOOD', 'SCROLL', 'KEY', 'KEY_RING',
    'WRITTEN', 'GENERIC', 'PC', 'NPC', 'TRAP'];
  function check(ok, message) { if (!ok) throw new Error(message); }
  function bytes(value) { return value instanceof Uint8Array ? new Uint8Array(value.buffer, value.byteOffset, value.byteLength) : new Uint8Array(value); }
  class Reader {
    constructor(data, start = 0, end = data.length) {
      this.data = data; this.pos = start; this.end = end;
      this.view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    }
    take(n) {
      check(Number.isSafeInteger(n) && n >= 0 && n <= this.end - this.pos, 'Truncated file or invalid size.');
      const start = this.pos; this.pos += n; return this.data.subarray(start, this.pos);
    }
    u8() { return this.take(1)[0]; }
    u16() { const p = this.pos; this.take(2); return this.view.getUint16(p, true); }
    u32() { const p = this.pos; this.take(4); return this.view.getUint32(p, true); }
    i32() { const p = this.pos; this.take(4); return this.view.getInt32(p, true); }
  }
  function parseArchive(index, data) {
    index = bytes(index); data = bytes(data);
    const r = new Reader(index), stack = [], entries = [], paths = new Set();
    let offset = 0, ended = false;
    while (r.pos < r.end) {
      const type = r.u32();
      if (type === 3) { check(!stack.length, 'Unclosed folders in the index.'); ended = true; break; }
      if (type === 2) { check(stack.length, 'Invalid folder index.'); stack.pop(); continue; }
      check(type === 0 || type === 1, 'Unknown TFAI entry type.');
      const length = r.u32();
      check(length > 0 && length < 260, 'Invalid entry name.');
      const name = decodeGameText(r.take(length));
      check(!/[\\/\x00-\x1f:]/.test(name) && name !== '.' && name !== '..', 'Invalid entry path.');
      if (type === 1) { check(stack.length < 32, 'Too many nested folders.'); stack.push(name); continue; }
      const sizeOffset = r.pos, size = r.u32(), path = [...stack, name].join('/');
      check(!paths.has(path.toLowerCase()), 'Duplicate archive entry.');
      check(size <= data.length - offset, 'The TFAI and TFAF files do not match or are truncated.');
      paths.add(path.toLowerCase()); entries.push({path, offset, size, sizeOffset}); offset += size;
      check(entries.length <= 100000, 'Archive is too complex.');
    }
    check(ended && r.pos === r.end && offset === data.length, 'Incomplete index or unrecognized trailing data.');
    return {index, data, entries};
  }
  function objectLayout(type) {
    check(typeNames[type], 'Unsupported object type.');
    const groups = ['BASE'];
    if (type >= 5 && type <= 14) groups.push('ITEM');
    if (type === 15 || type === 16) groups.push('CRITTER');
    groups.push(typeNames[type]);
    const fields = []; let words = 0;
    for (const group of groups) {
      schema.groups[group].forEach(([name, kind], i) => fields.push({name, kind, word: words + (i >> 5), bit: i % 32}));
      words += Math.ceil(schema.groups[group].length / 32);
    }
    return {fields, words};
  }
  function idKey(id) {
    // ObjectID padding is not part of identity.
    return [...id.subarray(0, 2), ...id.subarray(8)].map(b => b.toString(16).padStart(2, '0')).join('');
  }
  const arraySizes = {INT32_ARRAY: 4, UINT32_ARRAY: 4, INT64_ARRAY: 8, UINT64_ARRAY: 8,
    HANDLE_ARRAY: 24, SCRIPT_ARRAY: 12, QUEST_ARRAY: 16};
  function readField(r, kind) {
    if (kind === 'INT32') { const offset = r.pos; return {offset, value: r.i32(), kind}; }
    const present = r.u8();
    check(present === 0 || present === 1, 'Invalid field presence marker.');
    if (!present) return {kind, absent: true};
    if (kind === 'INT64') return {kind, raw: r.take(8)};
    if (kind === 'HANDLE') return {kind, id: idKey(r.take(24))};
    if (kind === 'STRING') {
      const length = r.u32(), raw = r.take(length + 1);
      check(raw[length] === 0, 'Unterminated string.');
      return {kind, text: decodeGameText(raw.subarray(0, length))};
    }
    check(Object.hasOwn(arraySizes, kind), 'Unknown field format.');
    const size = r.u32(), count = r.u32(), bitsetId = r.u32();
    check(size === arraySizes[kind], `Unsupported ${kind} array element size (${size}).`);
    const offset = r.pos; r.take(size * count);
    const words = r.u32();
    check(words <= 100000, 'Array is too large.');
    const mask = r.take(words * 4), maskView = new DataView(mask.buffer, mask.byteOffset, mask.byteLength);
    const elements = new Map(); let rank = 0;
    for (let w = 0; w < words; w++) {
      const word = maskView.getUint32(w * 4, true);
      for (let bit = 0; bit < 32; bit++) if (word & (1 << bit)) {
        check(rank < count, 'Array and bitmask do not match.');
        const p = offset + rank++ * size;
        elements.set(w * 32 + bit, {offset: p, value: size === 4 ? r.view.getInt32(p, true) : null, kind: size === 4 ? 'INT32' : kind});
      }
    }
    check(rank === count, 'Element count does not match.');
    return {kind, elements, bitsetId, words};
  }
  function readObject(r, entry) {
    const offset = r.pos;
    check(r.u32() === 119, 'Object version is not 119.');
    const proto = r.take(24);
    check(new DataView(proto.buffer, proto.byteOffset, 24).getInt16(0, true) === 1, 'Unrecognized object prototype.');
    const oid = r.take(24), oidType = new DataView(oid.buffer, oid.byteOffset, 24).getInt16(0, true);
    check([0, 2, 3].includes(oidType), 'Invalid object identifier.');
    const type = r.u32(), count = r.u16(), layout = objectLayout(type);
    const masks = Array.from({length: layout.words}, () => r.u32());
    const allowed = Array(layout.words).fill(0);
    for (const f of layout.fields) allowed[f.word] |= 1 << f.bit;
    check(masks.every((m, i) => (m & ~allowed[i]) === 0), 'Unrecognized field bitmask.');
    const fields = new Map();
    for (const f of layout.fields) if (masks[f.word] & (1 << f.bit)) {
      const start = r.pos, field = readField(r, f.kind);
      fields.set(f.name, {...field, start, end: r.pos});
    }
    check(fields.size === count, 'Field count does not match.');
    return {id: idKey(oid), type, offset, end: r.pos, entry, fields, layout};
  }
  const editableStats = [0, 1, 2, 3, 4, 5, 6, 7, 18, 19, 20, 21];
  function statRange(index) {
    if (index < 8) return [1, 40];
    if (index === 18) return [0, 2000000000];
    if (index === 19) return [-1000, 1000];
    if (index === 20) return [0, 100];
    return [0, 1000];
  }
  function openSave(index, data) {
    // Own the input buffers; caller and file bytes remain immutable.
    const archive = parseArchive(bytes(index).slice(), bytes(data).slice()), objects = [];
    for (const entry of archive.entries) {
      if (!/\.(mdy|mob)$/i.test(entry.path)) continue;
      const r = new Reader(archive.data, entry.offset, entry.offset + entry.size);
      try { while (r.pos < r.end) objects.push(readObject(r, entry)); }
      catch (error) { throw new Error(`${entry.path} : ${error.message}`); }
    }
    const players = objects.filter(o => o.type === 15);
    check(players.length, 'No player character found in this save’s mobile objects.');
    const fields = [];
    for (const [playerIndex, player] of players.entries()) {
      player.name = player.fields.get('PC_PLAYER_NAME')?.text || `Character ${playerIndex + 1}`;
      player.level = player.fields.get('CRITTER_STAT_BASE_IDX')?.elements?.get(17)?.value;
      const add = (field, key, group, range, stat = null) => {
        if (!field || field.kind !== 'INT32' || !Number.isInteger(field.value)) return;
        fields.push({key: `${playerIndex}:${key}`, label: key, group, stat, playerIndex,
          offset: field.offset, original: field.value, min: range[0], max: range[1]});
      };
      for (const stat of editableStats) add(player.fields.get('CRITTER_STAT_BASE_IDX')?.elements?.get(stat),
        schema.stats[stat], stat < 8 ? 'attributes' : 'progress', statRange(stat), stat);
      const addArray = (arrayName, element, key, group, max, extra = {}) => {
        const raw = player.fields.get(arrayName)?.elements?.get(element)?.value ?? 0;
        const mask = extra.mask ?? 0xffffffff, shift = extra.shift ?? 0;
        fields.push({key: `${playerIndex}:${key}`, label: key, group, stat: null, playerIndex,
          arrayName, element, mask, shift, original: (raw >>> shift) & mask, min: 0, max, ...extra});
      };
      schema.skills.forEach((name, skill) => {
        const array = skill < 12 ? 'CRITTER_BASIC_SKILL_IDX' : 'CRITTER_TECH_SKILL_IDX';
        const element = skill < 12 ? skill : skill - 12;
        addArray(array, element, name, 'skills', 5, {skill, mask: 63, aptitude: skill >= 12 ? 'tech' : null});
        addArray(array, element, `${name}_TRAINING`, 'skills', 3, {skill, training: true, mask: 3, shift: 6});
      });
      schema.colleges.forEach((name, college) => addArray('CRITTER_SPELL_TECH_IDX', college,
        name, 'magic', 5, {college, aptitude: 'magic'}));
      add(player.fields.get('PC_BANK_MONEY'), 'PC_BANK_MONEY', 'resources', [0, 2000000000]);
      for (const suffix of ['GOLD', 'ARROWS', 'BULLETS', 'POWER_CELLS', 'FUEL']) {
        const ref = player.fields.get(`CRITTER_${suffix}`)?.id;
        // Resolve within the same mobile stream; never modify an unrelated stack.
        const matches = objects.filter(o => ref && o.id === ref && o.entry === player.entry);
        if (matches.length !== 1) continue;
        const target = matches[0], expectedType = suffix === 'GOLD' ? 8 : 6;
        if (target.type !== expectedType) continue;
        add(target.fields.get(suffix === 'GOLD' ? 'GOLD_QUANTITY' : 'AMMO_QUANTITY'), suffix, 'resources', [0, 2000000000]);
      }
    }
    check(fields.length, 'No editable fields found.');
    return {archive, objects, players, fields};
  }
  const races = ['Human', 'Dwarf', 'Elf', 'Half-elf', 'Gnome', 'Halfling', 'Half-orc', 'Half-ogre', 'Dark elf', 'Ogre', 'Orc'];
  const genders = ['Female', 'Male'];
  const bodyTypes = [0, 1, 4, 0, 2, 2, 0, 3, 4, 3, 0];
  const femaleRaces = new Set([0, 2, 3, 6, 8]);
  const characters = new Map(Array.from({length:256}, (_, i) => [decodeGameText(Uint8Array.of(i)), i]));
  function encodeName(name) {
    check(typeof name === 'string' && name.trim() && name.length <= 23 && !/[\x00-\x1f\x7f-\x9f]/.test(name),
      'The name must contain 1 to 23 printable characters.');
    return Uint8Array.from([...name], ch => {
      check(characters.has(ch), 'This character cannot be saved in an Arcanum name.');
      return characters.get(ch);
    });
  }
  function numberBytes(value) {
    const b = new Uint8Array(4); new DataView(b.buffer).setUint32(0, value >>> 0, true); return b;
  }
  function savedIdentity(player) {
    const stats = player.fields.get('CRITTER_STAT_BASE_IDX')?.elements;
    return {
      name: player.fields.get('PC_PLAYER_NAME')?.text ?? '',
      gender: stats?.get(26)?.value,
      race: stats?.get(27)?.value
    };
  }
  function identityFields(save) {
    const labels = {name: 'Name', gender: 'Gender', race: 'Race'};
    const choices = {gender: genders, race: races};
    return save.players.flatMap((player, playerIndex) =>
      Object.entries(savedIdentity(player)).map(([id, original]) => ({
        key: playerIndex + ':identity:' + id, playerIndex, id, group: 'identity', original,
        label: labels[id], choices: choices[id] ?? null
      })));
  }
  function identityOverrides(player, edits, raceMod) {
    const fields = new Map(), original = savedIdentity(player);
    if (edits.has('name')) {
      const name = encodeName(edits.get('name'));
      fields.set('PC_PLAYER_NAME', concat([Uint8Array.of(1), numberBytes(name.length), name, Uint8Array.of(0)]));
    }
    if (!edits.has('race') && !edits.has('gender')) return fields;
    const gender = edits.has('gender') ? edits.get('gender') : original.gender;
    const race = edits.has('race') ? edits.get('race') : original.race;
    check(Number.isInteger(gender) && genders[gender] && Number.isInteger(race) && races[race], 'Select a valid gender and race.');
    check(race < 8 || raceMod, 'This race requires the UAP Race Mod.');
    check(gender === 1 || femaleRaces.has(race), 'This race and gender combination has no playable model.');

    // Match stat_base_set: racial effects have cause 0, the female effect has cause 9.
    const effects = player.fields.get('CRITTER_EFFECTS_IDX')?.elements ?? new Map();
    const causes = player.fields.get('CRITTER_EFFECT_CAUSE_IDX')?.elements ?? new Map();
    check(effects.size === causes.size && [...effects.keys()].every(i => causes.has(i)), 'Incompatible effect arrays.');
    const pairs = [];
    for (const [i, effect] of effects) {
      const cause = causes.get(i).value;
      if (edits.has('race') && cause === 0 || edits.has('gender') && cause === 9) continue;
      pairs.push([effect.value, cause]);
    }
    if (edits.has('race')) pairs.push([64 + race, 0]);
    if (edits.has('gender') && gender === 0) pairs.push([330, 9]);
    for (const [column, field] of ['CRITTER_EFFECTS_IDX', 'CRITTER_EFFECT_CAUSE_IDX'].entries()) {
      const values = new Map(pairs.map((pair, i) => [i, pair[column]]));
      fields.set(field, serializeArray({field: player.fields.get(field), values}));
    }

    const body = race === 3 && raceMod ? 4 : bodyTypes[race];
    for (const field of ['AID', 'CURRENT_AID']) {
      let art = player.fields.get(field)?.value ?? player.fields.get('AID')?.value ?? 0x28002000;
      if (race === 10) art = 0xc1002000;
      else {
        if ((art >>> 28) !== 2) art = 0x28002000;
        art = (art & ~0x0f000000) | (body << 24) | (gender << 27);
      }
      fields.set(field, numberBytes(art));
    }
    fields.set('SOUND_EFFECT', numberBytes(10 * (gender + 2 * race + 1)));
    return fields;
  }
  function identityError(save, changes, playerIndex, raceMod) {
    const edits = new Map(identityFields(save)
      .filter(f => f.playerIndex === playerIndex && changes.has(f.key))
      .map(f => [f.id, changes.get(f.key)]));
    try {
      identityOverrides(save.players[playerIndex], edits, raceMod);
      return '';
    } catch (error) { return error.message; }
  }
  function exportSave(save, changes, {raceMod=false}={}) {
    const output = save.archive.data.slice(), view = new DataView(output.buffer);
    const known = new Map(save.fields.map(f => [f.key, f])), written = new Map();
    const arrays = new Map(), aptitude = new Map(), identities = new Map(), overrides = new Map();
    const identity = new Map(identityFields(save).map(f => [f.key,f]));
    const queueArray = (player, name, index, value, mask = 0xffffffff, shift = 0) => {
      let edits = arrays.get(player);
      if (!edits) { edits = new Map(); arrays.set(player, edits); }
      let edit = edits.get(name);
      if (!edit) {
        const field = player.fields.get(name), values = new Map();
        for (const [key, element] of field?.elements ?? []) values.set(key, view.getInt32(element.offset, true));
        edit = {field, values, rebuild: !field?.elements}; edits.set(name, edit);
      }
      const raw = edit.values.get(index) ?? 0;
      const merged = (raw & ~(mask << shift)) | ((value & mask) << shift);
      if (!edit.values.has(index)) edit.rebuild = true;
      edit.values.set(index, merged);
      const existing = edit.field?.elements?.get(index);
      if (existing) view.setInt32(existing.offset, merged, true);
      // An omitted mastery element must not accidentally grant college 0 mastery.
      if (name === 'CRITTER_SPELL_TECH_IDX' && !edit.values.has(16)) {
        edit.values.set(16, -1); edit.rebuild = true;
      }
    };
    for (const [key, value] of changes) {
      if (identity.has(key)) {
        const f=identity.get(key), player=save.players[f.playerIndex];
        if(value!==f.original) {
          if(!identities.has(player)) identities.set(player,new Map());
          identities.get(player).set(f.id,value);
        }
        continue;
      }
      const field = known.get(key);
      check(field, 'Unknown field.');
      check(typeof value === 'number' && Number.isInteger(value) && value >= field.min && value <= field.max, `Invalid value for ${field.label}.`);
      if (field.arrayName) continue;
      check(!written.has(field.offset) || written.get(field.offset) === value, 'Conflicting changes.');
      written.set(field.offset, value); view.setInt32(field.offset, value, true);
    }
    for (const [key, value] of changes) {
      const field = known.get(key);
      if (!field?.arrayName || value === field.original) continue;
      const player = save.players[field.playerIndex];
      queueArray(player, field.arrayName, field.element, value, field.mask, field.shift);
      if (field.aptitude) {
        const delta = aptitude.get(player) ?? {magic: 0, tech: 0};
        delta[field.aptitude] += value - field.original; aptitude.set(player, delta);
      }
    }
    for (const [player, delta] of aptitude) for (const [kind, stat] of [['magic', 22], ['tech', 23]]) {
      if (!delta[kind]) continue;
      const before = player.fields.get('CRITTER_STAT_BASE_IDX')?.elements?.get(stat)?.value ?? 0;
      queueArray(player, 'CRITTER_STAT_BASE_IDX', stat, Math.max(0, Math.min(210, before + delta[kind])));
    }
    for (const [player, edits] of identities) {
      overrides.set(player, identityOverrides(player, edits, raceMod));
      for (const [id, stat] of [['gender', 26], ['race', 27]]) {
        if (edits.has(id)) queueArray(player, 'CRITTER_STAT_BASE_IDX', stat, edits.get(id));
      }
    }
    const replacements = new Map();
    for (const player of new Set([...arrays.keys(),...overrides.keys()])) {
      const edits=arrays.get(player) ?? new Map();
      if (!overrides.has(player) && ![...edits.values()].some(e => e.rebuild)) continue;
      const fields = new Map();
      for (const [name, field] of player.fields) fields.set(name, output.subarray(field.start, field.end));
      for (const [name, edit] of edits) if (edit.rebuild) fields.set(name, serializeArray(edit));
      for (const [name,data] of overrides.get(player) ?? []) fields.set(name,data);
      const header = output.slice(player.offset, player.offset + 58 + player.layout.words * 4);
      const h = new DataView(header.buffer); h.setUint16(56, fields.size, true);
      const parts = [header];
      for (const f of player.layout.fields) if (fields.has(f.name)) {
        h.setUint32(58 + f.word * 4, h.getUint32(58 + f.word * 4, true) | (1 << f.bit), true);
        parts.push(fields.get(f.name));
      }
      replacements.set(player, concat(parts));
    }
    const index = save.archive.index.slice();
    if (!replacements.size) return {index, data: output};
    const indexView = new DataView(index.buffer), parts = [];
    for (const entry of save.archive.entries) {
      const objects = [...replacements.keys()].filter(p => p.entry === entry).sort((a, b) => a.offset - b.offset);
      let cursor = entry.offset, size = entry.size;
      for (const object of objects) {
        const replacement = replacements.get(object);
        parts.push(output.subarray(cursor, object.offset), replacement);
        size += replacement.length - (object.end - object.offset); cursor = object.end;
      }
      parts.push(output.subarray(cursor, entry.offset + entry.size));
      indexView.setUint32(entry.sizeOffset, size, true);
    }
    return {index, data: concat(parts)};
  }
  function concat(parts) {
    const result = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
    let offset = 0; for (const part of parts) { result.set(part, offset); offset += part.length; }
    return result;
  }
  function serializeArray({field, values}) {
    const keys = [...values.keys()].sort((a, b) => a - b);
    const words = Math.max(field?.words ?? 0, Math.ceil(((keys.at(-1) ?? -1) + 1) / 32));
    const data = new Uint8Array(1 + 12 + keys.length * 4 + 4 + words * 4), v = new DataView(data.buffer);
    data[0] = 1; v.setUint32(1, 4, true); v.setUint32(5, keys.length, true); v.setUint32(9, field?.bitsetId ?? 0, true);
    const bitset = 13 + keys.length * 4; v.setUint32(bitset, words, true);
    keys.forEach((key, rank) => {
      v.setInt32(13 + rank * 4, values.get(key), true);
      const offset = bitset + 4 + (key >> 5) * 4;
      v.setUint32(offset, v.getUint32(offset, true) | (1 << (key % 32)), true);
    });
    return data;
  }
  function updateGsiName(data, save, changes) {
    const renames = identityFields(save).filter(f => f.id === 'name' && changes.has(f.key));
    if (!renames.length) return data;
    const r = new Reader(bytes(data));
    r.u32(); // version
    r.take(r.u32()); // module name
    const start = r.pos;
    const oldName = decodeGameText(r.take(r.u32()));
    const field = save.players.length === 1 ? renames[0] : renames.find(f => f.original === oldName);
    if (!field) return data;
    if (save.players.length > 1) {
      check(save.players.filter(p => savedIdentity(p).name === oldName).length === 1, 'Ambiguous name in the .gsi file.');
    }
    const name = encodeName(changes.get(field.key));
    return concat([data.subarray(0, start), numberBytes(name.length), name, data.subarray(r.pos)]);
  }
  function parseMes(data) {
    data = bytes(data);
    let text;
    try { text = new TextDecoder('utf-8', {fatal: true}).decode(data); }
    catch { text = decodeGameText(data); }
    const entries = new Map();
    for (const match of text.matchAll(/\{\s*(-?\d+)\s*\}\s*\{([^}]*)\}/g)) entries.set(Number(match[1]), match[2]);
    check(entries.size, 'No MES entries recognized.');
    return entries;
  }
  // ZIP (store) keeps the original filenames and avoids multiple-download prompts.
  function crc32(data) {
    let crc = 0xffffffff;
    for (const byte of data) { crc ^= byte; for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0); }
    return (crc ^ 0xffffffff) >>> 0;
  }
  function makeZip(files) {
    check(files.length <= 65535, 'Too many files.');
    const chunks = [], central = []; let offset = 0, centralSize = 0;
    for (const file of files) {
      check(!/[\\/\x00-\x1f:]/.test(file.name) && !['.', '..', ''].includes(file.name), 'Invalid ZIP filename.');
      const name = new TextEncoder().encode(file.name), data = bytes(file.data), crc = crc32(data);
      check(name.length <= 65535 && data.length < 0xffffffff, 'File is too large.');
      const local = new Uint8Array(30 + name.length), l = new DataView(local.buffer);
      l.setUint32(0, 0x04034b50, true); l.setUint16(4, 20, true); l.setUint16(6, 0x800, true);
      l.setUint16(12, 33, true); l.setUint32(14, crc, true); l.setUint32(18, data.length, true);
      l.setUint32(22, data.length, true); l.setUint16(26, name.length, true); local.set(name, 30);
      const record = new Uint8Array(46 + name.length), c = new DataView(record.buffer);
      c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true);
      c.setUint16(8, 0x800, true); c.setUint16(14, 33, true); c.setUint32(16, crc, true);
      c.setUint32(20, data.length, true); c.setUint32(24, data.length, true); c.setUint16(28, name.length, true);
      c.setUint32(42, offset, true); record.set(name, 46);
      chunks.push(local, data); central.push(record); offset += local.length + data.length; centralSize += record.length;
    }
    check(offset + centralSize + 22 < 0xffffffff, 'ZIP archive is too large.');
    const end = new Uint8Array(22), e = new DataView(end.buffer);
    e.setUint32(0, 0x06054b50, true); e.setUint16(8, files.length, true); e.setUint16(10, files.length, true);
    e.setUint32(12, centralSize, true); e.setUint32(16, offset, true);
    return [...chunks, ...central, end];
  }
  globalThis.ArcanumSave = {identityError, identityFields, encodeName, updateGsiName, parseArchive, openSave, exportSave, parseMes, makeZip, crc32, objectLayout};
})();
