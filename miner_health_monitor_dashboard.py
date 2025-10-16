"""
Miner Health Monitoring System - GRAPHICAL DASHBOARD
Real-time visualization with vitals, battery, sensor efficiency, and thermal monitoring
"""

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec
import numpy as np
import time
import random
from datetime import datetime
from collections import deque
from enum import Enum

# Enable interactive mode
plt.ion()

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

class MinerDashboard:
    def __init__(self, max_data_points=100):
        self.max_points = max_data_points
        
        # Data buffers for real-time plotting
        self.time_data = deque(maxlen=max_data_points)
        self.heart_rate_data = deque(maxlen=max_data_points)
        self.spo2_data = deque(maxlen=max_data_points)
        self.temperature_data = deque(maxlen=max_data_points)
        self.battery_data = deque(maxlen=max_data_points)
        
        # System parameters
        self.battery_level = 100.0  # Start at 100%
        self.measurement_count = 0
        self.health_status = HealthStatus.NORMAL
        self.power_mode = "SLEEP"
        self.last_transmission = "Never"
        
        # Sensor specifications (from your component table)
        self.sensors = {
            'MAX30102': {'power_mw': 15.0, 'efficiency': 95, 'heat_mw': 2.5},
            'ADS1292R': {'power_mw': 12.0, 'efficiency': 92, 'heat_mw': 3.0},
            'TMP117': {'power_mw': 0.15, 'efficiency': 98, 'heat_mw': 0.05},
            'ICM42688': {'power_mw': 8.0, 'efficiency': 94, 'heat_mw': 1.8},
            'nRF52840': {'power_mw': 25.0, 'efficiency': 88, 'heat_mw': 4.5}
        }
        
        self.setup_dashboard()
    
    def setup_dashboard(self):
        """Create the dashboard layout"""
        # Create figure with dark theme for better visibility
        plt.style.use('dark_background')
        self.fig = plt.figure(figsize=(16, 10))
        self.fig.suptitle('🏭 MINER HEALTH MONITORING SYSTEM - REAL-TIME DASHBOARD', 
                         fontsize=16, fontweight='bold', color='cyan')
        
        # Create grid layout
        gs = GridSpec(4, 3, figure=self.fig, hspace=0.4, wspace=0.3)
        
        # Heart Rate Plot (top left)
        self.ax_hr = self.fig.add_subplot(gs[0, 0])
        self.ax_hr.set_title('💓 HEART RATE', fontweight='bold', color='red')
        self.ax_hr.set_ylabel('BPM')
        self.ax_hr.set_ylim(40, 160)
        self.ax_hr.grid(True, alpha=0.3)
        self.line_hr, = self.ax_hr.plot([], [], 'r-', linewidth=2, label='Heart Rate')
        self.ax_hr.axhspan(45, 120, alpha=0.2, color='green', label='Normal Range')
        self.ax_hr.legend(loc='upper right', fontsize=8)
        
        # SpO2 Plot (top middle)
        self.ax_spo2 = self.fig.add_subplot(gs[0, 1])
        self.ax_spo2.set_title('🫁 BLOOD OXYGEN (SpO2)', fontweight='bold', color='cyan')
        self.ax_spo2.set_ylabel('%')
        self.ax_spo2.set_ylim(80, 100)
        self.ax_spo2.grid(True, alpha=0.3)
        self.line_spo2, = self.ax_spo2.plot([], [], 'c-', linewidth=2, label='SpO2')
        self.ax_spo2.axhspan(92, 100, alpha=0.2, color='green', label='Normal Range')
        self.ax_spo2.legend(loc='upper right', fontsize=8)
        
        # Temperature Plot (top right)
        self.ax_temp = self.fig.add_subplot(gs[0, 2])
        self.ax_temp.set_title('🌡️ BODY TEMPERATURE', fontweight='bold', color='orange')
        self.ax_temp.set_ylabel('°C')
        self.ax_temp.set_ylim(34, 41)
        self.ax_temp.grid(True, alpha=0.3)
        self.line_temp, = self.ax_temp.plot([], [], 'orange', linewidth=2, label='Temperature')
        self.ax_temp.axhspan(35.5, 38.5, alpha=0.2, color='green', label='Normal Range')
        self.ax_temp.legend(loc='upper right', fontsize=8)
        
        # Battery Level (middle left)
        self.ax_battery = self.fig.add_subplot(gs[1, 0])
        self.ax_battery.set_title('🔋 BATTERY LEVEL', fontweight='bold', color='lime')
        self.ax_battery.set_ylabel('%')
        self.ax_battery.set_ylim(0, 100)
        self.ax_battery.grid(True, alpha=0.3)
        self.line_battery, = self.ax_battery.plot([], [], 'lime', linewidth=3, label='Battery')
        self.ax_battery.axhspan(0, 20, alpha=0.3, color='red', label='Critical')
        self.ax_battery.axhspan(20, 50, alpha=0.3, color='yellow', label='Low')
        self.ax_battery.legend(loc='upper right', fontsize=8)
        
        # Sensor Power Consumption (middle center/right)
        self.ax_power = self.fig.add_subplot(gs[1, 1:])
        self.ax_power.set_title('⚡ SENSOR POWER CONSUMPTION', fontweight='bold', color='yellow')
        self.ax_power.set_ylabel('Power (mW)')
        self.ax_power.grid(True, alpha=0.3, axis='y')
        
        # Sensor Efficiency (bottom left)
        self.ax_efficiency = self.fig.add_subplot(gs[2, 0])
        self.ax_efficiency.set_title('📊 SENSOR EFFICIENCY', fontweight='bold', color='lightblue')
        self.ax_efficiency.set_ylabel('Efficiency (%)')
        self.ax_efficiency.set_ylim(80, 100)
        self.ax_efficiency.grid(True, alpha=0.3, axis='y')
        
        # Heat Generation (bottom center)
        self.ax_heat = self.fig.add_subplot(gs[2, 1])
        self.ax_heat.set_title('🔥 THERMAL OUTPUT', fontweight='bold', color='red')
        self.ax_heat.set_ylabel('Heat (mW)')
        self.ax_heat.grid(True, alpha=0.3, axis='y')
        
        # Power Management Timeline (bottom right)
        self.ax_power_mode = self.fig.add_subplot(gs[2, 2])
        self.ax_power_mode.set_title('🔌 POWER MODE HISTORY', fontweight='bold', color='magenta')
        self.ax_power_mode.set_ylabel('Mode')
        self.ax_power_mode.set_ylim(-0.5, 1.5)
        self.ax_power_mode.grid(True, alpha=0.3)
        self.power_mode_data = deque(maxlen=self.max_points)
        self.line_power_mode, = self.ax_power_mode.plot([], [], 'm-', linewidth=2, marker='o', markersize=4)
        self.ax_power_mode.set_yticks([0, 1])
        self.ax_power_mode.set_yticklabels(['SLEEP', 'ACTIVE'])
        
        # System Status Panel (bottom full width)
        self.ax_status = self.fig.add_subplot(gs[3, :])
        self.ax_status.axis('off')
        self.status_text = self.ax_status.text(0.5, 0.5, '', 
                                               ha='center', va='center',
                                               fontsize=12, fontweight='bold',
                                               bbox=dict(boxstyle='round', facecolor='black', alpha=0.8))
        
        plt.tight_layout()
    
    def update_sensor_bars(self):
        """Update sensor power, efficiency, and heat bars"""
        sensor_names = list(self.sensors.keys())
        
        # Power consumption bars
        self.ax_power.clear()
        self.ax_power.set_title('⚡ SENSOR POWER CONSUMPTION', fontweight='bold', color='yellow')
        self.ax_power.set_ylabel('Power (mW)')
        self.ax_power.grid(True, alpha=0.3, axis='y')
        
        powers = [self.sensors[s]['power_mw'] for s in sensor_names]
        colors_power = ['red', 'orange', 'green', 'cyan', 'magenta']
        bars_power = self.ax_power.bar(sensor_names, powers, color=colors_power, alpha=0.7)
        self.ax_power.set_xticklabels(sensor_names, rotation=45, ha='right', fontsize=8)
        
        # Add value labels on bars
        for bar, power in zip(bars_power, powers):
            height = bar.get_height()
            self.ax_power.text(bar.get_x() + bar.get_width()/2., height,
                             f'{power:.1f}mW',
                             ha='center', va='bottom', fontsize=8)
        
        # Efficiency bars
        self.ax_efficiency.clear()
        self.ax_efficiency.set_title('📊 SENSOR EFFICIENCY', fontweight='bold', color='lightblue')
        self.ax_efficiency.set_ylabel('Efficiency (%)')
        self.ax_efficiency.set_ylim(80, 100)
        self.ax_efficiency.grid(True, alpha=0.3, axis='y')
        
        efficiencies = [self.sensors[s]['efficiency'] for s in sensor_names]
        bars_eff = self.ax_efficiency.bar(sensor_names, efficiencies, color='lightblue', alpha=0.7)
        self.ax_efficiency.set_xticklabels(sensor_names, rotation=45, ha='right', fontsize=8)
        
        for bar, eff in zip(bars_eff, efficiencies):
            height = bar.get_height()
            self.ax_efficiency.text(bar.get_x() + bar.get_width()/2., height,
                                   f'{eff}%',
                                   ha='center', va='bottom', fontsize=8)
        
        # Heat generation bars
        self.ax_heat.clear()
        self.ax_heat.set_title('🔥 THERMAL OUTPUT', fontweight='bold', color='red')
        self.ax_heat.set_ylabel('Heat (mW)')
        self.ax_heat.grid(True, alpha=0.3, axis='y')
        
        heats = [self.sensors[s]['heat_mw'] for s in sensor_names]
        bars_heat = self.ax_heat.bar(sensor_names, heats, color='red', alpha=0.7)
        self.ax_heat.set_xticklabels(sensor_names, rotation=45, ha='right', fontsize=8)
        
        for bar, heat in zip(bars_heat, heats):
            height = bar.get_height()
            self.ax_heat.text(bar.get_x() + bar.get_width()/2., height,
                            f'{heat:.2f}mW',
                            ha='center', va='bottom', fontsize=8)
    
    def update_status_panel(self):
        """Update system status information"""
        status_colors = {
            HealthStatus.NORMAL: 'lime',
            HealthStatus.WARNING: 'yellow',
            HealthStatus.CRITICAL: 'orange',
            HealthStatus.EMERGENCY: 'red'
        }
        
        status_text = f"""
        ⚡ SYSTEM STATUS: {self.health_status.name} | 🔌 Power Mode: {self.power_mode} | 📡 Last Transmission: {self.last_transmission}
        🔋 Battery: {self.battery_level:.1f}% | 📊 Measurements: {self.measurement_count} | 
        ⚙️ Total Power: {sum(s['power_mw'] for s in self.sensors.values()):.1f}mW | 🔥 Total Heat: {sum(s['heat_mw'] for s in self.sensors.values()):.2f}mW
        """
        
        self.status_text.set_text(status_text)
        self.status_text.set_color(status_colors[self.health_status])
    
    def add_measurement(self, heart_rate, spo2, temperature, power_active=False):
        """Add new measurement data"""
        current_time = len(self.time_data)
        
        self.time_data.append(current_time)
        self.heart_rate_data.append(heart_rate)
        self.spo2_data.append(spo2)
        self.temperature_data.append(temperature)
        
        # Update battery (discharge based on power mode)
        if power_active:
            # Active mode: consuming power
            discharge_rate = 0.08  # ~0.08% per active measurement
            self.power_mode = "ACTIVE"
        else:
            # Sleep mode: minimal discharge
            discharge_rate = 0.02  # ~0.02% per sleep cycle
            self.power_mode = "SLEEP"
        
        self.battery_level = max(0, self.battery_level - discharge_rate)
        self.battery_data.append(self.battery_level)
        
        # Power mode tracking (1 = active, 0 = sleep)
        self.power_mode_data.append(1 if power_active else 0)
        
        self.measurement_count += 1
        
        # Update plots
        self.update_plots()
    
    def update_plots(self):
        """Update all real-time plots"""
        time_array = list(self.time_data)
        
        # Update heart rate
        self.line_hr.set_data(time_array, list(self.heart_rate_data))
        self.ax_hr.set_xlim(max(0, len(time_array) - 50), max(50, len(time_array)))
        
        # Update SpO2
        self.line_spo2.set_data(time_array, list(self.spo2_data))
        self.ax_spo2.set_xlim(max(0, len(time_array) - 50), max(50, len(time_array)))
        
        # Update temperature
        self.line_temp.set_data(time_array, list(self.temperature_data))
        self.ax_temp.set_xlim(max(0, len(time_array) - 50), max(50, len(time_array)))
        
        # Update battery
        self.line_battery.set_data(time_array, list(self.battery_data))
        self.ax_battery.set_xlim(max(0, len(time_array) - 50), max(50, len(time_array)))
        
        # Update power mode
        self.line_power_mode.set_data(time_array, list(self.power_mode_data))
        self.ax_power_mode.set_xlim(max(0, len(time_array) - 50), max(50, len(time_array)))
        
        # Update sensor bars
        self.update_sensor_bars()
        
        # Update status panel
        self.update_status_panel()
        
        # Refresh display
        plt.pause(0.01)
    
    def set_health_status(self, status):
        """Set current health status"""
        self.health_status = status
        if status in [HealthStatus.CRITICAL, HealthStatus.EMERGENCY]:
            self.last_transmission = datetime.now().strftime('%H:%M:%S')

