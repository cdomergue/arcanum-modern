/* Run: node --test tests/test_save_editor.cjs
 * Optional read-only real-save coverage: ARCANUM_SAVE_TEST_DIR=/path/to/Save
 */
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const {spawnSync} = require('node:child_process');
require('../tools/save-editor/schema.js');
require('../tools/save-editor/save-format.js');
const api = globalThis.ArcanumSave;
const u32 = n => { const b = Buffer.alloc(4); b.writeUInt32LE(n >>> 0); return b; };
const u16 = n => { const b = Buffer.alloc(2); b.writeUInt16LE(n); return b; };
const cat = (...buffers) => Buffer.concat(buffers);
const oid = n => cat(u32(2), u32(0), u32(n), Buffer.alloc(12));
const proto = cat(u32(1), u32(0), u32(1000), Buffer.alloc(12));
const record = (name, data) => cat(u32(0), u32(name.length), Buffer.from(name), u32(data.length));

function fixture({goldReference = 2, quest = true} = {}) {
  // Hard-coded field masks/ordinals from the engine, independent of schema.js.
  const statKeys = [0, 1, 17, 18, 19, 20, 21];
  const stats = cat(Buffer.from([1]), u32(4), u32(7), u32(42),
    ...[8, 9, 4, 3200, -100, 2, 1].map(u32), u32(1), u32(statKeys.reduce((m, i) => m | 1 << i, 0)));
  const name = Buffer.from('H\xe9l\xe8ne <&>', 'latin1');
  const quests = quest ? cat(Buffer.from([1]), u32(16), u32(1), u32(99), Buffer.alloc(16), u32(1), u32(1 << 5)) : Buffer.alloc(0);
  const pc = cat(u32(119), proto, oid(1), u32(15), u16(quest ? 5 : 4),
    u32(0), u32(0), u32((1 << 2) | (1 << 14)), u32(0), u32((1 << 17) | (1 << 18) | (quest ? (1 << 6) : 0)),
    stats, Buffer.from([1]), oid(goldReference), quests, Buffer.from([1]), u32(name.length), name, Buffer.from([0]), u32(550));
  const gold = cat(u32(119), proto, oid(2), u32(8), u16(1), u32(0), u32(0), u32(0), u32(2), u32(123));
  const mobile = cat(pc, gold), opaque = Buffer.from([119, 0, 255, 14, 39, 0, 8]);
  const index = cat(record('data.sav', opaque), u32(1), u32(4), Buffer.from('maps'),
    u32(1), u32(4), Buffer.from('Test'), record('mobile.mdy', mobile), u32(2), u32(2), u32(3));
  return {index, data: cat(opaque, mobile)};
}
function opened() { const f = fixture(); return api.openSave(f.index, f.data); }

