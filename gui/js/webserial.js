class SerialConnection {
    constructor() {
        this.port = null;
        this.reader = null;
        this.writer = null;
        this.isConnected = false;
        this.decoder = new TextDecoder();
        this.encoder = new TextEncoder();
        
        // Parameter tracking
        this.parameters = {
            pulseWidth: '--',
            peakTime: '--',
            holdDuty: '--',
            holdFreq: '--',
            pot0: '---',
            pot1: '---',
            pot2: '---',
            pot3: '---'
        };
        
        this.initializeUI();
    }

    initializeUI() {
        this.connectBtn = document.getElementById('connectBtn');
        this.sendBtn = document.getElementById('sendBtn');
        this.commandInput = document.getElementById('commandInput');
        this.consoleElement = document.getElementById('console');
        this.connectionStatus = document.getElementById('connectionStatus');
        this.pulseWidthInput = document.getElementById('pulseWidthInput');
        this.setPulseWidthBtn = document.getElementById('setPulseWidthBtn');

        this.connectBtn.addEventListener('click', () => this.toggleConnection());
        this.sendBtn.addEventListener('click', () => this.sendCustomCommand());
        this.setPulseWidthBtn.addEventListener('click', () => this.setPulseWidth());
        
        // Add event listeners for all injector buttons
        document.querySelectorAll('.injector-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const cmd = e.target.getAttribute('data-cmd');
                this.sendCommand(cmd);
            });
        });
        
        // Add event listeners for all config buttons
        document.querySelectorAll('.config-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const cmd = e.target.getAttribute('data-cmd');
                this.sendCommand(cmd);
            });
        });
        
        this.commandInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.sendCustomCommand();
            }
        });

        if (!('serial' in navigator)) {
            this.logToConsole('WebSerial API not supported in this browser', 'error');
            this.connectBtn.disabled = true;
        }
    }

    async toggleConnection() {
        if (this.isConnected) {
            await this.disconnect();
        } else {
            await this.connect();
        }
    }

    async connect() {
        try {
            this.port = await navigator.serial.requestPort();
            await this.port.open({ baudRate: 115200 });

            this.writer = this.port.writable.getWriter();
            
            this.isConnected = true;
            this.updateConnectionStatus(true);
            this.logToConsole('Connected to device', 'system');

            this.readLoop();
        } catch (error) {
            this.logToConsole(`Connection failed: ${error.message}`, 'error');
        }
    }

    async disconnect() {
        try {
            if (this.reader) {
                await this.reader.cancel();
                this.reader = null;
            }
            if (this.writer) {
                await this.writer.close();
                this.writer = null;
            }
            if (this.port) {
                await this.port.close();
                this.port = null;
            }
            
            this.isConnected = false;
            this.updateConnectionStatus(false);
            this.logToConsole('Disconnected from device', 'system');
        } catch (error) {
            this.logToConsole(`Disconnect error: ${error.message}`, 'error');
        }
    }

    async readLoop() {
        try {
            while (this.port.readable && this.isConnected) {
                this.reader = this.port.readable.getReader();
                
                while (true) {
                    const { value, done } = await this.reader.read();
                    if (done) break;
                    
                    const text = this.decoder.decode(value);
                    const lines = text.split('\n').filter(line => line.trim());
                    
                    lines.forEach(line => {
                        this.parseMessage(line);
                    });
                }
            }
        } catch (error) {
            this.logToConsole(`Read error: ${error.message}`, 'error');
        } finally {
            if (this.reader) {
                this.reader.releaseLock();
            }
        }
    }

    async sendCommand(command) {
        if (!this.isConnected || !this.writer) {
            this.logToConsole('Not connected to device', 'error');
            return;
        }

        try {
            const data = this.encoder.encode(command + '\n');
            await this.writer.write(data);
            this.logToConsole(`> ${command}`, 'sent');
        } catch (error) {
            this.logToConsole(`Send error: ${error.message}`, 'error');
        }
    }

    sendCustomCommand() {
        const command = this.commandInput.value.trim();
        if (command) {
            this.sendCommand(command);
            this.commandInput.value = '';
        }
    }

    updateConnectionStatus(connected) {
        if (connected) {
            this.connectionStatus.textContent = 'Connected';
            this.connectionStatus.className = 'status-indicator connected';
            this.connectBtn.textContent = 'Disconnect';
            
            // Enable all control buttons
            document.querySelectorAll('.injector-btn, .config-btn').forEach(btn => {
                btn.disabled = false;
            });
            this.sendBtn.disabled = false;
            this.commandInput.disabled = false;
            this.pulseWidthInput.disabled = false;
            this.setPulseWidthBtn.disabled = false;
            
            // Request initial status
            setTimeout(() => this.sendCommand('i'), 500);
        } else {
            this.connectionStatus.textContent = 'Disconnected';
            this.connectionStatus.className = 'status-indicator disconnected';
            this.connectBtn.textContent = 'Connect to Device';
            
            // Disable all control buttons
            document.querySelectorAll('.injector-btn, .config-btn').forEach(btn => {
                btn.disabled = true;
            });
            this.sendBtn.disabled = true;
            this.commandInput.disabled = true;
            this.pulseWidthInput.disabled = true;
            this.setPulseWidthBtn.disabled = true;
        }
    }

    logToConsole(message, type = 'received') {
        const line = document.createElement('div');
        line.className = `console-line ${type}`;
        line.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        this.consoleElement.appendChild(line);
        this.consoleElement.scrollTop = this.consoleElement.scrollHeight;
    }
    
    parseMessage(line) {
        // Check for structured messages
        if (line.startsWith('[STATUS]')) {
            this.parseStatusMessage(line.substring(8));
        } else if (line.startsWith('[RESULT]')) {
            this.parseResultMessage(line.substring(8));
        } else if (line.startsWith('[ERROR]')) {
            this.logToConsole(line.substring(7), 'error');
        } else if (line.startsWith('[LOG]')) {
            this.logToConsole(line.substring(5), 'received');
        } else {
            // Regular message
            this.logToConsole(line, 'received');
            this.parseParameterData(line);
        }
    }
    
    parseStatusMessage(jsonStr) {
        try {
            const status = JSON.parse(jsonStr);
            this.parameters.pulseWidth = status.pulseWidth + ' ms';
            this.parameters.peakTime = status.peakTime + ' ms';
            this.parameters.holdFreq = status.holdFreq + ' Hz';
            this.parameters.holdDuty = status.holdDuty + ' %';
            this.pulseWidthInput.value = status.pulseWidth;
            
            // Update SD status
            const sdStatus = status.sdAvailable ? 
                (status.logging ? 'LOGGING' : 'READY') : 'NOT AVAILABLE';
            this.logToConsole(`Status: Pulse=${status.pulseWidth}ms, SD=${sdStatus}`, 'system');
            
            this.updateParameterDisplay();
        } catch (e) {
            console.error('Failed to parse status message:', e);
        }
    }
    
    parseResultMessage(jsonStr) {
        try {
            const result = JSON.parse(jsonStr);
            const injNum = result.injector;
            document.getElementById(`peak${injNum}`).textContent = result.peakCurrent.toFixed(2);
            document.getElementById(`avg${injNum}`).textContent = result.avgCurrent.toFixed(2);
            
            const mode = result.peakHold ? 'P&H' : 'Normal';
            this.logToConsole(`Injector ${injNum} (${mode}): Peak=${result.peakCurrent.toFixed(2)}A, Avg=${result.avgCurrent.toFixed(2)}A`, 'result');
        } catch (e) {
            console.error('Failed to parse result message:', e);
        }
    }
    
    setPulseWidth() {
        const value = parseFloat(this.pulseWidthInput.value);
        if (value >= 0.1 && value <= 100) {
            this.sendCommand('p');
            setTimeout(() => {
                this.sendCommand(value.toString());
            }, 100);
        } else {
            this.logToConsole('Invalid pulse width. Must be between 0.1 and 100 ms', 'error');
        }
    }
    
    parseParameterData(line) {
        // Parse initial values from help output
        if (line.includes('Current pulse width:')) {
            const match = line.match(/Current pulse width: ([\d.]+) ms/);
            if (match) {
                this.parameters.pulseWidth = match[1] + ' ms';
                this.pulseWidthInput.value = match[1];
                this.updateParameterDisplay();
            }
        } else if (line.includes('Peak time:')) {
            const match = line.match(/Peak time: ([\d.]+) ms/);
            if (match) {
                this.parameters.peakTime = match[1] + ' ms';
                this.updateParameterDisplay();
            }
        } else if (line.includes('Hold frequency:')) {
            const match = line.match(/Hold frequency: ([\d.]+) Hz/);
            if (match) {
                this.parameters.holdFreq = Math.round(parseFloat(match[1])) + ' Hz';
                this.updateParameterDisplay();
            }
        }
    }
    
    updateParameterDisplay() {
        document.getElementById('pulseWidth').textContent = this.parameters.pulseWidth;
        document.getElementById('peakTime').textContent = this.parameters.peakTime;
        document.getElementById('holdDuty').textContent = this.parameters.holdDuty;
        document.getElementById('holdFreq').textContent = this.parameters.holdFreq;
        document.getElementById('pot0').textContent = this.parameters.pot0;
        document.getElementById('pot1').textContent = this.parameters.pot1;
        document.getElementById('pot2').textContent = this.parameters.pot2;
        document.getElementById('pot3').textContent = this.parameters.pot3;
    }
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    new SerialConnection();
});