import React from 'react'
import Car from "../assets/Car.png"
function Body() {
  return (
    <div className='mainbody'>
      <div className='maintext'>
        <p className='text1'>Welcome to AutoLight</p>
        <p className='text2'>Smart Street Lighting for</p>
        <p className='text3'>Smarter <strong className="wordcities">Cities</strong></p>
        <p className='text4'>Save energy with IoT-powered lights that turn on only when vehicles pass</p>
        <p className='text5'>a step towards sustainable and smart cities.</p>
      </div>
      <div>
      <img src={Car} className='carimage'></img>
      </div>
    </div>
  )
}

export default Body
