
#define PULSE_TIMEOUT 0xFFFFFFFF

int sensor = D1;

struct SensorReading {
    unsigned char humidityIntegral, humidityDecimal;
    unsigned char temperatureIntegral, temperatureDecimal;
    unsigned char checksum;
};

struct SensorReading reading = SensorReading { 0, 0 };

void setup() {
    
    pinMode(sensor, INPUT_PULLUP);

}

unsigned long readPulse(int pin, int value){
    
    unsigned long cycles = 0;
    
    // the pulse isnt happening yet, so just wait
    if(pinReadFast(pin) != value) 
        readPulse(pin, !value);
    
    // this code needs to be fast and we dont need to know the absolute length of a pulse,
    // we just need to compare the high pulse in a bit to the low pulse
    // so hopefully the varying runtimes of these loop cycles average out
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
    status = status && (readPulse(sensor, LOW) != PULSE_TIMEOUT);
    status = status && (readPulse(sensor, HIGH) != PULSE_TIMEOUT);
    return status;
}

bool readSensorBase(){
    
    sendStartSignal();
    if(!receiveResponseSignal()) return false;
    
    unsigned char bytes[5] = {0, 0, 0, 0, 0};

    for(unsigned int i = 0; i < 40; i++){
        unsigned long lowPulseWidth = readPulse(sensor, LOW);
        unsigned long highPulseWidth = readPulse(sensor, HIGH);
        
        if(lowPulseWidth == PULSE_TIMEOUT || highPulseWidth == PULSE_TIMEOUT) 
            return false;
            
        bytes[ i / 8 ] <<= 1;
        if(highPulseWidth > lowPulseWidth) 
            bytes[ i / 8 ] |= 1;
    }
    
    reading = SensorReading {
        bytes[0], bytes[1],
        bytes[2], bytes[3],
        bytes[4]
    };

    return true;
}

// a wrapper to avoid tedious return logic in the real function
bool readSensor(){

    // disabling interrupts = polling the sensor more often
    noInterrupts();
        bool status = readSensorBase();
    interrupts();

    return status;

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
            
            Particle.publish("temphumid_reading", String::format(
                "{\"humidity\": %f, \"temperature\": %f}",
                partsToFloatingPoint(reading.humidityIntegral, reading.humidityDecimal),
                partsToFloatingPoint(reading.temperatureIntegral, reading.temperatureDecimal)
            ));

        }
    }
    delay(3000);
}