# Development Standard Operating Procedure (SOP)

## Upload-Test-Commit Workflow

### Overview
This document outlines the mandatory workflow for making changes to the MAC-SYS Arduino project to ensure code quality and system reliability.

### Workflow Steps

#### 1. Code Development
- Make necessary code changes
- Review implementation for:
  - Syntax correctness
  - Logic flow
  - Security considerations
  - Code style consistency

#### 2. Upload to Device
```bash
pio run --target upload
```
- **MANDATORY**: Always upload before testing
- Monitor compilation output for warnings/errors
- Ensure upload completes successfully
- Wait for device restart completion

#### 3. Testing Phase
**CRITICAL**: Never skip testing phase

##### A. Basic Connectivity Test
```bash
curl -s "http://192.168.1.2/" | head -20
```

##### B. Feature-Specific Testing
- **Web Interface**: Test all modified pages/features
- **API Endpoints**: Verify new/modified API calls
- **Configuration Persistence**: Test save/reload functionality  
- **Real-time Updates**: Verify dynamic content updates

##### C. Integration Testing
- Test interaction between modified and existing features
- Verify no regressions in unmodified functionality
- Check system stability under normal operations

#### 4. Test Results Evaluation
**PASS Criteria:**
- All functionality works as expected
- No critical errors or exceptions
- Configuration persists across reloads/reboots
- Real-time features update properly
- No regression in existing features

**FAIL Criteria:**
- Any critical functionality broken
- Configuration not persisting
- Real-time updates not working
- System instability or crashes
- Compilation errors or warnings

#### 5. Commit Decision
**✅ COMMIT** - Only if ALL tests pass:
```bash
git add .
git commit -m "Feature description

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

**❌ DO NOT COMMIT** - If ANY test fails:
- Debug and fix issues
- Return to step 1 (Code Development)
- Repeat entire workflow

### Testing Commands Reference

#### Web Interface Testing
```bash
# Test main pages
curl -s "http://192.168.1.2/"
curl -s "http://192.168.1.2/sensors"
curl -s "http://192.168.1.2/temperature"
curl -s "http://192.168.1.2/relays"

# Test API endpoints
curl -s "http://192.168.1.2/api/sensors/config"
curl -s "http://192.168.1.2/api/system/status"
```

#### Configuration Testing
```bash
# Test configuration save
curl -X POST "http://192.168.1.2/api/sensors/config" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "ds18b20_enabled=true&ds18b20_pin=4&ds18b20_priority=0"

# Verify persistence
curl -s "http://192.168.1.2/api/sensors/config"
```

#### Sensor Testing
```bash
# Test individual sensors
curl -s "http://192.168.1.2/api/sensors/test?type=2&pin=4"  # DS18B20
curl -s "http://192.168.1.2/api/sensors/test?type=1&pin=2"  # AM2302
curl -s "http://192.168.1.2/api/sensors/test?type=3&pin=32" # LM35
```

### Common Issues and Solutions

#### Issue: Configuration Not Persisting
**Symptoms**: Settings revert after page refresh
**Debug Steps**:
1. Check EEPROM write operations in logs
2. Verify API POST returns success
3. Test GET API immediately after POST
4. Check server-side HTML generation

#### Issue: Real-time Updates Not Working  
**Symptoms**: Page requires refresh to see changes
**Debug Steps**:
1. Check browser console for JavaScript errors
2. Verify event listeners are attached
3. Test API endpoints manually
4. Check timing of DOM ready events

#### Issue: Compilation Warnings
**Action**: Fix all warnings before proceeding
**Common**: Deprecated library functions, unused variables

### Emergency Rollback
If testing reveals critical issues:

```bash
# Rollback to previous working commit
git log --oneline -10  # Find last working commit
git checkout <commit-hash>
pio run --target upload
# Test functionality
# If working, create new branch or fix issues
```

### Best Practices
1. **Always test incrementally** - Don't accumulate multiple untested changes
2. **Test edge cases** - Empty configurations, invalid inputs, network issues
3. **Document test results** - Note what was tested and outcomes
4. **Verify persistence** - Test across device reboots when possible
5. **Check logs** - Monitor serial output during testing

### Mandatory Checklist
Before each commit, verify:

- [ ] Code uploaded successfully to device
- [ ] Device boots and connects to network  
- [ ] All modified features tested and working
- [ ] Configuration persistence verified
- [ ] Real-time updates functioning
- [ ] No regressions in existing functionality
- [ ] All API endpoints responding correctly
- [ ] Web interface displays properly
- [ ] No critical compilation warnings

**Remember: A broken commit wastes everyone's time. Test thoroughly!**