class MinerHealthMonitorGraphical:
    """Enhanced monitor with graphical dashboard"""
    
    # Thresholds
    SPO2_MIN_NORMAL = 92
    SPO2_MIN_CRITICAL = 85
    HEART_RATE_MIN = 45
    HEART_RATE_MAX = 120
    TEMP_MIN_NORMAL = 35.5
    TEMP_MAX_NORMAL = 38.5
    
    def __init__(self):
        self.dashboard = MinerDashboard()
        self.cycle_count = 0
        
    def simulate_vitals(self):
        """Generate realistic vital signs"""
        self.cycle_count += 1
        
        # Normal values with variation
        spo2 = random.randint(95, 99)
        heart_rate = random.randint(70, 90)
        temperature = round(random.uniform(36.2, 37.2), 2)
        
        # Simulate occasional anomalies
        if self.cycle_count % 8 == 0:
            spo2 = random.randint(87, 91)
            print("⚠️  Simulating low SpO2")
            
        if self.cycle_count % 12 == 0:
            heart_rate = random.randint(122, 135)
            print("⚠️  Simulating high heart rate")
            
        if self.cycle_count % 15 == 0:
            temperature = round(random.uniform(38.6, 39.2), 2)
            print("⚠️  Simulating elevated temperature")
        
        return heart_rate, spo2, temperature
    
    def analyze_health(self, heart_rate, spo2, temperature):
        """Analyze health status"""
        warning_flags = 0
        critical_flags = 0
        
        if spo2 < self.SPO2_MIN_CRITICAL:
            critical_flags += 1
        elif spo2 < self.SPO2_MIN_NORMAL:
            warning_flags += 1
            
        if heart_rate < 40 or heart_rate > 150:
            critical_flags += 1
        elif heart_rate < self.HEART_RATE_MIN or heart_rate > self.HEART_RATE_MAX:
            warning_flags += 1
            
        if temperature < 35.0 or temperature > 40.0:
            critical_flags += 1
        elif temperature < self.TEMP_MIN_NORMAL or temperature > self.TEMP_MAX_NORMAL:
            warning_flags += 1
        
        if critical_flags > 0:
            return HealthStatus.EMERGENCY
        elif warning_flags >= 2:
            return HealthStatus.CRITICAL
        elif warning_flags > 0:
            return HealthStatus.WARNING
        else:
            return HealthStatus.NORMAL
    
    def run(self, duration_minutes=5, speed_factor=1.0):
        """Run simulation with graphical dashboard"""
        print("\n" + "="*60)
        print("🏭 MINER HEALTH MONITORING - GRAPHICAL DASHBOARD")
        print("="*60)
        print(f"⏱️  Duration: {duration_minutes} minutes")
        print(f"🚀 Speed Factor: {speed_factor}x")
        print("📊 Dashboard window opening...")
        print("="*60 + "\n")
        
        total_cycles = int((duration_minutes * 60) / 5)  # One cycle every 5 seconds (simulated)
        
        for cycle in range(total_cycles):
            # Simulate sleep phase
            self.dashboard.add_measurement(70, 96, 36.5, power_active=False)
            time.sleep(0.5 / speed_factor)
            
            # Simulate active measurement phase
            heart_rate, spo2, temperature = self.simulate_vitals()
            health_status = self.analyze_health(heart_rate, spo2, temperature)
            
            self.dashboard.set_health_status(health_status)
            self.dashboard.add_measurement(heart_rate, spo2, temperature, power_active=True)
            
            print(f"Cycle {cycle + 1}/{total_cycles}: HR={heart_rate} BPM, SpO2={spo2}%, Temp={temperature}°C, Status={health_status.name}")
            
            time.sleep(1.0 / speed_factor)
        
        print("\n" + "="*60)
        print("✅ Simulation Complete! Dashboard will remain open.")
        print("Close the window to exit.")
        print("="*60)
        
        plt.show(block=True)

if __name__ == "__main__":
    monitor = MinerHealthMonitorGraphical()
    monitor.run(duration_minutes=5, speed_factor=2.0)  # 2x speed for demo