test('parse sparse arrays, accented name, quest array and gold reference', () => {
  const save = opened();
  assert.equal(save.players[0].name, 'Hélène <&>');
  assert.equal(save.players[0].level, 4);
  assert.equal(save.objects.length, 2);
  assert.equal(save.fields.find(f => f.label === 'ALIGNMENT').original, -100);
  assert.equal(save.fields.find(f => f.label === 'GOLD').original, 123);
  assert.equal(save.fields.find(f => f.label === 'PC_BANK_MONEY').original, 550);
  assert(!save.fields.some(f => f.label === 'CONSTITUTION')); // absent is not zero
});
test('no-op is byte identical; caller buffers and original remain unchanged', () => {
  const source = fixture(), before = Buffer.from(source.data), save = api.openSave(source.index, source.data);
  const output = api.exportSave(save, new Map());
  assert.deepEqual(Buffer.from(output.index), source.index);
  assert.deepEqual(Buffer.from(output.data), before);
  output.data[0] ^= 0xff;
  assert.deepEqual(Buffer.from(save.archive.data), before);
  source.data[0] ^= 0xff;
  assert.deepEqual(Buffer.from(save.archive.data), before);
});
test('edits only chosen int32 fields and preserves opaque entries', () => {
  const save = opened(), chosen = save.fields.filter(f => ['STRENGTH', 'UNSPENT_POINTS', 'GOLD'].includes(f.label));
  const changes = new Map(chosen.map(f => [f.key, f.original + 3]));
  const output = api.exportSave(save, changes), edited = api.openSave(output.index, output.data);
  for (const f of chosen) assert.equal(edited.fields.find(v => v.key === f.key).original, f.original + 3);
  const allowed = new Set(chosen.flatMap(f => [f.offset, f.offset + 1, f.offset + 2, f.offset + 3]));
  for (let i = 0; i < output.data.length; i++) if (!allowed.has(i)) assert.equal(output.data[i], save.archive.data[i]);
  assert.equal(output.data.length, save.archive.data.length);
  assert.deepEqual(output.index, save.archive.index);
});
test('rejects invalid writes, including NaN, fractional and unknown fields', () => {
  const save = opened(), field = save.fields.find(f => f.label === 'STRENGTH');
  for (const value of [NaN, Infinity, 1.5, '10', -1, 41]) assert.throws(() => api.exportSave(save, new Map([[field.key, value]])), /Invalid value/);
  assert.throws(() => api.exportSave(save, new Map([['unknown', 10]])), /Unknown field/);
});
test('unresolved gold handle never becomes an arbitrary editable stack', () => {
  const f = fixture({goldReference: 999}), save = api.openSave(f.index, f.data);
  assert(!save.fields.some(f => f.label === 'GOLD'));
});
test('adds absent skill and spell arrays, preserves training bits, adjusts aptitudes', () => {
  const save = opened();
  const changes = new Map([['0:STRENGTH', 11], ['0:MELEE', 5], ['0:MELEE_TRAINING', 3],
    ['0:FIREARMS', 4], ['0:FIREARMS_TRAINING', 2], ['0:FIRE', 5], ['0:GOLD', 900]]);
  const output = api.exportSave(save, changes), edited = api.openSave(output.index, output.data);
  assert(output.data.length > save.archive.data.length);
  for (const [key, value] of changes) assert.equal(edited.fields.find(f => f.key === key).original, value);
  const pc = edited.players[0];
  assert.equal(pc.fields.get('CRITTER_BASIC_SKILL_IDX').elements.get(2).value, 5 | (3 << 6));
  assert.equal(pc.fields.get('CRITTER_TECH_SKILL_IDX').elements.get(1).value, 4 | (2 << 6));
  assert.equal(pc.fields.get('CRITTER_SPELL_TECH_IDX').elements.get(16).value, -1);
  assert.equal(pc.fields.get('CRITTER_STAT_BASE_IDX').elements.get(22).value, 5);
  assert.equal(pc.fields.get('CRITTER_STAT_BASE_IDX').elements.get(23).value, 4);
  // Entries not edited are exact copies even when following objects move.
  const before = save.archive.entries[0], after = edited.archive.entries[0];
  assert.deepEqual(output.data.subarray(after.offset, after.offset + after.size), save.archive.data.subarray(before.offset, before.offset + before.size));
  const unmodifiedField = 'PC_QUEST_IDX', a = save.players[0].fields.get(unmodifiedField), b = pc.fields.get(unmodifiedField);
  assert.deepEqual(output.data.subarray(b.start, b.end), save.archive.data.subarray(a.start, a.end));
  // Adding another college exercises insertion into an existing sparse array.
  const second = api.exportSave(edited, new Map([['0:WATER', 3], ['0:FIRE', 2], ['0:MELEE', 1]]));
  const reopened = api.openSave(second.index, second.data);
  assert.equal(reopened.fields.find(f => f.key === '0:MELEE_TRAINING').original, 3);
  assert.equal(reopened.fields.find(f => f.key === '0:WATER').original, 3);
  assert.equal(reopened.players[0].fields.get('CRITTER_STAT_BASE_IDX').elements.get(22).value, 5);
});
test('learns all 80 spells, preserves technology slots and unrelated packed skill bits', () => {
  const save = opened();
  const learned = api.exportSave(save, new Map(save.fields.filter(f => f.group === 'magic').map(f => [f.key, 5])));
  const next = api.openSave(learned.index, learned.data);
  assert.equal(next.fields.filter(f => f.group === 'magic').reduce((sum, f) => sum + f.original, 0), 80);

  const second = api.exportSave(next, new Map([['0:FIREARMS', 4], ['0:FIREARMS_TRAINING', 3], ['0:FIRE', 2]]));
  const again = api.openSave(second.index, second.data);
  for (const [key, value] of [['0:FIREARMS',4], ['0:FIREARMS_TRAINING',3], ['0:FIRE',2], ['0:WATER',5]])
    assert.equal(again.fields.find(f => f.key === key).original, value);
});
test('refuses mismatched archives and truncated indices', () => {
  const f = fixture();
  assert.throws(() => api.openSave(f.index, f.data.subarray(0,-1)));
  assert.throws(() => api.openSave(f.index.subarray(0,-1), f.data));
  assert.throws(() => api.openSave(f.index, cat(f.data, Buffer.of(0))));
});
test('rejects unsafe paths and duplicate entries', () => {
  for (const name of ['..', 'x/y', 'C:x']) assert.throws(() => api.parseArchive(cat(record(name, []),u32(3)), Buffer.alloc(0)));
  assert.throws(() => api.parseArchive(cat(record('a', []), record('A', []), u32(3)), Buffer.alloc(0)));
});
test('refuses unsupported objects and malformed field masks', () => {
  const f = fixture(), start = opened().players[0].offset;
  for (const [offset,value] of [[0,120],[58,0x80000000]]) {
    const data = Buffer.from(f.data); data.writeUInt32LE(value,start+offset);
    assert.throws(() => api.openSave(f.index,data));
  }
});
test('optional quest record may be absent', () => {
  const f=fixture({quest:false}), save=api.openSave(f.index, f.data);
  assert.equal(save.players[0].fields.has('PC_QUEST_IDX'),false);
  assert.equal(save.fields.find(v => v.key==='0:GOLD').original,123);
});
test('reads MES text in UTF-8 and Windows-1252', () => {
  for (const enc of ['utf8','latin1']) assert.equal(api.parseMes(Buffer.from('{7}{Épée}', enc)).get(7),'Épée');
  assert.throws(() => api.parseMes(Buffer.from('invalid')));
});

