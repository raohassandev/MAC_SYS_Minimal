// Simple thermostat-style schedule UI replacement

    device_server->on("/schedule", []() {
        String html = "<!DOCTYPE html><html><head><title>AC Schedule Control</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta charset='UTF-8'>";
        html += "<style>";
        html += "body{font-family:Arial,sans-serif;margin:20px;background:#f0f2f5;color:#333}";
        html += ".container{background:white;padding:30px;border-radius:12px;box-shadow:0 4px 20px rgba(0,0,0,0.1);max-width:1000px;margin:0 auto}";
        html += ".header{text-align:center;margin-bottom:30px}";
        html += ".status-card{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:20px;border-radius:10px;margin:20px 0;text-align:center}";
        html += ".controls{display:flex;justify-content:center;gap:15px;margin:20px 0;flex-wrap:wrap}";
        html += ".toggle-btn{padding:12px 24px;border:none;border-radius:25px;font-weight:bold;cursor:pointer;transition:all 0.3s ease}";
        html += ".toggle-btn.active{background:#4CAF50;color:white}";
        html += ".toggle-btn.inactive{background:#f0f0f0;color:#666}";
        html += ".schedule-grid{margin:30px 0;overflow-x:auto}";
        html += ".grid-table{width:100%;border-collapse:collapse;background:white;border-radius:8px;overflow:hidden;box-shadow:0 2px 10px rgba(0,0,0,0.05)}";
        html += ".grid-table th{background:#f8f9fa;padding:15px 10px;text-align:center;font-weight:bold;border-bottom:2px solid #dee2e6}";
        html += ".grid-table td{padding:12px 8px;text-align:center;border-bottom:1px solid #eee}";
        html += ".grid-table tr:hover{background:#f8f9fa}";
        html += ".day-name{font-weight:bold;background:#e9ecef!important;text-align:left;padding-left:15px}";
        html += ".period-cell{min-width:120px}";
        html += ".temp-input{width:60px;padding:5px;border:1px solid #ddd;border-radius:4px;text-align:center}";
        html += ".time-display{font-size:11px;color:#666;display:block}";
        html += ".enable-checkbox{transform:scale(1.2)}";
        html += ".save-btn{background:#28a745;color:white;padding:15px 30px;border:none;border-radius:8px;font-size:16px;cursor:pointer;display:block;margin:30px auto;box-shadow:0 3px 10px rgba(40,167,69,0.3)}";
        html += ".save-btn:hover{background:#218838;transform:translateY(-2px)}";
        html += "@media (max-width: 768px){.grid-table th,.grid-table td{padding:8px 5px;font-size:12px}.temp-input{width:50px}.period-cell{min-width:100px}}";
        html += "</style></head><body>";
        
        html += "<div class='container'>";
        html += "<div class='header'>";
        html += "<h1>🏠 AC Schedule Control</h1>";
        html += "<p style='color:#666;margin:0'>Simple weekly temperature scheduling for your air conditioning</p>";
        html += "</div>";
        
        // Status Card
        ScheduleStatus status = simple_schedule.getStatus();
        html += "<div class='status-card'>";
        html += "<h3 style='margin:0 0 10px 0'>📅 " + rtc_manager.getFormattedDateTime() + "</h3>";
        if (simple_schedule.isScheduleActive()) {
            html += "<div style='font-size:18px;margin:10px 0'>🎯 " + status.active_period_name + " Period Active</div>";
            html += "<div style='font-size:24px;font-weight:bold'>" + String(status.current_setpoint, 1) + "°C Target</div>";
            html += "<div style='font-size:14px;opacity:0.9'>Next: " + status.next_change_time + " → " + String(status.next_setpoint, 1) + "°C</div>";
        } else {
            html += "<div style='font-size:18px;margin:10px 0'>📱 Direct Control Mode</div>";
            html += "<div style='font-size:24px;font-weight:bold'>" + String(g_system_config.ac_setpoint, 1) + "°C Manual</div>";
        }
        html += "</div>";
        
        // Control Toggles
        html += "<div class='controls'>";
        html += "<button class='toggle-btn " + String(simple_schedule.isScheduleActive() ? "active" : "inactive") + "' onclick='toggleSchedule()'>";
        html += simple_schedule.isScheduleActive() ? "📅 SCHEDULE ON" : "📱 MANUAL MODE";
        html += "</button>";
        html += "</div>";
        
        // Weekly Schedule Grid
        html += "<div class='schedule-grid'>";
        html += "<h3 style='text-align:center;color:#555;margin:30px 0 20px 0'>📊 Weekly Temperature Schedule</h3>";
        html += "<table class='grid-table'>";
        html += "<thead><tr>";
        html += "<th style='text-align:left'>Day</th>";
        html += "<th class='period-cell'>Morning<br><span class='time-display'>6:00 - 12:00</span></th>";
        html += "<th class='period-cell'>Afternoon<br><span class='time-display'>12:00 - 18:00</span></th>";
        html += "<th class='period-cell'>Evening<br><span class='time-display'>18:00 - 22:00</span></th>";
        html += "<th>Day On/Off</th>";
        html += "</tr></thead><tbody>";
        
        // Generate schedule grid for each day
        String days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        for (int day = 0; day < 7; day++) {
            html += "<tr>";
            html += "<td class='day-name'>" + days[day] + "</td>";
            
            // Morning period
            ScheduleEntry morning = simple_schedule.getScheduleEntry(day, 0);
            html += "<td class='period-cell'>";
            html += "<input type='checkbox' " + String(morning.active ? "checked" : "") + " id='morning_" + String(day) + "_active'> ";
            html += "<input type='number' value='" + String(morning.target_temperature, 1) + "' min='16' max='35' step='0.5' class='temp-input' id='morning_" + String(day) + "_temp'>°C";
            html += "</td>";
            
            // Afternoon period
            ScheduleEntry afternoon = simple_schedule.getScheduleEntry(day, 1);
            html += "<td class='period-cell'>";
            html += "<input type='checkbox' " + String(afternoon.active ? "checked" : "") + " id='afternoon_" + String(day) + "_active'> ";
            html += "<input type='number' value='" + String(afternoon.target_temperature, 1) + "' min='16' max='35' step='0.5' class='temp-input' id='afternoon_" + String(day) + "_temp'>°C";
            html += "</td>";
            
            // Evening period
            ScheduleEntry evening = simple_schedule.getScheduleEntry(day, 2);
            html += "<td class='period-cell'>";
            html += "<input type='checkbox' " + String(evening.active ? "checked" : "") + " id='evening_" + String(day) + "_active'> ";
            html += "<input type='number' value='" + String(evening.target_temperature, 1) + "' min='16' max='35' step='0.5' class='temp-input' id='evening_" + String(day) + "_temp'>°C";
            html += "</td>";
            
            // Day enable/disable
            html += "<td>";
            html += "<input type='checkbox' class='enable-checkbox' " + String(simple_schedule.isDayEnabled(day) ? "checked" : "") + " id='day_" + String(day) + "_enabled'>";
            html += "</td>";
            
            html += "</tr>";
        }
        
        html += "</tbody></table>";
        html += "</div>";
        
        // Save button
        html += "<button class='save-btn' onclick='saveSchedule()'>💾 Save Schedule</button>";
        
        // Navigation
        html += "<div style='text-align:center;margin-top:30px'>";
        html += "<button onclick=\"location.href='/'\" style='background:#6c757d;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer'>← Back to Main</button>";
        html += "</div>";
        
        html += "</div>";
        
        // JavaScript
        html += "<script>";
        html += "function toggleSchedule() {";
        html += "  const isActive = " + String(simple_schedule.isScheduleActive() ? "true" : "false") + ";";
        html += "  fetch('/api/schedule/active', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'enabled=' + (!isActive)";
        html += "  }).then(() => location.reload()).catch(err => alert('Failed to toggle: ' + err));";
        html += "}";
        
        html += "function saveSchedule() {";
        html += "  const scheduleData = [];";
        html += "  for (let day = 0; day < 7; day++) {";
        html += "    const dayData = {";
        html += "      day: day,";
        html += "      enabled: document.getElementById('day_' + day + '_enabled').checked,";
        html += "      morning: {";
        html += "        active: document.getElementById('morning_' + day + '_active').checked,";
        html += "        temp: parseFloat(document.getElementById('morning_' + day + '_temp').value)";
        html += "      },";
        html += "      afternoon: {";
        html += "        active: document.getElementById('afternoon_' + day + '_active').checked,";
        html += "        temp: parseFloat(document.getElementById('afternoon_' + day + '_temp').value)";
        html += "      },";
        html += "      evening: {";
        html += "        active: document.getElementById('evening_' + day + '_active').checked,";
        html += "        temp: parseFloat(document.getElementById('evening_' + day + '_temp').value)";
        html += "      }";
        html += "    };";
        html += "    scheduleData.push(dayData);";
        html += "  }";
        html += "  fetch('/api/schedule/save', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/json'},";
        html += "    body: JSON.stringify(scheduleData)";
        html += "  }).then(response => response.json()).then(data => {";
        html += "    if (data.success) {";
        html += "      alert('✅ Schedule saved successfully!');";
        html += "      location.reload();";
        html += "    } else {";
        html += "      alert('❌ Failed to save: ' + data.message);";
        html += "    }";
        html += "  }).catch(err => alert('❌ Save failed: ' + err));";
        html += "}";
        html += "</script>";
        
        html += "</body></html>";
        
        device_server->send(200, "text/html", html);
    });