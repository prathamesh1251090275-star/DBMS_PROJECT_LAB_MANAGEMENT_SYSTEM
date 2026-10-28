/* Lab Equipment Management - frontend (vanilla JS).
   Every page talks to the C++ API with fetch(). Hiding a menu item is only cosmetic:
   the server decides what each role may do. */
'use strict';

const $ = (sel, el = document) => el.querySelector(sel);
const state = { user: null, labs: [], cats: [] };

/* ---------- small helpers ---------- */
function esc(v) { // escape text before putting it into HTML (prevents XSS)
  return String(v ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}
function today(offsetDays = 0) {
  const d = new Date(); d.setDate(d.getDate() + offsetDays);
  return d.getFullYear() + '-' + String(d.getMonth() + 1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');
}
let toastTimer;
function toast(msg, isError = false) {
  const t = $('#toast'); t.textContent = msg; t.className = 'toast show' + (isError ? ' error' : '');
  clearTimeout(toastTimer); toastTimer = setTimeout(() => (t.className = 'toast'), 3500);
}
let currentSessionToken = localStorage.getItem('lab_session_token') || '';

async function api(method, path, body) {
  const opt = { method, headers: {}, credentials: 'include' };
  if (currentSessionToken) {
    opt.headers['x-session-id'] = currentSessionToken;
    opt.headers['Authorization'] = 'Bearer ' + currentSessionToken;
  }
  if (body !== undefined) { opt.headers['Content-Type'] = 'application/json'; opt.body = JSON.stringify(body); }
  let res;
  try { res = await fetch('/api' + path, opt); } catch (err) { console.error('Fetch error:', path, err); throw new Error('Cannot reach the server. Is it running?'); }
  let data = null;
  try { data = await res.json(); } catch { /* no JSON body */ }
  if (res.status === 401 && path !== '/auth/login' && path !== '/auth/me') {
    currentSessionToken = '';
    localStorage.removeItem('lab_session_token');
    showLogin('Please log in again.');
    throw new Error('Not logged in.');
  }
  if (!res.ok) throw new Error((data && data.error) || 'Request failed (' + res.status + ').');
  return data;
}
const pill = (text, kind) => `<span class="pill ${kind}">${esc(text)}</span>`;
const loading = () => '<div class="loading">Loading…</div>';

function table(cols, rows, emptyMsg = 'No records found.') {
  if (!rows.length) return `<div class="table-wrap"><div class="empty">${esc(emptyMsg)}</div></div>`;
  const head = cols.map(c => `<th>${esc(c.label)}</th>`).join('');
  const body = rows.map(r => '<tr>' + cols.map(c => {
    if (c.html) return `<td class="${c.cls || ''}">${c.html(r)}</td>`;
    return `<td class="${c.cls || ''}">${esc(r[c.key] ?? '')}</td>`;
  }).join('') + '</tr>').join('');
  return `<div class="table-wrap"><table><thead><tr>${head}</tr></thead><tbody>${body}</tbody></table></div>`;
}
// Buttons inside tables carry data-act / data-id; this wires them up after rendering.
function bindActions(root, handlers) {
  root.querySelectorAll('[data-act]').forEach(b => b.addEventListener('click', () => handlers[b.dataset.act](Number(b.dataset.id))));
}
const actionBtn = (act, id, label, cls = '') => `<button class="btn small ${cls}" data-act="${act}" data-id="${id}">${esc(label)}</button>`;

/* ---------- generic popup form ---------- */
// fields: [{name,label,type,options,required,min,max,step,numeric,hint,half}]
function openForm({ title, fields, values = {}, submitLabel = 'Save', onSubmit }) {
  const dlg = $('#dlg'), form = $('#dlg-form');
  const input = f => {
    const v = values[f.name] ?? '';
    const common = `name="${esc(f.name)}" id="f_${esc(f.name)}" ${f.required ? 'required' : ''}`;
    if (f.type === 'select')
      return `<select ${common}>${f.required ? '' : '<option value="">—</option>'}${f.options.map(o =>
        `<option value="${esc(o.value)}" ${String(o.value) === String(v) ? 'selected' : ''}>${esc(o.label)}</option>`).join('')}</select>`;
    if (f.type === 'textarea') return `<textarea ${common} rows="3" maxlength="${f.max || 500}">${esc(v)}</textarea>`;
    return `<input ${common} type="${f.type || 'text'}" value="${esc(v)}" ${f.min !== undefined ? `min="${f.min}"` : ''} ${f.max !== undefined ? `max="${f.max}"` : ''} ${f.step ? `step="${f.step}"` : ''} ${f.type === 'password' ? 'autocomplete="new-password"' : ''}>`;
  };
  const rows = []; let i = 0;
  while (i < fields.length) {
    if (fields[i].half && fields[i + 1] && fields[i + 1].half) { rows.push(`<div class="row2">${fieldHtml(fields[i])}${fieldHtml(fields[i + 1])}</div>`); i += 2; }
    else rows.push(fieldHtml(fields[i++]));
  }
  function fieldHtml(f) { return `<label>${esc(f.label)}${f.required ? ' *' : ''}${input(f)}${f.hint ? `<span class="hint">${esc(f.hint)}</span>` : ''}</label>`; }

  form.innerHTML = `<h2>${esc(title)}</h2>${rows.join('')}<div class="form-error" id="dlg-error" role="alert"></div>
    <div class="dlg-actions"><button type="button" class="btn" id="dlg-cancel">Cancel</button><button type="submit" class="btn primary" id="dlg-save">${esc(submitLabel)}</button></div>`;
  $('#dlg-cancel').onclick = () => dlg.close();
  form.onsubmit = async e => {
    e.preventDefault();
    const data = {};
    for (const f of fields) {
      const raw = form.elements[f.name].value.trim();
      if (raw === '') { if (f.keepEmpty) data[f.name] = ''; continue; }   // empty optional fields are simply left out
      data[f.name] = (f.numeric || f.type === 'number') ? Number(raw) : raw;
    }
    const btn = $('#dlg-save'); btn.disabled = true; $('#dlg-error').textContent = '';
    try { await onSubmit(data); dlg.close(); }
    catch (err) { $('#dlg-error').textContent = err.message; }
    finally { btn.disabled = false; }
  };
  dlg.showModal();
}
function confirmBox(message, onYes) {
  openForm({ title: 'Please confirm', fields: [], submitLabel: 'Yes, continue', onSubmit: onYes });
  $('#dlg-form h2').insertAdjacentHTML('afterend', `<p>${esc(message)}</p>`);
}

/* ---------- lookups (labs / categories) ---------- */
async function loadLookups() {
  [state.labs, state.cats] = await Promise.all([api('GET', '/labs'), api('GET', '/categories')]);
}
const labOptions = () => state.labs.map(l => ({ value: l.lab_id, label: l.lab_name }));
const catOptions = () => state.cats.map(c => ({ value: c.category_id, label: c.category_name }));
const CONDITIONS = ['Good', 'Fair', 'Poor'].map(c => ({ value: c, label: c }));

/* ---------- pages ---------- */
const pages = {};

pages.dashboard = {
  title: 'Dashboard',
  async render(view) {
    view.innerHTML = loading();
    const [d, over] = await Promise.all([api('GET', '/dashboard'), api('GET', '/reports/overdue')]);
    const card = (n, label, kind) => `<div class="stat ${kind}"><div class="num">${esc(n)}</div><div class="lbl">${esc(label)}</div></div>`;
    view.innerHTML = `<div class="cards">
      ${card(d.equipment_types, 'Equipment types', '')}
      ${card(d.total_quantity, 'Total quantity', 'info')}
      ${card(d.available_quantity, 'Available now', 'good')}
      ${card(d.issued_quantity, 'Currently issued', 'info')}
      ${card(d.under_maintenance_quantity, 'Under maintenance', 'warn')}
      ${card(d.overdue_quantity + ' (' + d.overdue_issues + ' issues)', 'Overdue units', 'bad')}</div>
      <div class="panel"><h2>Overdue returns</h2>${overdueTable(over)}</div>`;
  }
};
const overdueTable = rows => table([
  { label: 'Issue', key: 'issue_id', cls: 'mono' }, { label: 'Borrower', key: 'borrower' }, { label: 'Equipment', key: 'equipment_name' },
  { label: 'Outstanding', key: 'outstanding', cls: 'num' }, { label: 'Due date', key: 'due_date', cls: 'mono' },
  { label: 'Days late', html: r => pill(r.days_overdue + ' d', 'bad') }], rows, 'Nothing is overdue. 🎉');

pages.equipment = {
  title: 'Equipment',
  async render(view) {
    const admin = state.user.role === 'Admin';
    await loadLookups();
    const opt = (list) => '<option value="">All</option>' + list.map(o => `<option value="${esc(o.value)}">${esc(o.label)}</option>`).join('');
    view.innerHTML = `<div class="toolbar">
      <label class="grow">Search name or ID<input id="eq-q" placeholder="e.g. multimeter or 3"></label>
      <label>Laboratory<select id="eq-lab">${opt(labOptions())}</select></label>
      <label>Category<select id="eq-cat">${opt(catOptions())}</select></label>
      <label>Condition<select id="eq-cond">${opt(CONDITIONS)}</select></label>
      ${admin ? '<button class="btn primary" id="eq-add">+ Add equipment</button>' : ''}</div><div id="eq-table"></div>`;

    const form = (existing) => ({
      fields: [
        { name: 'equipment_name', label: 'Equipment name', required: true, max: 150 },
        { name: 'category_id', label: 'Category', type: 'select', options: catOptions(), required: true, numeric: true, half: true },
        { name: 'lab_id', label: 'Laboratory', type: 'select', options: labOptions(), required: true, numeric: true, half: true },
        { name: 'total_quantity', label: 'Total quantity', type: 'number', min: 0, required: true, half: true,
          hint: existing ? 'Available stock is recalculated automatically.' : '' },
        { name: 'condition_status', label: 'Condition', type: 'select', options: CONDITIONS, required: true, half: true },
        { name: 'purchase_date', label: 'Purchase date', type: 'date' },
        { name: 'description', label: 'Description', type: 'textarea' }]
    });
    let rows = [];
    const load = async () => {
      const box = $('#eq-table'); box.innerHTML = loading();
      const p = new URLSearchParams();
      for (const [id, key] of [['eq-q', 'q'], ['eq-lab', 'lab_id'], ['eq-cat', 'category_id'], ['eq-cond', 'condition']])
        if ($('#' + id).value.trim()) p.set(key, $('#' + id).value.trim());
      try { rows = await api('GET', '/equipment?' + p); } catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
      box.innerHTML = table([
        { label: 'ID', key: 'equipment_id', cls: 'mono' }, { label: 'Name', key: 'equipment_name' },
        { label: 'Category', key: 'category_name' }, { label: 'Lab', key: 'lab_name' },
        { label: 'Total', key: 'total_quantity', cls: 'num' },
        { label: 'Available', cls: 'num', html: r => pill(r.available_quantity, r.available_quantity === 0 ? 'bad' : r.available_quantity <= r.total_quantity * 0.2 ? 'warn' : 'good') },
        { label: 'Condition', html: r => pill(r.condition_status, r.condition_status === 'Good' ? 'good' : r.condition_status === 'Fair' ? 'warn' : 'bad') },
        { label: 'Purchased', key: 'purchase_date', cls: 'mono' },
        ...(admin ? [{ label: '', cls: 'actions', html: r => actionBtn('edit', r.equipment_id, 'Edit') + actionBtn('del', r.equipment_id, 'Delete', 'danger') }] : [])
      ], rows, 'No equipment matches your filters.');
      bindActions(box, {
        edit: id => openForm({ title: 'Edit equipment', ...form(true), values: rows.find(r => r.equipment_id === id),
          onSubmit: async d => { await api('PUT', '/equipment/' + id, d); toast('Equipment updated.'); load(); } }),
        del: id => confirmBox('Delete this equipment? This only works if it has no issue or maintenance history.',
          async () => { await api('DELETE', '/equipment/' + id); toast('Equipment deleted.'); load(); })
      });
    };
    let t; $('#eq-q').addEventListener('input', () => { clearTimeout(t); t = setTimeout(load, 300); });
    ['eq-lab', 'eq-cat', 'eq-cond'].forEach(id => $('#' + id).addEventListener('change', load));
    if (admin) $('#eq-add').onclick = () => openForm({ title: 'Add equipment', ...form(false), values: { condition_status: 'Good' },
      onSubmit: async d => { await api('POST', '/equipment', d); toast('Equipment added.'); load(); } });
    load();
  }
};

pages.issues = {
  title: 'Issue & Return',
  async render(view) {
    view.innerHTML = `<div class="toolbar">
      <label>Show<select id="is-status"><option value="outstanding">Outstanding only</option><option value="">All records</option>
        <option>Issued</option><option>Partially Returned</option><option>Returned</option></select></label>
      <button class="btn primary" id="is-new">+ Issue equipment</button></div><div id="is-table"></div>`;
    let rows = [];
    const load = async () => {
      const box = $('#is-table'); box.innerHTML = loading();
      try { rows = await api('GET', '/issues?status=' + encodeURIComponent($('#is-status').value)); }
      catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
      box.innerHTML = issuesTable(rows, true);
      bindActions(box, { ret: id => {
        const r = rows.find(x => x.issue_id === id);
        openForm({ title: `Return — ${r.equipment_name} (${r.borrower})`, submitLabel: 'Record return',
          fields: [{ name: 'quantity', label: 'Quantity returned', type: 'number', min: 1, max: r.outstanding, required: true, hint: `Outstanding: ${r.outstanding}` },
                   { name: 'remarks', label: 'Remarks', max: 300 }],
          values: { quantity: r.outstanding },
          onSubmit: async d => { await api('POST', `/issues/${id}/returns`, d); toast('Return recorded.'); load(); } });
      } });
    };
    $('#is-status').addEventListener('change', load);
    $('#is-new').onclick = async () => {
      let users, equipment;
      try { [users, equipment] = await Promise.all([api('GET', '/users'), api('GET', '/equipment')]); } catch (e) { toast(e.message, true); return; }
      const borrowers = users.filter(u => (u.role === 'Student' || u.role === 'Faculty') && u.is_active !== 0)
        .map(u => ({ value: u.user_id, label: `${u.full_name} (${u.role}${u.department ? ', ' + u.department : ''})` }));
      const items = equipment.filter(e => e.available_quantity > 0)
        .map(e => ({ value: e.equipment_id, label: `${e.equipment_name} — ${e.available_quantity} available` }));
      openForm({ title: 'Issue equipment', submitLabel: 'Issue',
        fields: [{ name: 'user_id', label: 'Student / Faculty', type: 'select', options: borrowers, required: true, numeric: true },
                 { name: 'equipment_id', label: 'Equipment', type: 'select', options: items, required: true, numeric: true },
                 { name: 'quantity', label: 'Quantity', type: 'number', min: 1, required: true, half: true },
                 { name: 'due_date', label: 'Due date', type: 'date', min: today(), required: true, half: true }],
        values: { quantity: 1, due_date: today(7) },
        onSubmit: async d => { await api('POST', '/issues', d); toast('Equipment issued.'); load(); } });
    };
    load();
  }
};
function issuesTable(rows, canReturn) {
  const status = r => r.is_overdue ? pill('Overdue', 'bad') : pill(r.status, r.status === 'Returned' ? 'gray' : r.status === 'Issued' ? 'info' : 'warn');
  return table([
    { label: 'Issue', key: 'issue_id', cls: 'mono' },
    ...(canReturn ? [{ label: 'Borrower', key: 'borrower' }] : []),
    { label: 'Equipment', key: 'equipment_name' }, { label: 'Issued', key: 'quantity_issued', cls: 'num' },
    { label: 'Outstanding', key: 'outstanding', cls: 'num' }, { label: 'Issue date', key: 'issue_date', cls: 'mono' },
    { label: 'Due date', key: 'due_date', cls: 'mono' }, { label: 'Status', html: status },
    ...(canReturn ? [{ label: '', cls: 'actions', html: r => r.outstanding > 0 ? actionBtn('ret', r.issue_id, 'Return') : '' }] : [])
  ], rows, 'No issue records found.');
}

pages.myissues = {
  title: 'My Issues',
  async render(view) {
    view.innerHTML = loading();
    const rows = await api('GET', '/issues');
    view.innerHTML = issuesTable(rows, false);
  }
};

pages.maintenance = {
  title: 'Maintenance',
  async render(view) {
    const admin = state.user.role === 'Admin';
    view.innerHTML = `<div class="toolbar">
      <label>Show<select id="m-status"><option value="open">Open (Reported / In Progress)</option><option value="">All records</option>
        <option>Reported</option><option>In Progress</option><option>Completed</option><option>Written Off</option></select></label>
      <button class="btn primary" id="m-new">+ Report damage</button></div><div id="m-table"></div>`;
    let rows = [];
    const load = async () => {
      const box = $('#m-table'); box.innerHTML = loading();
      try { rows = await api('GET', '/maintenance?status=' + encodeURIComponent($('#m-status').value)); }
      catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
      const kind = s => s === 'Completed' ? 'good' : s === 'In Progress' ? 'info' : s === 'Written Off' ? 'gray' : 'warn';
      box.innerHTML = table([
        { label: 'ID', key: 'maintenance_id', cls: 'mono' }, { label: 'Equipment', key: 'equipment_name' },
        { label: 'Qty', key: 'quantity_affected', cls: 'num' }, { label: 'Problem', key: 'problem_description' },
        { label: 'Reported by', key: 'reported_by_name' }, { label: 'Reported', key: 'reported_date', cls: 'mono' },
        { label: 'Cost (₹)', cls: 'num', html: r => esc(Number(r.repair_cost).toFixed(2)) },
        { label: 'Status', html: r => pill(r.status, kind(r.status)) },
        ...(admin ? [{ label: '', cls: 'actions', html: r => (r.status === 'Reported' || r.status === 'In Progress') ? actionBtn('upd', r.maintenance_id, 'Update') : '' }] : [])
      ], rows, 'No maintenance records.');
      bindActions(box, { upd: id => {
        const r = rows.find(x => x.maintenance_id === id);
        const options = ['In Progress', 'Completed', 'Written Off'].filter(s => s !== r.status).map(s => ({ value: s, label: s }));
        openForm({ title: `Update maintenance #${id} — ${r.equipment_name}`,
          fields: [{ name: 'status', label: 'New status', type: 'select', options, required: true,
                     hint: 'Completed returns the items to stock. Written Off removes them from total stock.' },
                   { name: 'repair_cost', label: 'Repair cost (₹)', type: 'number', min: 0, step: '0.01' },
                   { name: 'maintenance_date', label: 'Maintenance date (for In Progress)', type: 'date' }],
          onSubmit: async d => { await api('PUT', '/maintenance/' + id, d); toast('Maintenance updated.'); load(); } });
      } });
    };
    $('#m-status').addEventListener('change', load);
    $('#m-new').onclick = async () => {
      let equipment; try { equipment = await api('GET', '/equipment'); } catch (e) { toast(e.message, true); return; }
      openForm({ title: 'Report damaged equipment', submitLabel: 'Report',
        fields: [{ name: 'equipment_id', label: 'Equipment', type: 'select', required: true, numeric: true,
                   options: equipment.filter(e => e.available_quantity > 0).map(e => ({ value: e.equipment_id, label: `${e.equipment_name} — ${e.available_quantity} available` })) },
                 { name: 'quantity_affected', label: 'Number of damaged units', type: 'number', min: 1, required: true },
                 { name: 'problem_description', label: 'Problem', type: 'textarea', required: true }],
        values: { quantity_affected: 1 },
        onSubmit: async d => { await api('POST', '/maintenance', d); toast('Damage reported. Units removed from available stock.'); load(); } });
    };
    load();
  }
};

pages.reports = {
  title: 'Reports',
  async render(view) {
    const admin = state.user.role === 'Admin';
    const defs = {
      inventory: { label: 'Available inventory', path: '/reports/inventory', cols: [
        { label: 'ID', key: 'equipment_id', cls: 'mono' }, { label: 'Equipment', key: 'equipment_name' }, { label: 'Category', key: 'category_name' },
        { label: 'Lab', key: 'lab_name' }, { label: 'Total', key: 'total_quantity', cls: 'num' }, { label: 'Available', key: 'available_quantity', cls: 'num' },
        { label: 'Condition', key: 'condition_status' }] },
      issued: { label: 'Currently issued equipment', path: '/reports/issued', cols: [
        { label: 'Issue', key: 'issue_id', cls: 'mono' }, { label: 'Borrower', key: 'borrower' }, { label: 'Dept', key: 'department' },
        { label: 'Equipment', key: 'equipment_name' }, { label: 'Lab', key: 'lab_name' }, { label: 'Outstanding', key: 'outstanding', cls: 'num' },
        { label: 'Issued on', key: 'issue_date', cls: 'mono' }, { label: 'Due', key: 'due_date', cls: 'mono' }] },
      overdue: { label: 'Overdue returns', path: '/reports/overdue', cols: [
        { label: 'Issue', key: 'issue_id', cls: 'mono' }, { label: 'Borrower', key: 'borrower' }, { label: 'Equipment', key: 'equipment_name' },
        { label: 'Lab', key: 'lab_name' }, { label: 'Outstanding', key: 'outstanding', cls: 'num' }, { label: 'Due', key: 'due_date', cls: 'mono' },
        { label: 'Days late', html: r => pill(r.days_overdue + ' d', 'bad') }] },
      ...(admin ? {
        maintenance: { label: 'Equipment under maintenance', path: '/reports/maintenance', cols: [
          { label: 'ID', key: 'maintenance_id', cls: 'mono' }, { label: 'Equipment', key: 'equipment_name' }, { label: 'Lab', key: 'lab_name' },
          { label: 'Qty', key: 'quantity_affected', cls: 'num' }, { label: 'Problem', key: 'problem_description' },
          { label: 'Reported', key: 'reported_date', cls: 'mono' }, { label: 'Status', key: 'status' }] },
        bylab: { label: 'Equipment by laboratory', path: '/reports/by-lab', cols: [
          { label: 'Laboratory', key: 'lab_name' }, { label: 'Equipment types', key: 'equipment_types', cls: 'num' },
          { label: 'Total quantity', key: 'total_quantity', cls: 'num' }, { label: 'Available', key: 'available_quantity', cls: 'num' }] } } : {}),
      history: { label: 'Issue history by user', path: null, cols: [
        { label: 'Issue', key: 'issue_id', cls: 'mono' }, { label: 'Equipment', key: 'equipment_name' }, { label: 'Issued', key: 'quantity_issued', cls: 'num' },
        { label: 'Outstanding', key: 'outstanding', cls: 'num' }, { label: 'Issue date', key: 'issue_date', cls: 'mono' }, { label: 'Due', key: 'due_date', cls: 'mono' },
        { label: 'Status', html: r => r.is_overdue ? pill('Overdue', 'bad') : esc(r.status) }] }
    };
    view.innerHTML = `<div class="toolbar"><label>Report<select id="rp-type">${Object.entries(defs).map(([k, d]) => `<option value="${k}">${esc(d.label)}</option>`).join('')}</select></label>
      <label id="rp-user-wrap" class="hidden">User<select id="rp-user"></select></label></div><div id="rp-out"></div>`;
    let users = [];
    try { users = await api('GET', '/users'); } catch { /* not needed for other reports */ }
    $('#rp-user').innerHTML = users.map(u => `<option value="${esc(u.user_id)}">${esc(u.full_name)}</option>`).join('');
    const load = async () => {
      const key = $('#rp-type').value, def = defs[key], out = $('#rp-out');
      $('#rp-user-wrap').classList.toggle('hidden', key !== 'history');
      out.innerHTML = loading();
      try {
        const path = def.path || '/reports/user-history/' + encodeURIComponent($('#rp-user').value);
        out.innerHTML = table(def.cols, await api('GET', path), 'No data for this report.');
      } catch (e) { out.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
    };
    $('#rp-type').addEventListener('change', load); $('#rp-user').addEventListener('change', load);
    load();
  }
};

// Labs and Categories share one small generic page
function lookupPage(title, path, idKey, nameKey, nameLabel, hasLocation, label) {
  return {
    title,
    async render(view) {
      view.innerHTML = `<div class="toolbar"><button class="btn primary" id="lk-add">+ Add ${esc(label)}</button></div><div id="lk-table"></div>`;
      let rows = [];
      const fields = [{ name: nameKey, label: nameLabel, required: true, max: 100 },
        ...(hasLocation ? [{ name: 'location', label: 'Location', max: 150 }] : []),
        { name: 'description', label: 'Description', type: 'textarea' }];
      const load = async () => {
        const box = $('#lk-table'); box.innerHTML = loading();
        rows = await api('GET', path);
        box.innerHTML = table([{ label: 'ID', key: idKey, cls: 'mono' }, { label: nameLabel, key: nameKey },
          ...(hasLocation ? [{ label: 'Location', key: 'location' }] : []), { label: 'Description', key: 'description' },
          { label: '', cls: 'actions', html: r => actionBtn('edit', r[idKey], 'Edit') + actionBtn('del', r[idKey], 'Delete', 'danger') }], rows);
        bindActions(box, {
          edit: id => openForm({ title: 'Edit ' + label, fields, values: rows.find(r => r[idKey] === id),
            onSubmit: async d => { await api('PUT', `${path}/${id}`, d); toast(label + ' updated.'); load(); } }),
          del: id => confirmBox(`Delete this ${label}? It cannot be deleted while equipment uses it.`,
            async () => { await api('DELETE', `${path}/${id}`); toast(label + ' deleted.'); load(); })
        });
      };
      $('#lk-add').onclick = () => openForm({ title: 'Add ' + label, fields,
        onSubmit: async d => { await api('POST', path, d); toast(label + ' added.'); load(); } });
      load();
    }
  };
}
pages.labs = lookupPage('Laboratories', '/labs', 'lab_id', 'lab_name', 'Lab name', true, 'laboratory');
pages.categories = lookupPage('Categories', '/categories', 'category_id', 'category_name', 'Category name', false, 'category');

pages.users = {
  title: 'Users',
  async render(view) {
    view.innerHTML = `<div class="toolbar"><button class="btn primary" id="u-add">+ Add user</button></div><div id="u-table"></div>`;
    let rows = [];
    const roles = ['Admin', 'LabAssistant', 'Student', 'Faculty'].map(r => ({ value: r, label: r }));
    const fields = editing => [
      { name: 'full_name', label: 'Full name', required: true, max: 100 },
      { name: 'username', label: 'Username', required: true, half: true, hint: 'Letters, digits, . _ -' },
      { name: 'role', label: 'Role', type: 'select', options: roles, required: true, half: true },
      { name: 'department', label: 'Department', half: true }, { name: 'phone', label: 'Phone', half: true },
      { name: 'email', label: 'Email', type: 'email' },
      ...(editing ? [{ name: 'is_active', label: 'Account status', type: 'select', required: true, numeric: true,
                        options: [{ value: 1, label: 'Active' }, { value: 0, label: 'Inactive' }] }] : []),
      { name: 'password', label: editing ? 'New password (leave blank to keep)' : 'Password', type: 'password', hint: 'At least 8 characters. Without a password the user cannot log in.' }];
    const load = async () => {
      const box = $('#u-table'); box.innerHTML = loading();
      rows = await api('GET', '/users');
      box.innerHTML = table([{ label: 'ID', key: 'user_id', cls: 'mono' }, { label: 'Name', key: 'full_name' }, { label: 'Username', key: 'username', cls: 'mono' },
        { label: 'Role', key: 'role' }, { label: 'Department', key: 'department' }, { label: 'Contact', html: r => esc([r.email, r.phone].filter(Boolean).join(' · ')) },
        { label: 'Login', html: r => r.has_password ? pill('Enabled', 'good') : pill('No password', 'gray') },
        { label: 'Status', html: r => r.is_active ? pill('Active', 'good') : pill('Inactive', 'bad') },
        { label: '', cls: 'actions', html: r => actionBtn('edit', r.user_id, 'Edit') + actionBtn('del', r.user_id, 'Delete', 'danger') }], rows);
      bindActions(box, {
        edit: id => openForm({ title: 'Edit user', fields: fields(true), values: rows.find(r => r.user_id === id),
          onSubmit: async d => { await api('PUT', '/users/' + id, d); toast('User updated.'); load(); } }),
        del: id => confirmBox('Delete this user? Users with issue history cannot be deleted — deactivate them instead.',
          async () => { await api('DELETE', '/users/' + id); toast('User deleted.'); load(); })
      });
    };
    $('#u-add').onclick = () => openForm({ title: 'Add user', fields: fields(false), values: { role: 'Student' },
      onSubmit: async d => { await api('POST', '/users', d); toast('User added.'); load(); } });
    load();
  }
};

/* ---------- navigation / app shell ---------- */
const NAV = {
  Admin: ['dashboard', 'equipment', 'issues', 'maintenance', 'reports', 'labs', 'categories', 'users'],
  LabAssistant: ['dashboard', 'equipment', 'issues', 'maintenance', 'reports'],
  Student: ['equipment', 'myissues'],
  Faculty: ['equipment', 'myissues']
};
async function showPage() {
  const allowed = NAV[state.user.role];
  let name = location.hash.replace('#/', '');
  if (!allowed.includes(name)) name = allowed[0];
  const page = pages[name];
  $('#page-title').textContent = page.title;
  document.querySelectorAll('#nav a').forEach(a => a.classList.toggle('active', a.dataset.page === name));
  $('#sidebar').classList.remove('open');
  const view = $('#view');
  try { await page.render(view); }
  catch (e) { if (state.user) view.innerHTML = `<div class="panel"><div class="form-error">${esc(e.message)}</div></div>`; }
}
function showApp() {
  $('#login-screen').classList.add('hidden'); $('#app').classList.remove('hidden');
  $('#user-name').textContent = state.user.full_name; $('#user-role').textContent = state.user.role;
  $('#nav').innerHTML = NAV[state.user.role].map(n => `<a href="#/${n}" data-page="${n}">${esc(pages[n].title)}</a>`).join('');
  showPage();
}
function showLogin(msg) {
  state.user = null;
  currentSessionToken = '';
  localStorage.removeItem('lab_session_token');
  $('#app').classList.add('hidden'); $('#login-screen').classList.remove('hidden');
  $('#login-error').textContent = msg || '';
  if ($('#dlg').open) $('#dlg').close();
}

$('#login-form').addEventListener('submit', async e => {
  e.preventDefault();
  const f = e.target, btn = f.querySelector('button'); btn.disabled = true; $('#login-error').textContent = '';
  try {
    state.user = await api('POST', '/auth/login', { username: f.elements.username.value, password: f.elements.password.value });
    if (state.user && state.user.token) {
      currentSessionToken = state.user.token;
      localStorage.setItem('lab_session_token', state.user.token);
    }
    f.elements.password.value = ''; location.hash = ''; showApp();
  } catch (err) { $('#login-error').textContent = err.message; }
  finally { btn.disabled = false; }
});
$('#logout').addEventListener('click', async () => {
  try { await api('POST', '/auth/logout'); } catch { /* ignore */ }
  currentSessionToken = '';
  localStorage.removeItem('lab_session_token');
  showLogin('');
});
$('#menu-btn').addEventListener('click', () => $('#sidebar').classList.toggle('open'));
window.addEventListener('hashchange', () => { if (state.user) showPage(); });

(async function start() {
  try { state.user = await api('GET', '/auth/me'); showApp(); } catch { showLogin(''); }
})();
