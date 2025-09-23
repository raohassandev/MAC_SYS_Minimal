#ifndef LOGIN_HTML_H
#define LOGIN_HTML_H

const char login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MAC-SYS Login</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .login-container {
            background: white;
            padding: 2rem;
            border-radius: 10px;
            box-shadow: 0 10px 25px rgba(0,0,0,0.1);
            width: 100%;
            max-width: 400px;
        }
        
        .logo {
            text-align: center;
            margin-bottom: 2rem;
        }
        
        .logo h1 {
            color: #333;
            font-size: 2rem;
            margin-bottom: 0.5rem;
        }
        
        .logo p {
            color: #666;
            font-size: 0.9rem;
        }
        
        .form-group {
            margin-bottom: 1.5rem;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 0.5rem;
            color: #333;
            font-weight: 500;
        }
        
        .form-group input {
            width: 100%;
            padding: 0.75rem;
            border: 2px solid #e1e5e9;
            border-radius: 5px;
            font-size: 1rem;
            transition: border-color 0.3s;
        }
        
        .form-group input:focus {
            outline: none;
            border-color: #667eea;
        }
        
        .login-btn {
            width: 100%;
            padding: 0.75rem;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 1rem;
            font-weight: 500;
            cursor: pointer;
            transition: transform 0.2s;
        }
        
        .login-btn:hover {
            transform: translateY(-1px);
        }
        
        .login-btn:disabled {
            opacity: 0.6;
            cursor: not-allowed;
            transform: none;
        }
        
        .error-message {
            background: #ffe6e6;
            color: #d63031;
            padding: 0.75rem;
            border-radius: 5px;
            margin-bottom: 1rem;
            display: none;
        }
        
        .success-message {
            background: #e6f7e6;
            color: #00b894;
            padding: 0.75rem;
            border-radius: 5px;
            margin-bottom: 1rem;
            display: none;
        }
        
        .default-credentials {
            background: #f8f9fa;
            padding: 1rem;
            border-radius: 5px;
            margin-top: 1rem;
            font-size: 0.85rem;
            color: #666;
        }
        
        .default-credentials h4 {
            margin-bottom: 0.5rem;
            color: #333;
        }
        
        .cred-item {
            margin-bottom: 0.25rem;
        }
    </style>
</head>
<body>
    <div class="login-container">
        <div class="logo">
            <h1>MAC-SYS</h1>
            <p>Temperature Control System</p>
        </div>
        
        <div class="error-message" id="errorMessage"></div>
        <div class="success-message" id="successMessage"></div>
        
        <form id="loginForm">
            <div class="form-group">
                <label for="username">Username</label>
                <input type="text" id="username" name="username" required autocomplete="username">
            </div>
            
            <div class="form-group">
                <label for="password">Password</label>
                <input type="password" id="password" name="password" required autocomplete="current-password">
            </div>
            
            <button type="submit" class="login-btn" id="loginBtn">
                Sign In
            </button>
        </form>
        
        <div class="default-credentials">
            <h4>Default Credentials:</h4>
            <div class="cred-item"><strong>Admin:</strong> admin / admin123</div>
            <div class="cred-item"><strong>User:</strong> user / user123</div>
        </div>
    </div>

    <script>
        document.getElementById('loginForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            const loginBtn = document.getElementById('loginBtn');
            const errorMsg = document.getElementById('errorMessage');
            const successMsg = document.getElementById('successMessage');
            
            // Hide previous messages
            errorMsg.style.display = 'none';
            successMsg.style.display = 'none';
            
            // Disable button
            loginBtn.disabled = true;
            loginBtn.textContent = 'Signing in...';
            
            try {
                const response = await fetch('/api/auth/login', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({
                        username: username,
                        password: password
                    })
                });
                
                const result = await response.json();
                
                if (result.success) {
                    // Store session in localStorage
                    localStorage.setItem('isAuthenticated', 'true');
                    localStorage.setItem('userRole', result.role);
                    localStorage.setItem('username', result.username);
                    localStorage.setItem('loginTime', Date.now().toString());
                    
                    // Show success message
                    successMsg.textContent = 'Login successful! Redirecting...';
                    successMsg.style.display = 'block';
                    
                    // Redirect to dashboard
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 1000);
                } else {
                    // Show error message
                    errorMsg.textContent = result.message || 'Login failed';
                    errorMsg.style.display = 'block';
                }
            } catch (error) {
                errorMsg.textContent = 'Connection error. Please try again.';
                errorMsg.style.display = 'block';
            }
            
            // Re-enable button
            loginBtn.disabled = false;
            loginBtn.textContent = 'Sign In';
        });
        
        // Auto-fill admin credentials when clicking on them
        document.querySelector('.default-credentials').addEventListener('click', function(e) {
            if (e.target.textContent.includes('admin / admin123')) {
                document.getElementById('username').value = 'admin';
                document.getElementById('password').value = 'admin123';
            } else if (e.target.textContent.includes('user / user123')) {
                document.getElementById('username').value = 'user';
                document.getElementById('password').value = 'user123';
            }
        });
    </script>
</body>
</html>
)rawliteral";

#endif // LOGIN_HTML_H