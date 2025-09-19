#pragma once

static const char PROGMEM schedule_html[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Weekly Schedule (CRUD)</title>
  <style>
    body{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:#0f172a;color:#e5e7eb;padding-top:88px}
    .header{position:fixed;top:0;left:0;right:0;z-index:100;background:linear-gradient(135deg,#1e3a8a 0%,#3b82f6 100%);color:#fff;padding:16px 24px;box-shadow:0 2px 10px rgba(0,0,0,0.3)}
    .header-top{display:flex;align-items:center;justify-content:space-between;gap:16px}
    .header h1{margin:0;font-size:22px}
    .header-info{text-align:right}
    .header-time{font-size:18px;font-weight:600;letter-spacing:0.5px}
    .header-date{font-size:14px;color:#dbeafe;margin-top:4px}
    .nav-links{margin-top:12px;display:flex;gap:16px;flex-wrap:wrap}
    .nav-links a{color:#dbeafe;text-decoration:none;padding:6px 12px;border-radius:8px;font-size:14px;transition:background .2s,color .2s}
    .nav-links a:hover{background:rgba(255,255,255,0.15)}
    .nav-links a.active{background:#00d4ff;color:#0f172a;font-weight:600}
    .container{max-width:980px;margin:0 auto;padding:16px}
    h1{margin:8px 0 16px 0;font-size:22px}
    .card{background:#111827;border:1px solid #1f2937;border-radius:10px;padding:16px}
    .row-active{background:rgba(37,99,235,0.12)}
    .row-active td{border-bottom-color:#2563eb}
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
    .active-now{background:#0f766e;color:#a7f3d0}
    .disabled{background:#3f3f46;color:#d4d4d8}
    .mode-toggle{display:flex;align-items:center;justify-content:space-between;background:#0b1220;border:1px solid #1f2937;border-radius:10px;padding:12px;margin-bottom:16px}
    .mode-toggle-left{display:flex;align-items:center;gap:12px}
    .mode-toggle-title{font-size:16px;font-weight:600;color:#e5e7eb}
    .mode-toggle-sub{font-size:13px;color:#9ca3af}
    .toggle-switch{position:relative;width:50px;height:26px}
    .toggle-switch input{opacity:0;width:0;height:0}
    .toggle-slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#374151;border-radius:999px;transition:all .2s}
    .toggle-slider:before{position:absolute;content:"";height:20px;width:20px;left:3px;bottom:3px;background:white;border-radius:50%;transition:all .2s}
    .toggle-switch input:checked + .toggle-slider{background:#2563eb}
    .toggle-switch input:checked + .toggle-slider:before{transform:translateX(24px)}
    .mode-toggle-status{font-size:14px;font-weight:600;color:#e5e7eb;margin-left:12px}
    .mode-toggle-status.schedule{color:#38bdf8}
    .mode-toggle-status.direct{color:#facc15}
  </style>
</head>
<body>
  <div class="header">
    <div class="header-top">
      <h1>Schedule Manager</h1>
      <div class="header-info">
        <div class="header-time" id="headerTime">--:--:--</div>
        <div class="header-date" id="headerDate">--</div>
      </div>
    </div>
    <div class="nav-links">
      <a href="/">Dashboard</a>
      <a href="/system">System</a>
      <a href="/relays">Relays</a>
      <a href="/temperature">Temperature</a>
      <a href="/schedule" class="active">Schedule</a>
      <a href="/sensors">Sensors</a>
      <a href="/wifi-config">Network</a>
    </div>
  </div>

  <div class="container">
    <h1>Weekly Schedule (CRUD)</h1>
    <div class="card">
      <div class="mode-toggle">
        <div class="mode-toggle-left">
          <div>
            <div class="mode-toggle-title">Setpoint Source</div>
            <div class="mode-toggle-sub">Choose between manual setpoint and scheduled automation</div>
          </div>
        </div>
        <div style="display:flex;align-items:center;gap:12px">
          <label class="toggle-switch">
            <input type="checkbox" id="setpointToggle" />
            <span class="toggle-slider"></span>
          </label>
          <span class="mode-toggle-status direct" id="setpointStatus">Direct</span>
        </div>
      </div>
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
        <button id="saveBtn" class="btn">Save Schedule</button>
        <button id="updateBtn" class="btn secondary">Update Schedule</button>
        <button id="clearBtn" class="btn danger">Clear Schedule</button>
        <button id="refreshBtn" class="btn secondary">Refresh View</button>
      </div>
    </div>
  </div>

<script>
  const API = '/api/schedule';
  const dayNames = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  let scheduleEnabled = true;
  let zoneEnabled = true;
  let setpointMode = 0; // 0=Direct, 1=Schedule
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
      tr.dataset.day = row.day;
      // Day label
      tr.appendChild(el('td', {}, [document.createTextNode(dayNames[row.day])]))
      // Enabled
      const chk = el('input', {type:'checkbox'});
      chk.checked = !!row.enabled;
      chk.addEventListener('change', ()=>{ row.enabled = chk.checked; updateStatus(tr, row); updateActiveIndicator(); });
      tr.appendChild(el('td', {}, [chk]));
      // Start Time
      const tim = el('input', {type:'time', value: validTime(row.time) ? row.time : ''});
      tim.addEventListener('input', ()=>{ row.time = tim.value; updateActiveIndicator(); });
      tr.appendChild(el('td', {}, [tim]));
      // End Time
      const endTim = el('input', {type:'time', value: validTime(row.end_time) ? row.end_time : ''});
      endTim.addEventListener('input', ()=>{ row.end_time = endTim.value; updateActiveIndicator(); });
      tr.appendChild(el('td', {}, [endTim]));
      // Temp
      const inp = el('input', {type:'number', step:'0.5', min:'10', max:'40', value: row.temp});
      inp.addEventListener('input', ()=>{ row.temp = parseFloat(inp.value||'0'); });
      tr.appendChild(el('td', {}, [inp]));
      // Status
      const statusLabel = row.enabled ? 'Enabled' : 'Disabled';
      const st = el('span', {class:'status-badge ' + (row.enabled?'enabled':'disabled')}, [document.createTextNode(statusLabel)]);
      st.dataset.role = 'status';
      st.dataset.enabledLabel = statusLabel;
      st.dataset.disabledLabel = 'Disabled';
      tr.appendChild(el('td', {}, [st]));
      body.appendChild(tr);
    });
  }

  function updateStatus(tr, row){
    const st = tr.querySelector('span[data-role="status"]');
    if (!st) return;
    const label = row.enabled ? 'Enabled' : 'Disabled';
    st.dataset.enabledLabel = 'Enabled';
    st.dataset.disabledLabel = 'Disabled';
    st.className = 'status-badge ' + (row.enabled?'enabled':'disabled');
    st.textContent = label;
  }

  function validTime(t){ return /^\d{2}:\d{2}$/.test(t); }

  function timeToMinutes(t){
    if (!validTime(t)) return null;
    const [h, m] = t.split(':').map(Number);
    return h * 60 + m;
  }

  function isRowActiveNow(row, currentDay, currentMinutes){
    if (!row.enabled) return false;
    const start = timeToMinutes(row.time);
    const endRaw = timeToMinutes(row.end_time);
    if (start === null) return false;
    const end = endRaw === null ? ((start + 60) % 1440) : endRaw;

    if (start === end) {
      return row.day === currentDay && currentMinutes === start;
    }

    if (start < end) {
      return row.day === currentDay && currentMinutes >= start && currentMinutes < end;
    }

    // Event crosses midnight
    if (row.day === currentDay && currentMinutes >= start) return true;
    const nextDay = (row.day + 1) % 7;
    if (nextDay === currentDay && currentMinutes < end) return true;
    return false;
  }

  function updateActiveIndicator(){
    const now = new Date();
    const currentDay = now.getDay();
    const currentMinutes = now.getHours() * 60 + now.getMinutes();
    const canActivate = scheduleEnabled && zoneEnabled && setpointMode === 1;
    const rows = document.querySelectorAll('#scheduleBody tr');
    rows.forEach((tr, idx) => {
      const rowModel = model[idx];
      if (!rowModel) return;
      const active = canActivate && isRowActiveNow(rowModel, currentDay, currentMinutes);
      tr.classList.toggle('row-active', active);
      const st = tr.querySelector('span[data-role="status"]');
      if (!st) return;
      if (active) {
        st.className = 'status-badge active-now';
        st.textContent = 'Active Now';
      } else {
        if (!scheduleEnabled) {
          st.className = 'status-badge disabled';
          st.textContent = 'Schedule Off';
        } else if (!zoneEnabled) {
          st.className = 'status-badge disabled';
          st.textContent = 'Zone Off';
        } else if (setpointMode !== 1) {
          st.className = 'status-badge disabled';
          st.textContent = 'Direct Mode';
        } else {
          const enabled = !!rowModel.enabled;
          st.className = 'status-badge ' + (enabled ? 'enabled' : 'disabled');
          st.textContent = enabled ? 'Enabled' : 'Disabled';
        }
      }
    });
  }

  function updateModeUI(){
    const toggle = document.getElementById('setpointToggle');
    const status = document.getElementById('setpointStatus');
    if (!toggle || !status) return;
    toggle.checked = setpointMode === 1;
    const usingSchedule = setpointMode === 1 && scheduleEnabled && zoneEnabled;
    const label = setpointMode === 1 ? (usingSchedule ? 'Schedule' : 'Schedule (inactive)') : 'Direct';
    status.textContent = label;
    status.className = 'mode-toggle-status ' + (usingSchedule ? 'schedule' : 'direct');
  }

  async function setSetpointMode(mode){
    const desired = mode === 1 ? 1 : 0;
    const toggle = document.getElementById('setpointToggle');
    if (toggle) toggle.disabled = true;
    try{
      const res = await fetch('/api/schedule/setpoint-mode', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body: JSON.stringify({ mode: desired === 1 ? 'schedule' : 'direct' })
      });
      if(!res.ok) throw new Error('Request failed (' + res.status + ')');
      const data = await res.json();
      if (data && data.operation_mode !== undefined) {
        setpointMode = Number(data.operation_mode) === 1 ? 1 : 0;
      } else {
        setpointMode = desired;
      }
      updateModeUI();
      updateActiveIndicator();
    }catch(err){
      console.error(err);
      alert('Unable to change setpoint mode: ' + err.message);
      updateModeUI();
    } finally {
      if (toggle) toggle.disabled = false;
    }
  }

  // Map API -> UI model (first event per day)
  function applyFromApi(data){
    scheduleEnabled = !!(data && data.global_enabled);
    zoneEnabled = !!(data && (data.zone_enabled === undefined ? true : data.zone_enabled));
    if (data && data.setpoint_mode !== undefined) {
      setpointMode = Number(data.setpoint_mode) === 1 ? 1 : 0;
    }
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
      updateModeUI();
      updateActiveIndicator();
    }catch(e){ console.error(e); }
    finally {
      const toggle = document.getElementById('setpointToggle');
      if (toggle) toggle.disabled = false;
    }
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

  function updateHeaderClock(){
    const elTime = document.getElementById('headerTime');
    const elDate = document.getElementById('headerDate');
    if (!elTime || !elDate) return;
    const now = new Date();
    elTime.textContent = now.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit', second:'2-digit'});
    elDate.textContent = now.toLocaleDateString([], {weekday:'short', year:'numeric', month:'short', day:'numeric'});
  }

  document.addEventListener('DOMContentLoaded', ()=>{
    renderTable();
    updateModeUI();
    load();
    const toggle = document.getElementById('setpointToggle');
    if (toggle) {
      toggle.disabled = true;
      toggle.addEventListener('change', ()=>{
        const targetMode = toggle.checked ? 1 : 0;
        if (targetMode !== setpointMode) {
          setSetpointMode(targetMode);
        }
      });
    }
    document.getElementById('saveBtn').addEventListener('click', ()=> save('POST'));
    document.getElementById('updateBtn').addEventListener('click', ()=> save('PUT'));
    document.getElementById('clearBtn').addEventListener('click', clearAll);
    document.getElementById('refreshBtn').addEventListener('click', load);
    updateHeaderClock();
    setInterval(updateHeaderClock, 1000);
    updateActiveIndicator();
    setInterval(updateActiveIndicator, 30000);
  });
</script>
</body>
</html>
)HTML";
