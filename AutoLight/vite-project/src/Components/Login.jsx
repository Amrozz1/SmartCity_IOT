import React, { useState } from "react";


function Login() {
  const [form, setForm] = useState({ email: "", password: "" });
  const [isLoggedIn, setIsLoggedIn] = useState(false);

  function handleSubmit(e) {
    e.preventDefault();
    console.log("Login form submitted:", form);
    setIsLoggedIn(true);
  }

  return (
    <div className="auth-container">
      <div className="auth-box">
        <h2>Login</h2>
        <form onSubmit={handleSubmit}>
          <div className="input-group">
            <label>Email</label>
            <input
              type="email"
              value={form.email}
              onChange={(e) => setForm({ ...form, email: e.target.value })}
              required
            />
          </div>

          <div className="input-group">
            <label>Password</label>
            <input
              type="password"
              value={form.password}
              onChange={(e) => setForm({ ...form, password: e.target.value })}
              required
            />
          </div>

          <button type="submit" className="auth-btn">Login</button>
        </form>

        {isLoggedIn && (
          <p className="success-text">✅ Logged in as {form.email}</p>
        )}
      </div>
    </div>
  );
}

export default Login;
