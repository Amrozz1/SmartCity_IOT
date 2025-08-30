import React from 'react'
import { Link } from "react-router-dom"
 
function Header() {
  return (
    <div>
      <div className='parent'>
         <div>
          <p className='welcometext'>AutoLight</p>
         </div>
         <div className='links'>
         <Link to="/" className='link'>Home</Link>
         <Link to="/dashboard" className='link'>Dashboard</Link>
          <Link to="/login" className='link'>Login</Link>
          <Link to="/signup" className='link'>Sign up</Link>
          <Link to="/Controlpanel" className='link'>Control</Link>
         </div>
      </div>
    </div>
  )
}

export default Header
