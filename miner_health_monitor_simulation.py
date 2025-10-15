"""
Miner Health Monitoring System - Simulation
Demonstrates the complete workflow without hardware
"""

import time
import random
import math
from datetime import datetime
from enum import Enum

class SystemState(Enum):
    SLEEP = 1
    WAKING = 2
    MONITORING = 3
    EXTENDED_MONITORING = 4
    EMERGENCY = 5
    TRANSMITTING = 6

class HealthStatus(Enum):
    NORMAL = 1
    WARNING = 2
    CRITICAL = 3
    EMERGENCY = 4

class VitalSigns:
    def __init__(self):
        self.spo2 = 0
        self.heart_rate = 0
        self.bp_systolic = 0
        self.bp_diastolic = 0
        self.temperature = 0.0
        self.accel_x = 0.0
        self.accel_y = 0.0
        self.accel_z = 0.0
        self.fall_detected = False
        self.no_movement = False
        self.timestamp = None

class MinerHealthMonitor:
    # Thresholds
    SPO2_MIN_NORMAL = 92
    SPO2_MIN_CRITICAL = 85
    HEART_RATE_MIN = 45
    HEART_RATE_MAX = 120
    HEART_RATE_CRITICAL_MIN = 40
    HEART_RATE_CRITICAL_MAX = 150
    TEMP_MIN_NORMAL = 35.5
    TEMP_MAX_NORMAL = 38.5
    TEMP_CRITICAL_MIN = 35.0
    TEMP_CRITICAL_MAX = 40.0
    
    NORMAL_INTERVAL = 35  # seconds
    EXTENDED_INTERVAL = 10  # seconds
    EMERGENCY_INTERVAL = 5  # seconds
    
    def __init__(self):
        self.current_state = SystemState.SLEEP
        self.health_status = HealthStatus.NORMAL
        self.vitals = VitalSigns()
        self.monitoring_interval = self.NORMAL_INTERVAL
        self.anomaly_count = 0
        self.emergency_sent = False
        self.measurement_count = 0
        
    def simulate_sensor_readings(self):
        """Simulate realistic sensor readings"""
        self.vitals = VitalSigns()
        self.vitals.timestamp = datetime.now()
        
        # Normal values with variation
        self.vitals.spo2 = random.randint(95, 99)
        self.vitals.heart_rate = random.randint(70, 90)
        self.vitals.bp_systolic = random.randint(110, 130)
        self.vitals.bp_diastolic = random.randint(70, 85)
        self.vitals.temperature = round(random.uniform(36.2, 37.2), 2)
        self.vitals.accel_x = round(random.uniform(-0.2, 0.2), 2)
        self.vitals.accel_y = round(random.uniform(-0.2, 0.2), 2)
        self.vitals.accel_z = round(random.uniform(0.9, 1.1), 2)
        
        # Simulate occasional anomalies
        self.measurement_count += 1
        
        # Every 8th measurement - low SpO2
        if self.measurement_count % 8 == 0:
            self.vitals.spo2 = random.randint(87, 91)
            print("⚠️  Simulating low SpO2 condition")
            
        # Every 12th measurement - high heart rate
        if self.measurement_count % 12 == 0:
            self.vitals.heart_rate = random.randint(122, 135)
            print("⚠️  Simulating elevated heart rate")
            
        # Every 15th measurement - fall
        if self.measurement_count % 15 == 0:
            self.vitals.fall_detected = True
            self.vitals.accel_x = random.uniform(-3.0, 3.0)
            self.vitals.accel_y = random.uniform(-3.0, 3.0)
            self.vitals.accel_z = random.uniform(3.5, 4.5)
            print("🚨 Simulating FALL event!")
            
    def analyze_health(self):
        """Analyze health based on vital signs"""
        warning_flags = 0
        critical_flags = 0
        
        # Check SpO2
        if self.vitals.spo2 < self.SPO2_MIN_CRITICAL:
            critical_flags += 1
            print(f"❌ CRITICAL: SpO2 too low: {self.vitals.spo2}%")
        elif self.vitals.spo2 < self.SPO2_MIN_NORMAL:
            warning_flags += 1
            print(f"⚠️  WARNING: SpO2 below normal: {self.vitals.spo2}%")
            
        # Check Heart Rate
        if (self.vitals.heart_rate < self.HEART_RATE_CRITICAL_MIN or 
            self.vitals.heart_rate > self.HEART_RATE_CRITICAL_MAX):
            critical_flags += 1
            print(f"❌ CRITICAL: Heart rate abnormal: {self.vitals.heart_rate} BPM")
        elif (self.vitals.heart_rate < self.HEART_RATE_MIN or 
              self.vitals.heart_rate > self.HEART_RATE_MAX):
            warning_flags += 1
            print(f"⚠️  WARNING: Heart rate outside normal: {self.vitals.heart_rate} BPM")
            
        # Check Temperature
        if (self.vitals.temperature < self.TEMP_CRITICAL_MIN or 
            self.vitals.temperature > self.TEMP_CRITICAL_MAX):
            critical_flags += 1
            print(f"❌ CRITICAL: Temperature abnormal: {self.vitals.temperature}°C")
        elif (self.vitals.temperature < self.TEMP_MIN_NORMAL or 
              self.vitals.temperature > self.TEMP_MAX_NORMAL):
            warning_flags += 1
            print(f"⚠️  WARNING: Temperature outside normal: {self.vitals.temperature}°C")
            
        # Check for fall
        if self.vitals.fall_detected:
            critical_flags += 1
            print("🚨 EMERGENCY: Fall detected!")
            
        # Determine overall status
        if critical_flags > 0 or self.vitals.fall_detected:
            return HealthStatus.EMERGENCY
        elif warning_flags >= 2:
            return HealthStatus.CRITICAL
        elif warning_flags > 0:
            return HealthStatus.WARNING
        else:
            return HealthStatus.NORMAL
            
    def handle_health_status(self, status):
        """Handle different health status scenarios"""
        if status == HealthStatus.NORMAL:
            print("✅ Health Status: NORMAL")
            self.monitoring_interval = self.NORMAL_INTERVAL
            self.anomaly_count = 0
            self.emergency_sent = False
            
        elif status == HealthStatus.WARNING:
            print("⚠️  Health Status: WARNING")
            self.anomaly_count += 1
            
            if self.anomaly_count >= 2:
                self.monitoring_interval = self.EXTENDED_INTERVAL
                self.current_state = SystemState.EXTENDED_MONITORING
                print("📊 Switching to EXTENDED monitoring mode")
                
        elif status == HealthStatus.CRITICAL:
            print("🔴 Health Status: CRITICAL")
            self.monitoring_interval = self.EXTENDED_INTERVAL
            self.current_state = SystemState.EXTENDED_MONITORING
            self.transmit_data(is_emergency=False)
            
        elif status == HealthStatus.EMERGENCY:
            print("🚨 Health Status: EMERGENCY")
            self.monitoring_interval = self.EMERGENCY_INTERVAL
            self.current_state = SystemState.EMERGENCY
            
            if not self.emergency_sent:
                self.transmit_data(is_emergency=True)
                self.emergency_sent = True
                
    def transmit_data(self, is_emergency=False):
        """Transmit data to gateway"""
        print("\n" + "="*60)
        if is_emergency:
            print("🚨🚨🚨 EMERGENCY TRANSMISSION TO SURFACE 🚨🚨🚨")
        else:
            print("📡 Transmitting data to gateway...")
            
        print(f"Timestamp: {self.vitals.timestamp}")
        print(f"SpO2: {self.vitals.spo2}%")
        print(f"Heart Rate: {self.vitals.heart_rate} BPM")
        print(f"Blood Pressure: {self.vitals.bp_systolic}/{self.vitals.bp_diastolic} mmHg")
        print(f"Temperature: {self.vitals.temperature}°C")
        print(f"Fall Detected: {self.vitals.fall_detected}")
        print(f"Health Status: {self.health_status.name}")
        print("="*60 + "\n")
        
    def log_vitals(self):
        """Log vital signs"""
        print("\n" + "─"*60)
        print("📊 VITAL SIGNS MEASUREMENT")
        print("─"*60)
        print(f"⏰ Time: {self.vitals.timestamp.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"💓 Heart Rate: {self.vitals.heart_rate} BPM")
        print(f"🫁 SpO2: {self.vitals.spo2}%")
        print(f"🩺 BP: {self.vitals.bp_systolic}/{self.vitals.bp_diastolic} mmHg")
        print(f"🌡️  Temperature: {self.vitals.temperature}°C")
        print(f"📍 Accel: X={self.vitals.accel_x:.2f} Y={self.vitals.accel_y:.2f} Z={self.vitals.accel_z:.2f} g")
        print(f"⚠️  Fall: {self.vitals.fall_detected}")
        print("─"*60 + "\n")
        
    def run(self, duration_minutes=5):
        """Run simulation for specified duration"""
        print("\n" + "="*60)
        print("🏭 MINER HEALTH MONITORING SYSTEM - SIMULATION")
        print("="*60)
        print(f"⏱️  Monitoring Interval: {self.NORMAL_INTERVAL} seconds")
        print(f"⏱️  Extended Interval: {self.EXTENDED_INTERVAL} seconds")
        print(f"⏱️  Emergency Interval: {self.EMERGENCY_INTERVAL} seconds")
        print(f"🕐 Simulation Duration: {duration_minutes} minutes")
        print("="*60 + "\n")
        
        start_time = time.time()
        end_time = start_time + (duration_minutes * 60)
        
        cycle_count = 0
        
        while time.time() < end_time:
            cycle_count += 1
            print(f"\n{'='*60}")
            print(f"🔄 MONITORING CYCLE #{cycle_count}")
            print(f"{'='*60}")
            
            # Sleep phase
            print(f"😴 Entering sleep mode for {self.monitoring_interval} seconds...")
            print(f"💾 Sensors powered OFF to conserve battery")
            time.sleep(min(self.monitoring_interval, 2))  # Shortened for demo
            
            # Wake phase
            print(f"\n⏰ Waking up system...")
            print(f"⚡ Powering ON sensors...")
            time.sleep(0.5)  # Sensor warmup
            
            # Monitoring phase
            print(f"📊 Measuring vitals...")
            self.simulate_sensor_readings()
            self.log_vitals()
            
            # Analysis
            self.health_status = self.analyze_health()
            self.handle_health_status(self.health_status)
            
            # Power down
            print(f"💾 Powering OFF sensors...")
            
            # Check if emergency state requires immediate next measurement
            if self.current_state == SystemState.EMERGENCY:
                print(f"⚠️  EMERGENCY MODE: Next check in {self.monitoring_interval}s")
            elif self.current_state == SystemState.EXTENDED_MONITORING:
                print(f"⚠️  EXTENDED MONITORING: Next check in {self.monitoring_interval}s")
            else:
                print(f"✅ NORMAL MODE: Next check in {self.monitoring_interval}s")
                self.current_state = SystemState.SLEEP
                
        print("\n" + "="*60)
        print("✅ Simulation Complete")
        print(f"📊 Total Monitoring Cycles: {cycle_count}")
        print("="*60 + "\n")

if __name__ == "__main__":
    monitor = MinerHealthMonitor()
    monitor.run(duration_minutes=5)  # Run 5-minute simulation