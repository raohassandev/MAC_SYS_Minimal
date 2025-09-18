#pragma once

static const char PROGMEM schedule_html[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Weekly Schedule (CRUD)</title>
  <style>
    body{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:#0f172a;color:#e5e7eb}
    .container{max-width:980px;margin:0 auto;padding:16px}
    h1{margin:8px 0 16px 0;font-size:22px}
    .card{background:#111827;border:1px solid #1f2937;border-radius:10px;padding:16px}
    table{width:100%;border-collapse:collapse;margin-top:8px}
    thead th{font-weight:600;color:#9ca3af;border-bottom:1px solid #1f2937;padding:10px;text-align:left}
    tbody td{border-bottom:1px solid #1f2937;padding:10px}
    .row{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
    .btn{background:#2563eb;border:none;color:#fff;padding:8px 14px;border-radius:8px;cursor:pointer}
    .btn.secondary{background:#374151}
    .btn.danger{background:#b91c1c}
    input[type="time"], input[type="number"]{background:#0b1220;color:#e5e7eb;border:1px solid #1f2937;border-radius:6px;padding:6px}
    .status-badge{display:inline-block;padding:4px 8px;border-radius:9999px;font-size:12px}
    .enabled{background:#064e3b;color:#a7f3d0}
    .disabled{background:#3f3f46;color:#d4d4d8}
  </style>
</head>
<body>
  <div class="container">
    <h1>Weekly Schedule (CRUD)</h1>
    <div class="card">
      <table id="scheduleTable">
        <thead>
          <tr>
            <th>Day</th>
            <th>Enabled</th>
            <th>Start Time</th>
            <th>End Time</th>
            <th>Temperature (°C)</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody id="scheduleBody"></tbody>
      </table>
      <div class="row">
        <button id="saveBtn" class="btn">Save (POST)</button>
        <button id="updateBtn" class="btn secondary">Update (PUT)</button>
        <button id="clearBtn" class="btn danger">Clear (DELETE)</button>
        <button id="refreshBtn" class="btn secondary">Refresh (GET)</button>
      </div>
    </div>
  </div>

<script>
  const API = '/api/schedule';
  const dayNames = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  // UI model: one event per day (single setpoint, start/end time, enabled)
  const model = Array.from({length:7}, (_,d)=>({day:d, enabled:false, time:'--:--', end_time:'--:--', temp:22}));

  function el(tag, attrs={}, children=[]) {
    const e = document.createElement(tag);
    Object.entries(attrs).forEach(([k,v])=>{
      if(k==='class') e.className = v; else if(k==='html') e.innerHTML=v; else e.setAttribute(k,v);
    });
    children.forEach(c=> e.appendChild(c));
    return e;
  }

  function renderTable() {
    const body = document.getElementById('scheduleBody');
    body.innerHTML = '';
    model.forEach(row => {
      const tr = document.createElement('tr');
      // Day label
      tr.appendChild(el('td', {}, [document.createTextNode(dayNames[row.day])]))
      // Enabled
      const chk = el('input', {type:'checkbox'});
      chk.checked = !!row.enabled;
      chk.addEventListener('change', ()=>{ row.enabled = chk.checked; updateStatus(tr, row); });
      tr.appendChild(el('td', {}, [chk]));
      // Start Time
      const tim = el('input', {type:'time', value: validTime(row.time) ? row.time : ''});
      tim.addEventListener('input', ()=>{ row.time = tim.value; });
      tr.appendChild(el('td', {}, [tim]));
      // End Time
      const endTim = el('input', {type:'time', value: validTime(row.end_time) ? row.end_time : ''});
      endTim.addEventListener('input', ()=>{ row.end_time = endTim.value; });
      tr.appendChild(el('td', {}, [endTim]));
      // Temp
      const inp = el('input', {type:'number', step:'0.5', min:'10', max:'40', value: row.temp});
      inp.addEventListener('input', ()=>{ row.temp = parseFloat(inp.value||'0') });
      tr.appendChild(el('td', {}, [inp]));
      // Status
      const st = el('span', {class:'status-badge ' + (row.enabled?'enabled':'disabled')}, [document.createTextNode(row.enabled?'Enabled':'Disabled')]);
      st.dataset.role = 'status';
      tr.appendChild(el('td', {}, [st]));
      body.appendChild(tr);
    });
  }

  function updateStatus(tr, row){
    const st = tr.querySelector('span[data-role="status"]');
    if (!st) return;
    st.className = 'status-badge ' + (row.enabled?'enabled':'disabled');
    st.textContent = row.enabled ? 'Enabled' : 'Disabled';
  }

  function validTime(t){ return /^\d{2}:\d{2}$/.test(t); }

  // Map API -> UI model (first event per day)
  function applyFromApi(data){
    // reset
    for(let d=0; d<7; d++){ model[d] = {day:d, enabled:false, time:'--:--', end_time:'--:--', temp:22}; }
    if (!data || !Array.isArray(data.events)) return;
    data.events.forEach(ev => {
      for (let d=0; d<7; d++){
        if ((ev.days & (1<<d)) !== 0){
          model[d].enabled = !!ev.enabled;
          model[d].time = ev.time || '--:--';
          model[d].end_time = ev.end_time || '--:--';
          model[d].temp = (ev.setpoint !== undefined) ? ev.setpoint : 22;
        }
      }
    });
  }

  // Map UI model -> API events (one event per enabled day)
  function buildPayload(){
    const events = [];
    for(let d=0; d<7; d++){
      const r = model[d];
      if (!r.enabled || !validTime(r.time)) continue;
      events.push({
        enabled: true,
        time: r.time,
        end_time: validTime(r.end_time) ? r.end_time : '23:59',
        setpoint: r.temp,
        delta: 1.0,
        days: (1<<d),
        description: dayNames[d]
      });
    }
    return events;
  }

  async function load(){
    try{
      const res = await fetch(API);
      if (!res.ok) throw new Error('GET failed');
      const data = await res.json();
      applyFromApi(data);
      renderTable();
    }catch(e){ console.error(e); }
  }

  async function save(method){
    try{
      const payload = buildPayload();
      const res = await fetch(API, {method, headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload)});
      if (!res.ok) throw new Error(method+' failed');
      await load();
      alert('Schedule '+(method==='POST'?'saved':'updated'));
    }catch(e){ console.error(e); alert('Error: '+e.message); }
  }

  async function clearAll(){
    try{
      const res = await fetch(API, {method:'DELETE'});
      if (!res.ok) throw new Error('DELETE failed');
      await load();
      alert('Schedule cleared');
    }catch(e){ console.error(e); alert('Error: '+e.message); }
  }

  document.addEventListener('DOMContentLoaded', ()=>{
    renderTable();
    load();
    document.getElementById('saveBtn').addEventListener('click', ()=> save('POST'));
    document.getElementById('updateBtn').addEventListener('click', ()=> save('PUT'));
    document.getElementById('clearBtn').addEventListener('click', clearAll);
    document.getElementById('refreshBtn').addEventListener('click', load);
  });
</script>
</body>
</html>
)HTML";
