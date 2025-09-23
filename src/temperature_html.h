#ifndef TEMPERATURE_HTML_H
#define TEMPERATURE_HTML_H

const char temperature_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Temperature Control - MAC-SYS Industrial Controller</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #0B1426;
            color: #E5E7EB;
            line-height: 1.6;
            padding-top: 6.5rem;
        }
        
        .header {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            z-index: 1000;
            background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%);
            color: white;
            padding: 1rem 2rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        }
        
        .header-top {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 1.5rem;
        }
        
        .header h1 {
            font-size: 1.5rem;
            margin: 0;
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }
        
        .header-info {
            text-align: right;
        }
        
        .header-time {
            font-size: 1.15rem;
            font-weight: 600;
            letter-spacing: 0.5px;
        }
        
        .header-date {
            font-size: 0.85rem;
            color: #dbeafe;
            margin-top: 0.2rem;
        }
        
        .nav-links {
            margin-top: 0.75rem;
            display: flex;
            gap: 1.5rem;
            flex-wrap: wrap;
        }
        
        .nav-links a {
            color: #dbeafe;
            text-decoration: none;
            padding: 0.375rem 0.75rem;
            border-radius: 0.375rem;
            transition: all 0.2s;
            font-size: 0.875rem;
        }
        
        .nav-links a:hover {
            background: rgba(255,255,255,0.2);
        }

        .nav-links a.active {
            background: #00D4FF;
            color: #0B1426;
            font-weight: 600;
        }

        .setpoint-source {
            margin-top: 1rem;
            padding: 1rem;
            background: #1f2937;
            border: 1px solid #374151;
            border-radius: 0.75rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 1rem;
        }

        .setpoint-info {
            display: flex;
            flex-direction: column;
            gap: 0.35rem;
        }

        .setpoint-title {
            font-size: 1rem;
            font-weight: 600;
            color: #E5E7EB;
        }

        .setpoint-subtitle {
            font-size: 0.85rem;
            color: #9CA3AF;
        }

        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 52px;
            height: 28px;
        }

        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #374151;
            transition: .2s;
            border-radius: 34px;
        }

        .toggle-slider:before {
            position: absolute;
            content: "";
            height: 22px;
            width: 22px;
            left: 3px;
            bottom: 3px;
            background-color: white;
            transition: .2s;
            border-radius: 50%;
        }

        .toggle-switch input:checked + .toggle-slider {
            background-color: #2563EB;
        }

        .toggle-switch input:checked + .toggle-slider:before {
            transform: translateX(24px);
        }

        .setpoint-status {
            font-size: 0.9rem;
            font-weight: 600;
            color: #E5E7EB;
        }

        .setpoint-status.schedule { color: #38bdf8; }
        .setpoint-status.direct { color: #facc15; }

        .container {
            max-width: 1400px;
            margin: 1rem auto;
            padding: 0 1rem;
        }

        @media (max-width: 768px) {
            body {
                padding-top: 140px;
            }

            .header {
                padding: 1rem;
            }

            .header h1 {
                font-size: 1.2rem;
            }

            .nav-links {
                gap: 0.75rem;
            }

            .nav-links a {
                font-size: 0.75rem;
                padding: 0.35rem 0.6rem;
            }

            .container {
                padding: 0 0.75rem;
            }

            .main-grid {
                grid-template-columns: 1fr;
            }

            .control-group {
                margin-bottom: 0.75rem;
            }

            .setpoint-source {
                flex-direction: column;
                align-items: flex-start;
                gap: 0.75rem;
            }

            .setpoint-info {
                width: 100%;
            }

            .setpoint-status {
                font-size: 0.8rem;
            }

            .state-indicators {
                justify-content: space-around;
            }

            .stats-grid {
                grid-template-columns: 1fr;
            }

            .emergency-section {
                padding: 1rem;
            }

            .emergency-info {
                grid-template-columns: 1fr;
            }

            .control-actions {
                flex-direction: column;
                gap: 0.5rem;
            }

            .control-actions .mode-btn,
            .emergency-btn {
                width: 100%;
            }
        }
        
        .temp-card {
            background: linear-gradient(135deg, #1f2937 0%, #374151 100%);
            border-radius: 0.75rem;
            padding: 1.5rem;
            border: 1px solid #374151;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
            margin-bottom: 1.5rem;
        }
        
        .card-header {
            color: #00D4FF;
            font-size: 1.125rem;
            font-weight: 600;
            margin-bottom: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .location-edit {
            display: inline-flex;
            align-items: center;
            gap: 10px;
        }
        
        .location-edit input {
            padding: 8px 12px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
        }
        
        .btn-save {
            padding: 8px 16px;
            background: #667eea;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-size: 14px;
        }
        
        .btn-save:hover {
            background: #5a67d8;
        }
        
        .status-bar {
            display: flex;
            gap: 15px;
            margin-top: 20px;
        }
        
        .status-item {
            padding: 10px 15px;
            background: #374151;
            border-radius: 8px;
            font-size: 14px;
            color: #E5E7EB;
        }
        
        .status-item.active {
            background: #48bb78;
            color: white;
        }
        
        .status-item.inactive {
            background: #fc8181;
            color: white;
        }
        
        .status-item.warning {
            background: #f6ad55;
            color: white;
        }
        
        .main-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr 1fr;
            gap: 1rem;
            align-items: start;
        }
        
        @media (max-width: 1200px) {
            .main-grid {
                grid-template-columns: 1fr 1fr;
            }
        }
        @media (max-width: 768px) {
            .main-grid {
                grid-template-columns: 1fr;
            }
        }
        
        .card {
            background: linear-gradient(135deg, #1f2937 0%, #374151 100%);
            border-radius: 0.5rem;
            padding: 1rem;
            box-shadow: 0 2px 4px rgba(0,0,0,0.3);
            border: 1px solid #374151;
            height: fit-content;
        }
        
        .card h2 {
            color: #00D4FF;
            font-size: 1rem;
            font-weight: 600;
            margin-bottom: 0.75rem;
            padding-bottom: 0.375rem;
            border-bottom: 1px solid #4B5563;
        }
        
        .temperature-display {
            text-align: center;
            padding: 0.75rem;
        }
        
        .temp-current {
            font-size: 2rem;
            font-weight: bold;
            color: #00D4FF;
            line-height: 1;
        }
        
        .temp-unit {
            font-size: 1.25rem;
            color: #9CA3AF;
        }
        
        .temp-compensated {
            font-size: 0.875rem;
            color: #9CA3AF;
            margin-top: 0.5rem;
        }
        
        .temp-setpoint {
            font-size: 1rem;
            color: #E5E7EB;
            margin-top: 0.5rem;
        }

        .setpoint-tag {
            display: inline-block;
            margin-left: 8px;
            padding: 2px 10px;
            border-radius: 999px;
            font-size: 0.75rem;
            font-weight: 600;
            background: #374151;
            color: #E5E7EB;
        }

        .setpoint-tag.schedule {
            background: #0369a1;
            color: #e0f2fe;
        }

        .setpoint-tag.direct {
            background: #92400e;
            color: #fef9c3;
        }

        .control-group {
            margin-bottom: 0.5rem;
        }
        
        .control-group label {
            display: block;
            color: #9CA3AF;
            font-size: 0.75rem;
            margin-bottom: 0.25rem;
        }
        
        .control-group input,
        .control-group select {
            width: 100%;
            padding: 0.5rem;
            border: 1px solid #4B5563;
            border-radius: 0.375rem;
            font-size: 0.875rem;
            background: #374151;
            color: #E5E7EB;
        }
        
        .control-group input:focus,
        .control-group select:focus {
            outline: none;
            border-color: #00D4FF;
            box-shadow: 0 0 0 2px rgba(0, 212, 255, 0.2);
        }
        
        .slider-container {
            position: relative;
            margin: 0.5rem 0;
        }
        
        .slider {
            width: 100%;
            -webkit-appearance: none;
            height: 6px;
            border-radius: 3px;
            background: #4B5563;
            outline: none;
        }
        
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #00D4FF;
            cursor: pointer;
        }
        
        .slider-value {
            text-align: center;
            font-size: 18px;
            color: #E5E7EB;
            margin-top: 10px;
        }
        
        .mode-buttons {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 0.5rem;
            margin-top: 0.5rem;
        }
        
        .mode-btn {
            padding: 0.75rem;
            border: 1px solid #4B5563;
            background: #374151;
            border-radius: 0.375rem;
            cursor: pointer;
            text-align: center;
            transition: all 0.3s;
            color: #E5E7EB;
            font-size: 0.875rem;
        }
        
        .mode-btn:hover {
            border-color: #00D4FF;
            background: #4B5563;
        }
        
        .mode-btn.active {
            background: #00D4FF;
            color: #0B1426;
            border-color: #00D4FF;
            font-weight: 600;
        }
        
        .state-indicators {
            display: flex;
            justify-content: space-evenly;
            margin-top: 0.5rem;
            gap: 2rem;
        }
        
        .state-indicator {
            text-align: center;
        }
        
        .state-icon {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.25rem;
            margin: 0 auto 0.25rem;
        }
        
        .state-icon.on {
            background: #48bb78;
            color: white;
        }
        
        .state-icon.off {
            background: #e2e8f0;
            color: #a0aec0;
        }
        
        .state-label {
            font-size: 0.75rem;
            color: #9CA3AF;
        }
        
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }
        
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #4B5563;
            transition: .4s;
            border-radius: 34px;
        }
        
        .toggle-slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        
        input:checked + .toggle-slider {
            background-color: #00D4FF;
        }
        
        input:checked + .toggle-slider:before {
            transform: translateX(26px);
        }
        
        .emergency-section {
            background: rgba(252, 129, 129, 0.1);
            border: 1px solid #fc8181;
            border-radius: 0.375rem;
            padding: 0.75rem;
            margin-top: 0.5rem;
        }
        
        .emergency-btn {
            width: 100%;
            padding: 0.5rem;
            font-size: 0.75rem;
            font-weight: 600;
            border: none;
            border-radius: 0.25rem;
            cursor: pointer;
            margin-top: 0.25rem;
        }
        
        .emergency-stop {
            background: #fc8181;
            color: white;
        }
        
        .emergency-clear {
            background: #48bb78;
            color: white;
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 0.5rem;
            margin-bottom: 0.75rem;
        }
        
        .stat-item {
            background: #4B5563;
            padding: 0.5rem;
            border-radius: 0.375rem;
        }
        
        .stat-label {
            font-size: 12px;
            color: #9CA3AF;
            text-transform: uppercase;
        }
        
        .stat-value {
            font-size: 1.25rem;
            font-weight: bold;
            color: #E5E7EB;
            margin-top: 0.25rem;
        }
        
        .notification {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 15px 20px;
            background: #48bb78;
            color: white;
            border-radius: 8px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.2);
            display: none;
            z-index: 1000;
        }
        
        .notification.error {
            background: #fc8181;
        }
        
        .notification.show {
            display: block;
            animation: slideIn 0.3s ease;
        }
        
        @keyframes slideIn {
            from {
                transform: translateX(100%);
            }
            to {
                transform: translateX(0);
            }
        }
    </style>