test('ZIP is independently readable with exact UTF-8 names, bytes and CRCs', () => {
  assert.equal(api.crc32(Buffer.from('123456789')), 0xcbf43926);
  const f = fixture(), companions = Buffer.from([0, 255, 12, 99]);
  const zipped = Buffer.concat(api.makeZip([{name: 'Slot0001.tfai', data: f.index}, {name: 'Slot0001.tfaf', data: f.data}, {name: 'Slot0001Hélène.gsi', data: companions}]));
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'arcanum-save-zip-test-'));
  try {
    fs.writeFileSync(path.join(dir, 'test.zip'), zipped);
    fs.writeFileSync(path.join(dir, 'expected.tfai'), f.index); fs.writeFileSync(path.join(dir, 'expected.tfaf'), f.data);
    const result = spawnSync(process.env.PYTHON || (process.platform === 'win32' ? 'python' : 'python3'), ['-c', 'import sys,zipfile,pathlib; p=pathlib.Path(sys.argv[1]); z=zipfile.ZipFile(p/"test.zip"); assert z.testzip() is None; assert z.read("Slot0001.tfai")== (p/"expected.tfai").read_bytes(); assert z.read("Slot0001.tfaf")== (p/"expected.tfaf").read_bytes(); assert z.read("Slot0001Hélène.gsi")==bytes([0,255,12,99])', dir], {encoding: 'utf8'});
    assert.equal(result.status, 0, result.stderr);
  } finally { fs.rmSync(dir, {recursive: true, force: true}); }
  assert.throws(() => api.makeZip([{name: '../escape', data: Buffer.alloc(0)}]), /Invalid/);
});

