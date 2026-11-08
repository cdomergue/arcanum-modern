(function () {
  'use strict';
  const api = globalThis.ArcanumSave, $ = id => document.getElementById(id);
  let save = null, files = [], slot = '', changes = new Map(), invalid = new Set();
  let translations = {stat: new Map(), skill: new Map(), spell: new Map()}, playerIndex = 0, filter = 'all', busy = false;
  let raceMod = false;
  const schema = globalThis.ArcanumSchema;
  if (globalThis.ArcanumLocalMessages) {
    for (const name of ['stat', 'skill', 'spell']) translations[name] = new Map(globalThis.ArcanumLocalMessages[name] ?? []);
    $('language-status').textContent = 'Textes de votre jeu inclus dans ce fichier local.';
  }
  const groupNames = {identity:'Personnage', attributes: 'Attributs', progress: 'Progression', resources: 'Ressources', skills: 'Compétences', magic: 'Magies'};
  const fmt = value => value.toLocaleString('fr-FR');
  function label(field) {
    if (field.skill !== undefined) return (translations.skill.get(field.skill) || schema.skills[field.skill]) + (field.training ? ' · entraînement' : ' · rang');
    if (field.college !== undefined) return translations.spell.get(500 + field.college) || field.label;
    return (field.stat !== null && translations.stat.get(field.stat)) || field.label;
  }
  function trainingLabel(value) { return translations.skill.get(16 + value) || schema.training[value]; }
  function valueLabel(field, value) { if (field.group === 'identity') return field.choices?.[value] ?? value ?? 'Hérité'; return field.training ? trainingLabel(value) : fmt(value); }
  function status(message, error = false) { $('status').textContent = message; $('status').classList.toggle('error', error); }
  function textNode(tag, text, className) {
    const node = document.createElement(tag); node.textContent = text;
    if (className) node.className = className; return node;
  }
  function updateReview() {
    $('change-count').textContent = changes.size;
    $('review-empty').hidden = changes.size > 0;
    $('invalid-note').hidden = invalid.size === 0;
    $('export').disabled = !save || busy || invalid.size > 0 || changes.size === 0;
    $('reset').disabled = busy || (changes.size === 0 && invalid.size === 0);
    $('changes').replaceChildren();
    if (!save) return;
    for (const field of [...api.identityFields(save),...save.fields]) if (changes.has(field.key)) {
      const name = save.players.length > 1 ? `${save.players[field.playerIndex].name} · ` : '';
      const row = textNode('li', name + label(field));
      row.append(textNode('strong', `${valueLabel(field, field.original)} → ${valueLabel(field, changes.get(field.key))}`));
      $('changes').append(row);
    }
    const deltas = new Map();
    for (const field of save.fields) if (field.aptitude && changes.has(field.key)) {
      const key = `${field.playerIndex}:${field.aptitude}`;
      deltas.set(key, (deltas.get(key) ?? 0) + changes.get(field.key) - field.original);
    }
    for (const [key, delta] of deltas) if (delta) {
      const [index, kind] = key.split(':'), player = save.players[Number(index)], stat = kind === 'magic' ? 22 : 23;
      const original = player.fields.get('CRITTER_STAT_BASE_IDX')?.elements?.get(stat)?.value ?? 0;
      const row = textNode('li', `${player.name} · ${schema.stats[stat]} (automatique)`);
      row.append(textNode('strong', `${fmt(original)} → ${fmt(Math.max(0, Math.min(210, original + delta)))}`)); $('changes').append(row);
    }
  }
  function checkIdentity() {
    for (const [i] of save.players.entries()) {
      const message = api.identityError(save, changes, i, raceMod), key = i + ':identity:error';
      if (message) invalid.add(key);
      else invalid.delete(key);
      if (i === playerIndex && $('identity-error')) $('identity-error').textContent = message;
    }
    $('character-name').textContent = changes.get(playerIndex + ':identity:name') || save.players[playerIndex].name;
    Array.from($('player-select').options).forEach((option, i) => {
      const name = changes.get(i + ':identity:name') || save.players[i].name;
      option.textContent = name + ' · ' + save.players[i].entry.path;
    });
    updateReview();
  }
  function identitySection(fields) {
    const section = textNode('section', '', 'field-group');
    section.append(textNode('h3', 'Identité du personnage'));
    const grid = textNode('div', '', 'fields-grid');
    for (const field of fields) {
      const box = textNode('label', '', 'field identity-field');
      const input = document.createElement(field.choices ? 'select' : 'input');
      box.append(textNode('span', field.label));
      if (field.choices) {
        if (field.original === undefined) {
          const option = textNode('option', 'Hérité du prototype');
          option.value = '';
          input.append(option);
        }
        field.choices.forEach((name, value) => {
          const option = textNode('option', name);
          option.value = value;
          option.disabled = field.id === 'race' && value >= 8 && !raceMod;
          input.append(option);
        });
      } else input.maxLength = 23;
      input.value = changes.get(field.key) ?? field.original ?? '';
      input.addEventListener('input', () => {
        const value = field.choices ? (input.value === '' ? undefined : Number(input.value)) : input.value;
        if (value === field.original) changes.delete(field.key);
        else changes.set(field.key, value);
        box.classList.toggle('changed', changes.has(field.key));
        checkIdentity();
      });
      box.classList.toggle('changed', changes.has(field.key));
      box.append(input, textNode('small', 'Valeur initiale : ' + valueLabel(field, field.original)));
      grid.append(box);
    }
    const profile = textNode('label', '', 'field-note'), toggle = document.createElement('input');
    toggle.type = 'checkbox';
    toggle.checked = raceMod;
    toggle.addEventListener('change', () => {
      raceMod = toggle.checked;
      renderFields();
    });
    profile.append(toggle, document.createTextNode(' Mon jeu utilise le Race Mod de l’UAP.'));
    const error = textNode('p', '', 'field-error');
    error.id = 'identity-error';
    error.setAttribute('aria-live', 'polite');
    section.append(grid, profile, textNode('p', 'Le portrait et l’historique sont conservés. Importez aussi le .gsi pour afficher le nouveau nom dans le menu Charger.', 'field-note'), error);
    return section;
  }
  function renderFields() {
    $('fields').replaceChildren();
    if (!save) return;
    const player = save.players[playerIndex];
    $('character-name').textContent = changes.get(playerIndex+':identity:name') || player.name;
    $('character-meta').textContent = `${player.level === undefined ? 'Niveau non enregistré' : `Niveau ${player.level}`} · ${save.archive.entries.length} fichiers · ${fmt(save.archive.data.length)} octets`;
    let visible = 0;
    for (const [group, title] of Object.entries(groupNames)) {
      if (filter !== 'all' && filter !== group) continue;
      if (group === 'identity') {
        const fields = api.identityFields(save).filter(f => f.playerIndex === playerIndex);
        $('fields').append(identitySection(fields));
        visible += fields.length;
        checkIdentity();
        continue;
      }
      const fields = save.fields.filter(f => f.playerIndex === playerIndex && f.group === group);
      if (!fields.length) continue;
      visible += fields.length;
      const section = document.createElement('section'); section.className = 'field-group';
      section.append(textNode('h3', title));
      if (group === 'skills') section.append(textNode('p', 'Rang de 0 à 5 ; l’entraînement se règle séparément. Le jeu limite l’efficacité selon les attributs. Les points disponibles ne sont pas consommés.', 'field-note'));
      if (group === 'magic') {
        section.append(textNode('p', 'Choisir un sort apprend aussi tous les rangs précédents de son collège. Décocher un sort retire les rangs suivants. Les prérequis d’utilisation du jeu restent applicables.', 'field-note'));
        const learn = textNode('button', 'Apprendre les 80 sorts', 'button subtle'); learn.type = 'button';
        learn.addEventListener('click', () => {
          for (const field of fields) { if (field.original === 5) changes.delete(field.key); else changes.set(field.key, 5); invalid.delete(field.key); }
          renderFields(); updateReview(); status('Les 16 collèges sont au rang 5 dans la copie à exporter.');
        }); section.append(learn);
      }
      const grid = document.createElement('div'); grid.className = 'fields-grid';
      for (const field of fields) {
        const box = document.createElement('div'); box.className = 'field';
        box.classList.toggle('changed', changes.has(field.key));
        const name = textNode('label', label(field)); name.htmlFor = `field-${field.key}`;
        const control = document.createElement('div'); control.className = 'field-controls';
        const input = document.createElement(field.training ? 'select' : 'input'); input.id = name.htmlFor;
        if (field.training) for (let value = 0; value <= 3; value++) { const option = textNode('option', trainingLabel(value)); option.value = value; input.append(option); }
        else { input.type = 'number'; input.min = field.min; input.max = field.max; input.step = 1; }
        input.value = invalid.has(field.key) ? '' : (changes.get(field.key) ?? field.original);
        input.setAttribute('aria-describedby', `hint-${field.key} error-${field.key}`);
        const hint = textNode('small', `Initial : ${valueLabel(field, field.original)}`); hint.id = `hint-${field.key}`;
        const error = textNode('span', '', 'field-error'); error.id = `error-${field.key}`;
        function setError(isInvalid) {
          input.setAttribute('aria-invalid', String(isInvalid));
          error.textContent = isInvalid ? `Entier de ${fmt(field.min)} à ${fmt(field.max)} attendu.` : '';
        }
        setError(invalid.has(field.key));
        const spellChecks = [];
        input.addEventListener('input', () => {
          const value = field.training ? Number(input.value) : input.valueAsNumber;
          const valid = input.value !== '' && Number.isInteger(value) && value >= field.min && value <= field.max;
          if (valid) {
            invalid.delete(field.key);
            if (value === field.original) changes.delete(field.key); else changes.set(field.key, value);
          } else { invalid.add(field.key); changes.delete(field.key); }
          setError(!valid); box.classList.toggle('changed', changes.has(field.key));
          for (const [rank, checkbox] of spellChecks) checkbox.checked = valid && value >= rank;
          updateReview();
        });
        control.append(input, hint); box.append(name, control, error);
        if (field.college !== undefined) {
          const list = document.createElement('div'); list.className = 'spell-list';
          for (let rank = 1; rank <= 5; rank++) {
            const spell = field.college * 5 + rank - 1, row = document.createElement('label');
            const checkbox = document.createElement('input'); checkbox.type = 'checkbox';
            checkbox.checked = Number(input.value) >= rank; spellChecks.push([rank, checkbox]);
            checkbox.addEventListener('change', () => {
              input.value = checkbox.checked ? rank : rank - 1;
              input.dispatchEvent(new Event('input', {bubbles: true}));
            });
            row.append(checkbox, textNode('span', `${rank}. ${translations.spell.get(spell) || schema.spells[spell]}`)); list.append(row);
          } box.append(list);
        }
        grid.append(box);
      }
      section.append(grid); $('fields').append(section);
    }
    $('no-fields').hidden = visible > 0;
  }
  async function importFiles(selected) {
    if (!selected.length || busy) return;
    if ((changes.size || invalid.size) && !window.confirm('Abandonner les modifications en cours et ouvrir cette sauvegarde ?')) return;
    busy = true; updateReview(); status('Lecture et vérification des fichiers…');
    try {
      const incoming = Array.from(selected), names = new Set();
      if (incoming.reduce((n, f) => n + f.size, 0) > 128 * 1024 * 1024) throw new Error('La sélection dépasse la limite de 128 Mo.');
      for (const file of incoming) {
        const name = file.name.toLowerCase();
        if (names.has(name)) throw new Error('La sélection contient des noms de fichiers en double.');
        names.add(name);
      }
      const indexes = incoming.filter(f => /\.tfai$/i.test(f.name));
      const archives = incoming.filter(f => /\.tfaf$/i.test(f.name));
      if (indexes.length !== 1 || archives.length !== 1) throw new Error('Sélectionnez ensemble un fichier .tfai et un fichier .tfaf d’une seule sauvegarde.');
      const base = indexes[0].name.slice(0, -5);
      if (archives[0].name.slice(0, -5).toLowerCase() !== base.toLowerCase()) throw new Error('Les fichiers .tfai et .tfaf doivent porter le même nom.');
      for (const file of incoming) {
        const name = file.name.toLowerCase(), stem = base.toLowerCase();
        if (file === indexes[0] || file === archives[0]) continue;
        if (name !== `${stem}.bmp` && !(name.startsWith(stem) && name.endsWith('.gsi'))) throw new Error(`Fichier associé inattendu : ${file.name}`);
      }
      if (incoming.filter(f => /\.gsi$/i.test(f.name)).length > 1) throw new Error('Sélectionnez un seul fichier .gsi pour cet emplacement.');
      const loaded = await Promise.all(incoming.map(async f => ({name: f.name, data: new Uint8Array(await f.arrayBuffer())})));
      const next = api.openSave(loaded.find(f => f.name === indexes[0].name).data, loaded.find(f => f.name === archives[0].name).data);
      raceMod=next.players.some(p=>{
        const race=p.fields.get('CRITTER_STAT_BASE_IDX')?.elements?.get(27)?.value;
        return race>=8 || (race===3 && ((p.fields.get('AID')?.value>>>24)&7)===4);
      });
      save = next; files = loaded; slot = base; changes = new Map(); invalid = new Set(); playerIndex = 0;
      $('empty-state').hidden = true; $('editor').hidden = false;
      $('slot-name').textContent = `SAUVEGARDE / ${slot}`;
      $('player-select').replaceChildren(...save.players.map((player, i) => {
        const option = textNode('option', `${player.name} · ${player.entry.path}`); option.value = i; return option;
      }));
      $('player-select').disabled = save.players.length === 1;
      renderFields(); status(`${slot} ouverte. ${save.fields.length + api.identityFields(save).length} champs modifiables. Les fichiers d’origine sont préservés.`);
    } catch (error) { status(error.message, true); }
    finally { busy = false; updateReview(); $('files-input').value = ''; }
  }
  $('choose-files').addEventListener('click', () => $('files-input').click());
  $('files-input').addEventListener('change', event => importFiles(event.target.files));
  document.addEventListener('dragover', event => { event.preventDefault(); if (!save) $('empty-state').classList.add('dragging'); });
  document.addEventListener('dragleave', event => { if (!event.relatedTarget) $('empty-state').classList.remove('dragging'); });
  document.addEventListener('drop', event => { event.preventDefault(); $('empty-state').classList.remove('dragging'); importFiles(event.dataTransfer.files); });
  $('player-select').addEventListener('change', event => { playerIndex = Number(event.target.value); renderFields(); });
  document.querySelectorAll('[data-filter]').forEach(button => button.addEventListener('click', () => {
    filter = button.dataset.filter;
    document.querySelectorAll('[data-filter]').forEach(b => { b.classList.toggle('active', b === button); b.setAttribute('aria-pressed', String(b === button)); });
    renderFields();
  }));
  $('fields-form').addEventListener('submit', event => event.preventDefault());
  $('reset').addEventListener('click', () => {
    changes.clear(); invalid.clear(); renderFields(); updateReview(); status('Toutes les valeurs d’origine ont été rétablies.');
  });
  $('mes-input').addEventListener('change', async event => {
    const files = Array.from(event.target.files); if (!files.length) return;
    try {
      const next = {...translations};
      for (const file of files) {
        if (file.size > 2 * 1024 * 1024) throw new Error('Un fichier MES dépasse 2 Mo.');
        const name = file.name.toLowerCase().replace(/\.mes$/, '');
        if (!['stat', 'skill', 'spell'].includes(name)) throw new Error('Importez stat.mes, skill.mes ou spell.mes provenant du dossier mes du jeu.');
        const entries = api.parseMes(await file.arrayBuffer()), count = {stat: 28, skill: 20, spell: 80}[name];
        if (!Array.from({length: count}, (_, i) => i).every(i => entries.has(i))) throw new Error(`${file.name} ne contient pas toutes les entrées attendues.`);
        next[name] = entries;
      }
      translations = next; $('language-status').textContent = Object.keys(next).filter(k => next[k].size).map(k => `${k}.mes`).join(', ') + ' chargés';
      renderFields(); updateReview(); status('Les noms affichés proviennent maintenant de vos fichiers MES.');
    } catch (error) { status(error.message, true); }
    finally { event.target.value = ''; }
  });
  $('export').addEventListener('click', async () => {
    if (!save || !changes.size || invalid.size || busy) return;
    busy = true; updateReview(); status('Préparation de la copie…');
    await new Promise(resolve => setTimeout(resolve, 0));
    try {
      const result = api.exportSave(save, changes,{raceMod});
      const output = files.map(file => ({name: file.name, data: /\.tfaf$/i.test(file.name) ? result.data : /\.tfai$/i.test(file.name) ? result.index : /\.gsi$/i.test(file.name) ? api.updateGsiName(file.data,save,changes) : file.data}));
      const blob = new Blob(api.makeZip(output), {type: 'application/zip'});
      const url = URL.createObjectURL(blob), link = document.createElement('a');
      link.href = url; link.download = `${slot}-modifiee.zip`; document.body.append(link); link.click(); link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 60000);
      status(`Copie préparée : ${slot}-modifiee.zip. Décompressez-la pour installer les fichiers, après avoir conservé l’original.`);
    } catch (error) { status(error.message, true); }
    finally { busy = false; updateReview(); }
  });
  window.addEventListener('beforeunload', event => { if (changes.size || invalid.size) { event.preventDefault(); event.returnValue = ''; } });
})();