</head>
<body>
    <!-- Professional Navigation Header -->
    <div class='header'>
        <div class='header-top'>
            <h1>Temperature Control System</h1>
            <div class='header-info'>
                <div class='header-time' id='headerTime'>--:--:--</div>
                <div class='header-date' id='headerDate'>--</div>
            </div>
        </div>
        <div class='nav-links'>
            <a href='/'>Dashboard</a>
            <a href='/system'>System</a>
            <a href='/relays'>Relays</a>
            <a href='/temperature' class='active'>Temperature</a>
            <a href='/schedule'>Schedule</a>
            <a href='/sensors'>Sensors</a>
            <a href='/wifi-config'>Network</a>
        </div>
    </div>

    <div class="container">
        <div class="main-grid">
            <div class="card">
                <h2>Temperature Monitor</h2>
                <div class="temperature-display">
                    <div>
                        <span class="temp-current" id="currentTemp">--</span>
                        <span class="temp-unit">°C</span>
                    </div>
                    <div class="temp-compensated">
                        Compensated: <span id="compensatedTemp">--</span>°C
                    </div>
                    <div class="temp-setpoint">
                        Active Setpoint: <span id="activeSetpointDisplay">--</span>°C
                        <span class="setpoint-tag direct" id="setpointSourceTag">Direct</span>
                    </div>
                    <div class="temp-compensated">
                        Manual Setpoint: <span id="manualSetpointDisplay">--</span>°C
                    </div>
                </div>
                <div class="state-indicators">
                    <div class="state-indicator">
                        <div class="state-icon off" id="compressorIcon">❄️</div>
                        <div class="state-label">Compressor</div>
                    </div>
                    <div class="state-indicator">
                        <div class="state-icon off" id="heaterIcon">🔥</div>
                        <div class="state-label">Heater</div>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <h2>Control Settings</h2>
                <div class="control-group">
                    <label>System Enable</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="systemEnable" onchange="toggleSystem()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                <div class="control-group">
                    <label>Operation Mode</label>
                    <div class="mode-buttons">
                        <button class="mode-btn" onclick="setMode(0)">OFF</button>
                        <button class="mode-btn" onclick="setMode(1)">HEAT</button>
                        <button class="mode-btn" onclick="setMode(2)">COOL</button>
                        <button class="mode-btn" onclick="setMode(3)">AUTO</button>
                    </div>
                </div>
                <div class="control-group">
                    <label>Setpoint: <span id="setpointValue">24.0</span>°C</label>
                    <div class="slider-container">
                        <input type="range" class="slider" id="setpointSlider" 
                               min="16" max="40" step="0.5" value="24" 
                               oninput="updateSetpoint()">
                    </div>
                </div>
                <div class="control-group">
                    <label>Delta: <span id="deltaValue">1.0</span>°C</label>
                    <input type="range" class="slider" id="deltaSlider" 
                           min="0.5" max="3" step="0.5" value="1" 
                           oninput="updateDelta()">
                </div>
                <div class="setpoint-source">
                    <div class="setpoint-info">
                        <div class="setpoint-title">Setpoint Source</div>
                        <div class="setpoint-subtitle">Switch between manual control and schedule automation</div>
                    </div>
                    <div style="display:flex;align-items:center;gap:12px">
                        <label class="toggle-switch">
                            <input type="checkbox" id="setpointToggle" />
                            <span class="toggle-slider"></span>
                        </label>
                        <span class="setpoint-status direct" id="setpointStatus">Direct</span>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <h2>Calibration</h2>
                <div class="control-group">
                    <label>Compensation: <span id="compensationValue">0.0</span>°C</label>
                    <input type="range" class="slider" id="compensationSlider" 
                           min="-5" max="5" step="0.5" value="0" 
                           oninput="updateCompensation()">
                </div>
            </div>
            
            <div class="card">
            </div>
        </div>
    </div>
    
    <div class="notification" id="notification"></div>
    
    <script>
        let currentMode = 0;
        let updateInterval;
        let setpointMode = 0; // 0=Direct, 1=Schedule

        function updateHeaderClock() {
            const elTime = document.getElementById('headerTime');
            const elDate = document.getElementById('headerDate');
            if (!elTime || !elDate) return;
            const now = new Date();
            elTime.textContent = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
            elDate.textContent = now.toLocaleDateString([], { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric' });
        }

        function showNotification(message, isError = false) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification show' + (isError ? ' error' : '');
            setTimeout(() => {
                notification.classList.remove('show');
            }, 3000);
        }
        
        async function fetchAPI(endpoint, method = 'GET', data = null) {
            try {
                const options = {
                    method: method,
                    headers: {
                        'Content-Type': 'application/json'
                    }
                };
                
                if (data) {
                    options.body = JSON.stringify(data);
                }
                
                const response = await fetch(`/api${endpoint}`, options);
                if (!response.ok) throw new Error('Request failed');
                return await response.json();
            } catch (error) {
                console.error('API Error:', error);
                showNotification('Communication error', true);
                return null;
            }
        }
        
        async function updateStatus() {
            const status = await fetchAPI('/temperature/status');
            if (!status) return;
            
            // Update temperature displays
            const currentTemp = document.getElementById('currentTemp');
            if (currentTemp) currentTemp.textContent = status.current_temp.toFixed(1);
            
            const compensatedTemp = document.getElementById('compensatedTemp');
            if (compensatedTemp) compensatedTemp.textContent = status.compensated_temp.toFixed(1);
            
            const activeSetpoint = status.active_setpoint !== undefined ? status.active_setpoint : status.setpoint;
            const manualSetpoint = status.manual_setpoint !== undefined ? status.manual_setpoint : status.setpoint;
            const activeSetpointDisplay = document.getElementById('activeSetpointDisplay');
            if (activeSetpointDisplay) activeSetpointDisplay.textContent = activeSetpoint.toFixed(1);
            const manualSetpointDisplay = document.getElementById('manualSetpointDisplay');
            if (manualSetpointDisplay) manualSetpointDisplay.textContent = manualSetpoint.toFixed(1);
            const sourceTag = document.getElementById('setpointSourceTag');
            if (sourceTag) {
                const label = status.setpoint_source === 'schedule' ? 'Schedule' : 'Direct';
                sourceTag.textContent = label;
                sourceTag.className = 'setpoint-tag ' + (status.setpoint_source === 'schedule' ? 'schedule' : 'direct');
            }
            
            // Update location
            const locationDisplay = document.getElementById('locationDisplay');
            if (locationDisplay) locationDisplay.textContent = status.location;
            
            // Update system status
            const systemStatus = document.getElementById('systemStatus');
            if (systemStatus) {
                systemStatus.textContent = `System: ${status.enabled ? 'ON' : 'OFF'}`;
                systemStatus.className = 'status-item ' + (status.enabled ? 'active' : 'inactive');
            }
            
            // Update sensor status
            const sensorStatus = document.getElementById('sensorStatus');
            if (sensorStatus) sensorStatus.textContent = `Sensor: ${status.sensor_valid ? 'OK' : 'ERROR'}`;
            if (sensorStatus) sensorStatus.className = 'status-item ' + (status.sensor_valid ? 'active' : 'inactive');
            
            // Update emergency status
            const emergencyStatus = document.getElementById('emergencyStatus');
            if (emergencyStatus) {
                emergencyStatus.textContent = `Emergency: ${status.emergency_stop ? 'ACTIVE' : 'OK'}`;
                emergencyStatus.className = 'status-item ' + (status.emergency_stop ? 'inactive' : 'active');
            }
            
            // Update state indicators
            const compressorIcon = document.getElementById('compressorIcon');
            if (compressorIcon) compressorIcon.className = 'state-icon ' + (status.state.compressor ? 'on' : 'off');
            
            const heaterIcon = document.getElementById('heaterIcon');
            if (heaterIcon) heaterIcon.className = 'state-icon ' + (status.state.heater ? 'on' : 'off');
            
            // Update controls
            const systemEnable = document.getElementById('systemEnable');
            if (systemEnable) systemEnable.checked = status.enabled;
            const setpointSlider = document.getElementById('setpointSlider');
            if (setpointSlider) setpointSlider.value = manualSetpoint;
            
            const setpointValue = document.getElementById('setpointValue');
            if (setpointValue) setpointValue.textContent = manualSetpoint.toFixed(1);
            
            const deltaSlider = document.getElementById('deltaSlider');
            if (deltaSlider) deltaSlider.value = status.delta;
            
            const deltaValue = document.getElementById('deltaValue');
            if (deltaValue) deltaValue.textContent = status.delta.toFixed(1);
            const compensationSlider = document.getElementById('compensationSlider');
            if (compensationSlider) compensationSlider.value = status.compensation;
            
            const compensationValue = document.getElementById('compensationValue');
            if (compensationValue) compensationValue.textContent = status.compensation.toFixed(1);
            
            // Update mode buttons
            currentMode = status.mode;
            document.querySelectorAll('.mode-btn').forEach((btn, index) => {
                btn.className = 'mode-btn' + (index === currentMode ? ' active' : '');
            });
            
            // Update statistics
            if (status.stats) {
                const runtime = document.getElementById('runtime');
                if (runtime) runtime.textContent = status.stats.runtime_hours.toFixed(1) + ' hrs';
                
                const cycles = document.getElementById('cycles');
                if (cycles) cycles.textContent = status.stats.cycles;
                const minTemp = document.getElementById('minTemp');
                if (minTemp) minTemp.textContent = status.stats.min_temp.toFixed(1) + '°C';
                
                const maxTemp = document.getElementById('maxTemp');
                if (maxTemp) maxTemp.textContent = status.stats.max_temp.toFixed(1) + '°C';
            }

            if (status.operation_mode !== undefined) {
                setpointMode = Number(status.operation_mode) === 1 ? 1 : 0;
                updateSetpointModeUI();
                const statusChip = document.getElementById('setpointStatus');
                if (statusChip) {
                    const usingSchedule = status.setpoint_source === 'schedule';
                    let chipText = setpointMode === 1 ? (usingSchedule ? 'Schedule' : 'Schedule (inactive)') : 'Direct';
                    statusChip.textContent = chipText;
                    statusChip.className = 'setpoint-status ' + (usingSchedule ? 'schedule' : 'direct');
                }
            }
        }
        
        async function loadConfig() {
            const config = await fetchAPI('/temperature/config');
            if (!config) return;
            
            const timing = await fetchAPI('/temperature/timing');
            if (timing) {
                document.getElementById('minOnTime').value = timing.min_on_time / 60000;
                document.getElementById('minOffTime').value = timing.min_off_time / 60000;
            }
        }
        
        async function saveLocation() {
            const location = document.getElementById('locationInput').value;
            if (!location) return;
            
            const result = await fetchAPI('/temperature/location', 'POST', { location });
            if (result) {
                showNotification('Location saved');
                document.getElementById('locationInput').value = '';
                updateStatus();
            }
        }
        
        async function toggleSystem() {
            const enabled = document.getElementById('systemEnable').checked;
            const endpoint = enabled ? '/temperature/enable' : '/temperature/disable';
            const result = await fetchAPI(endpoint, 'POST');
            if (result) {
                showNotification(`System ${enabled ? 'enabled' : 'disabled'}`);
            }
        }
        
        async function setMode(mode) {
            const result = await fetchAPI('/temperature/mode', 'POST', { mode });
            if (result) {
                const modeNames = ['OFF', 'HEATING', 'COOLING', 'AUTO'];
                showNotification(`Mode set to ${modeNames[mode]}`);
                updateStatus();
            }
        }
        
        async function updateSetpoint() {
            const setpoint = parseFloat(document.getElementById('setpointSlider').value);
            document.getElementById('setpointValue').textContent = setpoint.toFixed(1);
            const manualDisplay = document.getElementById('manualSetpointDisplay');
            if (manualDisplay) manualDisplay.textContent = setpoint.toFixed(1);
            
            const result = await fetchAPI('/temperature/setpoint', 'POST', { setpoint });
            if (result) {
                showNotification('Setpoint updated');
                updateStatus();
            }
        }
        
        async function updateDelta() {
            const delta = parseFloat(document.getElementById('deltaSlider').value);
            document.getElementById('deltaValue').textContent = delta.toFixed(1);
            
            const result = await fetchAPI('/temperature/setpoint', 'POST', { delta });
            if (result) {
                showNotification('Hysteresis updated');
            }
        }
        
        async function updateCompensation() {
            const compensation = parseFloat(document.getElementById('compensationSlider').value);
            document.getElementById('compensationValue').textContent = compensation.toFixed(1);
            
            const result = await fetchAPI('/temperature/compensation', 'POST', { compensation });
            if (result) {
                showNotification('Compensation updated');
            }
        }

        function updateSetpointModeUI() {
            const toggle = document.getElementById('setpointToggle');
            const status = document.getElementById('setpointStatus');
            if (!toggle || !status) return;
            toggle.checked = setpointMode === 1;
            status.textContent = setpointMode === 1 ? 'Schedule' : 'Direct';
            status.className = 'setpoint-status ' + (setpointMode === 1 ? 'schedule' : 'direct');
        }

        async function setSetpointMode(mode) {
            const desired = mode === 1 ? 1 : 0;
            const toggle = document.getElementById('setpointToggle');
            if (toggle) toggle.disabled = true;
            try {
                const res = await fetch('/api/schedule/setpoint-mode', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ mode: desired === 1 ? 'schedule' : 'direct' })
                });
                if (!res.ok) throw new Error('Request failed (' + res.status + ')');
                const data = await res.json();
                if (data && data.operation_mode !== undefined) {
                    setpointMode = Number(data.operation_mode) === 1 ? 1 : 0;
                } else {
                    setpointMode = desired;
                }
                updateSetpointModeUI();
            } catch (err) {
                console.error(err);
                showNotification('Failed to change mode', true);
                updateSetpointModeUI();
            } finally {
                if (toggle) toggle.disabled = false;
                updateStatus();
            }
        }

        async function emergencyStop() {
            if (confirm('Activate emergency stop?')) {
                const result = await fetchAPI('/temperature/emergency/stop', 'POST');
                if (result) {
                    showNotification('EMERGENCY STOP ACTIVATED', true);
                    updateStatus();
                }
            }
        }
        
        async function emergencyClear() {
            const result = await fetchAPI('/temperature/emergency/clear', 'POST');
            if (result) {
                showNotification('Emergency stop cleared');
                updateStatus();
            }
        }
        
        // Initialize
        document.addEventListener('DOMContentLoaded', () => {
            updateStatus();
            loadConfig();
            updateHeaderClock();
            updateSetpointModeUI();

            const toggle = document.getElementById('setpointToggle');
            if (toggle) {
                toggle.addEventListener('change', () => {
                    const target = toggle.checked ? 1 : 0;
                    if (target !== setpointMode) {
                        setSetpointMode(target);
                    }
                });
            }

            // Update every 4 seconds
            updateInterval = setInterval(updateStatus, 4000);
            setInterval(updateHeaderClock, 1000);
        });
    </script>
</body>
</html>
)rawliteral";

#endif // TEMPERATURE_HTML_H
