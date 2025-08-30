import React, { createContext, useState } from "react";

export const IoTContext = createContext();

export const IoTProvider = ({ children }) => {
  const [lightsOn, setLightsOn] = useState(false);
  const [flameDetected, setFlameDetected] = useState(false);
  const [buzzerOn, setBuzzerOn] = useState(false); 

  return (
    <IoTContext.Provider
      value={{
        lightsOn,
        setLightsOn,
        flameDetected,
        setFlameDetected,
        buzzerOn,
        setBuzzerOn, // 🔹 Expose buzzer control
      }}
    >
      {children}
    </IoTContext.Provider>
  );
};
