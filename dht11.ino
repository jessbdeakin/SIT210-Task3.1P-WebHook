
#define PULSE_TIMEOUT 0xFFFFFFFF

int sensor = D1;
int led = D0;

struct SensorReading {
    unsigned char humidityIntegral, humidityDecimal;
    unsigned char temperatureIntegral, temperatureDecimal;
    unsigned char checksum;
};

struct SensorReading reading = SensorReading { 0, 0 };

void setup() {
    
    pinMode(led, OUTPUT);
    pinMode(sensor, INPUT_PULLUP);

}


unsigned long pulseInSafe(int pin, int value){
    
    unsigned long cycles = 0;
    
    // the pulse isnt happening yet, wait
    if(pinReadFast(pin) != value) 
        pulseInSafe(pin, !value);
    
    while(pinReadFast(pin) == value){
        if(cycles++ > 10000) {
            return PULSE_TIMEOUT;
        }
    }
    
    return cycles;
    
}

void pullUp(int pin, int t){
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(t);
}

void pullDown(int pin, int t){
    pinMode(pin, OUTPUT);
    digitalWriteFast(pin, LOW);
    delayMicroseconds(t);
}

void sendStartSignal(){
    pullUp(sensor, 10);
    pullDown(sensor, 20000); // just over 18 milliseconds to be safe
    pullUp(sensor, 40);
}

bool receiveResponseSignal(){
    bool status = true;
    status = status && (pulseInSafe(sensor, LOW) != PULSE_TIMEOUT);
    status = status && (pulseInSafe(sensor, HIGH) != PULSE_TIMEOUT);
    return status;
}

bool readSensor(){
    
    sendStartSignal();
    if(!receiveResponseSignal()){
        return false;
    }
    
    unsigned char bytes[5] = {0, 0, 0, 0, 0};
    
    noInterrupts();
    for(unsigned int i = 0; i < 40; i++){
        unsigned long lowPulseWidth = pulseInSafe(sensor, LOW);
        unsigned long highPulseWidth = pulseInSafe(sensor, HIGH);
        
        if(lowPulseWidth == PULSE_TIMEOUT || highPulseWidth == PULSE_TIMEOUT) 
            return false;
            
        bytes[ i / 8 ] <<= 1;
        if(highPulseWidth > lowPulseWidth) 
            bytes[ i / 8 ] |= 1;
    }
    interrupts();
    
    reading = SensorReading {
        bytes[0], bytes[1],
        bytes[2], bytes[3],
        bytes[4]
    };
    
    return true;
}


bool checkReadingIntegry(struct SensorReading reading){
    return reading.checksum == (
        (reading.humidityIntegral 
            + reading.humidityDecimal 
            + reading.temperatureIntegral 
            + reading.temperatureDecimal
        ) % 0xFF
    );
}

double partsToFloatingPoint(unsigned char integral, unsigned char decimal){
    return double(integral) + double(decimal) * 0.1;
}

void loop() {
    
    if(readSensor()){
        if(checkReadingIntegry(reading)){
            
            Particle.publish("jess", String::format(
                "{\"humidity\": %f, \"temperature\": %f}",
                partsToFloatingPoint(reading.humidityIntegral, reading.humidityDecimal),
                partsToFloatingPoint(reading.temperatureIntegral, reading.temperatureDecimal)
            ));

        }
    }
    delay(3000);
}