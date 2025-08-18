#ifndef CAN_SIMULATOR_H
#define CAN_SIMULATOR_H

#include <Arduino.h>

// CAN Simulator for testing without actual ECU
class CANSimulator {
private:
  unsigned long last_send_time = 0;
  float sim_rpm = 800;
  float sim_tps = 0;
  bool engine_running = false;
  unsigned long engine_start_time = 0;
  
public:
  void begin() {
    Serial.println("CAN Simulator initialized");
    engine_start_time = millis();
  }
  
  void update() {
    if (millis() - last_send_time > 50) { // 20Hz simulation
      simulateEngineData();
      last_send_time = millis();
    }
  }
  
private:
  void simulateEngineData() {
    // Simulate engine startup and running
    unsigned long runtime = millis() - engine_start_time;
    
    if (runtime < 5000) {
      // Cranking phase
      sim_rpm = 200 + random(0, 100);
      sim_tps = 0;
      engine_running = false;
    } else if (runtime < 10000) {
      // Starting phase
      sim_rpm = 800 + random(-50, 50);
      sim_tps = random(0, 20);
      engine_running = true;
    } else {
      // Normal running - simulate some driving
      engine_running = true;
      float cycle_time = (runtime - 10000) / 1000.0;
      sim_rpm = 1500 + sin(cycle_time * 0.1) * 2000 + random(-100, 100);
      sim_tps = 20 + sin(cycle_time * 0.15) * 30 + random(-5, 5);
      
      // Clamp values
      sim_rpm = constrain(sim_rpm, 800, 7000);
      sim_tps = constrain(sim_tps, 0, 100);
    }
    
    sendSimulatedCANMessages();
  }
  
  void sendSimulatedCANMessages() {
    // Simulate RPM/TPS message (0x360)
    sendRPMTPS();
    
    // Simulate MAP/Lambda message (0x361)
    sendMAPLambda();
    
    // Simulate temperatures (0x362)
    sendTemperatures();
    
    // Simulate pressures (0x363)
    sendPressures();
    
    // Simulate battery (0x364)
    sendBattery();
    
    // Simulate fuel data (0x365)
    sendFuelData();
    
    // Simulate timing (0x366)
    sendTiming();
    
    // Simulate status (0x367)
    sendStatus();
  }
  
  void sendRPMTPS() {
    uint8_t data[8] = {0};
    uint16_t rpm_raw = (uint16_t)(sim_rpm / 0.39063);
    uint8_t tps_raw = (uint8_t)(sim_tps / 0.39216);
    
    data[0] = rpm_raw & 0xFF;
    data[1] = (rpm_raw >> 8) & 0xFF;
    data[2] = tps_raw;
    
    // Simulate sending CAN message
    Serial.printf("SIM CAN 0x360: RPM=%.0f TPS=%.1f\n", sim_rpm, sim_tps);
  }
  
  void sendMAPLambda() {
    uint16_t map_raw = (uint16_t)((80 + sim_tps * 1.5) * 10); // Simulate MAP based on TPS
    uint16_t lambda_raw = (uint16_t)((0.85 + random(-50, 50) * 0.001) * 10000);
    
    Serial.printf("SIM CAN 0x361: MAP=%.1f Lambda=%.3f\n", 
                  map_raw * 0.1, lambda_raw * 0.0001);
  }
  
  void sendTemperatures() {
    uint8_t coolant = 85 + random(-5, 5); // 80-90°C
    uint8_t intake = 25 + random(-5, 10);  // 20-35°C
    uint8_t oil = 90 + random(-10, 10);    // 80-100°C
    
    Serial.printf("SIM CAN 0x362: Coolant=%d Intake=%d Oil=%d\n", 
                  coolant, intake, oil);
  }
  
  void sendPressures() {
    uint16_t oil_pressure = (uint16_t)((300 + sim_rpm * 0.1) * 10);
    uint16_t fuel_pressure = (uint16_t)(350 * 10);
    uint16_t boost_pressure = (uint16_t)((100 + sim_tps * 2) * 10);
    
    Serial.printf("SIM CAN 0x363: Oil=%.1f Fuel=%.1f Boost=%.1f\n",
                  oil_pressure * 0.1, fuel_pressure * 0.1, boost_pressure * 0.1);
  }
  
  void sendBattery() {
    uint16_t battery = (uint16_t)((13.8 + random(-20, 20) * 0.01) * 100);
    Serial.printf("SIM CAN 0x364: Battery=%.2fV\n", battery * 0.01);
  }
  
  void sendFuelData() {
    uint8_t duty = (uint8_t)(sim_tps * 0.8); // Rough correlation
    uint16_t flow = (uint16_t)(sim_rpm * 0.02 * 10);
    
    Serial.printf("SIM CAN 0x365: Duty=%d%% Flow=%.1fL/h\n", 
                  duty, flow * 0.1);
  }
  
  void sendTiming() {
    int16_t ign_timing = (int16_t)((15 + sim_rpm * 0.005) * 10);
    int16_t fuel_timing = (int16_t)((-5) * 10);
    
    Serial.printf("SIM CAN 0x366: Ign=%.1f° Fuel=%.1f°\n",
                  ign_timing * 0.1, fuel_timing * 0.1);
  }
  
  void sendStatus() {
    uint16_t errors = 0; // No errors in simulation
    uint8_t status = engine_running ? 0x01 : 0x00;
    
    Serial.printf("SIM CAN 0x367: Status=0x%02X Errors=0x%04X\n", 
                  status, errors);
  }
};

#endif