const realDir = process.env.ARCANUM_SAVE_TEST_DIR;
test('installed saves: parse, exact no-op and bounded edits (read-only originals)', {skip: !realDir}, () => {
  const names = fs.readdirSync(realDir).filter(n => /\.tfai$/i.test(n));
  assert(names.length > 0);
  for (const name of names) {
    const base = name.slice(0, -5), index = fs.readFileSync(path.join(realDir, name)), data = fs.readFileSync(path.join(realDir, base + '.tfaf'));
    const save = api.openSave(index, data), unchanged = api.exportSave(save, new Map());
    assert.deepEqual(Buffer.from(unchanged.data), data);
    assert.deepEqual(Buffer.from(unchanged.index), index);
    const changes = new Map(save.fields.filter(f => !f.arrayName).map(f => [f.key, Math.max(f.min, Math.min(f.max, f.original + 1))]));
    const edited = api.exportSave(save, changes), reparsed = api.openSave(edited.index, edited.data);
    for (const f of reparsed.fields) assert.equal(f.original, changes.get(f.key) ?? save.fields.find(v => v.key === f.key).original);
    const allowed = new Set(save.fields.filter(f => !f.arrayName).flatMap(f => [f.offset, f.offset + 1, f.offset + 2, f.offset + 3]));
    for (let i = 0; i < data.length; i++) if (!allowed.has(i)) assert.equal(edited.data[i], data[i]);
  }
});

test('renames a PC in CP1252 without changing the other objects or attributes', () => {
  const save = opened(), changes = new Map([['0:identity:name', 'Zoé d’Argent']]);
  const output = api.exportSave(save, changes), edited = api.openSave(output.index, output.data);
  assert.equal(edited.players[0].name, 'Zoé d’Argent');
  assert.deepEqual(edited.fields.map(f => f.original), save.fields.map(f => f.original));
  const oldGold = save.objects[1], newGold = edited.objects[1];
  assert.deepEqual(output.data.subarray(newGold.offset, newGold.end), save.archive.data.subarray(oldGold.offset, oldGold.end));
  for (const name of ['', ' ', 'x'.repeat(24), 'dé\n', 'test😀'])
    assert.throws(() => api.exportSave(save, new Map([['0:identity:name', name]])));
});

test('updates the selected PC, its body, sound, race and gender effects', () => {
  const save = opened();
  const change = (pc, race, gender, raceMod = false) => {
    const edits = new Map([['0:identity:gender', gender], ['0:identity:race', race], ['0:MELEE', 3]]);
    const out = api.exportSave(pc, edits, {raceMod});
    return api.openSave(out.index, out.data);
  };
  const elf = change(save, 2, 0);
  const fields = elf.players[0].fields;
  assert.equal((fields.get('AID').value >>> 24) & 15, 4);
  assert.equal(fields.get('SOUND_EFFECT').value, 50);
  assert.equal(elf.fields.find(f => f.key === '0:MELEE').original, 3);
  const effects = pc => Array.from(pc.players[0].fields.get('CRITTER_EFFECTS_IDX').elements.values(), e => e.value);
  assert.deepEqual(effects(elf), [66, 330]);
  const dwarf = change(elf, 1, 1);
  assert.deepEqual(effects(dwarf), [65]);
  assert.equal((dwarf.players[0].fields.get('CURRENT_AID').value >>> 24) & 15, 9);
  assert.throws(() => change(elf, 1, 0), /playable model/);
  assert.throws(() => change(elf, 10, 1), /UAP/);
  const orc = change(elf, 10, 1, true);
  assert.equal(orc.players[0].fields.get('CURRENT_AID').value >>> 0, 0xc1002000);
  const halfElf = change(orc, 3, 1, true);
  assert.equal((halfElf.players[0].fields.get('AID').value >>> 24) & 15, 12);
});

test('updates just the length and name of matching load-menu metadata', () => {
  const save = opened(), old = Buffer.from(save.players[0].name,'latin1'), tail = Buffer.from([255,8,0,18]);
  const module = cat(u32(11), u32(7), Buffer.from('Arcanum'));
  const data = cat(module, u32(old.length), old, tail);
  const renamed = api.updateGsiName(data, save, new Map([['0:identity:name', 'Zoé']]));
  assert.deepEqual(Buffer.from(renamed), cat(module, u32(3), Buffer.from('Zoé','latin1'), tail));
  assert.equal(api.updateGsiName(data, save, new Map()), data);
  assert.throws(() => api.updateGsiName(module, save, new Map([['0:identity:name', 'Zoé']])));
});
